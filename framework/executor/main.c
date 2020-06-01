//
// Created by hy on 19-3-16.
//

#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_launch.h>
#include <rte_log.h>
#include <rte_prefetch.h>
#include <rte_branch_prediction.h>

#include "args.h"
#include "mempool.h"
#include "port.h"
#include "worker.h"
#include "tunnel.h"
#include "tag.h"
#include "common.h"
#include "dispatcher.h"
#include "manager.h"

#define MBUF_PER_POOL (65535 * 4)
#define MAX_PKT_BURST 32
#define TIMER_PERIOD 1
#define PEER_ID 1
#define RX_NUM 1
#define TX_NUM 1
#define MANAGER_NUM 1

struct rte_mempool *mbuf_pool;
struct port_info portinfo;
struct deliver_pool_t deliver_pool;
struct deliver_table_t deliver_table;

static void
handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        keep_running = 0;
    }
}

static void
clear_terminal(void) {
    const char clr[] = { 27, '[', '2', 'J', '\0' };
    const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

    printf("%s%s", clr, topLeft);
}

static int
rx_thread(void *arg) {
    uint8_t port_id;
    uint16_t nb_rx;
    struct rte_mbuf *pkts[MAX_PKT_BURST];

    port_id = *(uint8_t *)arg;
    init_dispatcher_buffer();

    RTE_LOG(INFO, DISPATCHER, "Core %d: Running rx worker thread\n", rte_lcore_id());

    for (; keep_running;) {
		// nb_rx = rte_eth_rx_burst(port_id, 0, pkts, MAX_PKT_BURST);
		nb_rx = vxlan_rx_pkts(port_id, 0, pkts, MAX_PKT_BURST);
		if (unlikely(nb_rx == 0))
            continue;

		portinfo.rx_stats[port_id].rx_pkts += nb_rx;

		dispatcher(pkts, nb_rx);
    }
    RTE_LOG(INFO, DISPATCHER, "Core %d: rx worker thread done\n", rte_lcore_id());

    return 0;
}

static int
tx_thread(void *arg) {
    uint8_t port_id;
    uint16_t nb_rx, nb_tx;
    struct rte_mbuf *pkts[MAX_PKT_BURST];

    port_id = *(uint8_t *)arg;

    RTE_LOG(INFO, DISPATCHER, "Core %d: Running tx worker thread\n", rte_lcore_id());

    for (; keep_running;) {
        nb_rx = aggregator(pkts, MAX_PKT_BURST);
        if (unlikely(nb_rx == 0))
            continue;

        nb_tx = vxlan_tx_pkts(port_id, 0, pkts, nb_rx);
        if (unlikely(nb_tx < nb_rx)) {
            pktmbuf_free_bulk(&pkts[nb_tx], nb_rx - nb_tx);
            portinfo.tx_stats[port_id].drop_pkts += (nb_rx - nb_tx);
        }
        portinfo.tx_stats[port_id].tx_pkts += nb_tx;

    }

    RTE_LOG(INFO, DISPATCHER, "Core %d: tx worker thread done\n", rte_lcore_id());

    return 0;
}

static int
master_thread(void) {
    const uint64_t sleeptime = TIMER_PERIOD;

    RTE_LOG(INFO, DISPATCHER, "Core %d: Running master thread\n", rte_lcore_id());

    /* Initial pause so above printf is seen */
    sleep(sleeptime * 3);

    /* Loop forever: sleep always returns 0 or <= param */
    while (keep_running && sleep(sleeptime) <= sleeptime) {
        clear_terminal();
        display_ports(sleeptime, &portinfo);
        printf("\n\n");

        display_deliver_info(sleeptime, &deliver_table, &deliver_pool);
        printf("\n\n");
    }

    RTE_LOG(INFO, DISPATCHER, "Core %d: Master thread done\n", rte_lcore_id());

    return 0;
}

int
main(int argc, char *argv[]) {
    unsigned cur_lcore, manager_lcore, worker_lcores;
    unsigned long portmask;
    uint16_t nb_ports;
    int i, ret;


    /* init the system */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        return -1;

    argc -= ret;
    argv += ret;

    /*---------------------init everything----------------*/
    init_mbuf_pool("exector_mbuf_pool", MBUF_PER_POOL);
    nb_ports = get_nb_ports();
    portmask = get_portmask(argc, argv);
    portinfo = init_ports(nb_ports, portmask, mbuf_pool);
    RTE_LOG(INFO, DISPATCHER, "Finished Process Init.\n");

    cur_lcore = rte_lcore_id();
    manager_lcore = MANAGER_NUM;
    worker_lcores = rte_lcore_count() - manager_lcore - 1;
    if (worker_lcores < (RX_NUM + TX_NUM))
        rte_exit(EXIT_FAILURE, "number of cores is %u, which is not enough for dispatcher\n", worker_lcores);

    vxlan_init(PEER_ID);
    deliver_pool_init(&deliver_pool);
    deliver_table_init(&deliver_table);
//    manager_init();
//    manager_launch_instance(0, "funcbox_0", 0);

    RTE_LOG(INFO, DISPATCHER, "%d cores available in total\n", rte_lcore_count());
    RTE_LOG(INFO, DISPATCHER, "%d cores available for manager\n", manager_lcore);
    RTE_LOG(INFO, DISPATCHER, "%d cores available for worker\n", worker_lcores);
    RTE_LOG(INFO, DISPATCHER, "%d cores available for master\n", 1);

    /* Listen for ^C and docker stop so we can exit gracefully */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /* Launch manager thread on core */
    for (i = 0; i < MANAGER_NUM; i++) {
        cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
        if (rte_eal_remote_launch(manager_thread, NULL,  cur_lcore) == -EBUSY) {
            RTE_LOG(ERR, DISPATCHER, "Core %d is already busy, can't use for manager\n", cur_lcore);
            return -1;
        }
    }

    /* Launch each rx thread on cores */
    for (i = 0; i < RX_NUM; i++) {
        uint8_t *rx_id = calloc(1, sizeof(unsigned));
        *rx_id = (uint8_t)i;
        cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
        if (rte_eal_remote_launch(rx_thread, (void*)rx_id,  cur_lcore) == -EBUSY) {
            RTE_LOG(ERR, DISPATCHER, "Core %d is already busy, can't use for rx %d\n", cur_lcore, *rx_id);
            return -1;
        }
    }

    /* Launch each rx thread on cores */
    for (i = 0; i < TX_NUM; i++) {
        uint8_t *tx_id = calloc(1, sizeof(unsigned));
        *tx_id = (uint8_t)i;
        cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
        if (rte_eal_remote_launch(tx_thread, (void*)tx_id,  cur_lcore) == -EBUSY) {
            RTE_LOG(ERR, DISPATCHER, "Core %d is already busy, can't use for rx %d\n", cur_lcore, *tx_id);
            return -1;
        }
    }

    /* Master thread handles statistics and NF management */
    master_thread();

    return 0;
}
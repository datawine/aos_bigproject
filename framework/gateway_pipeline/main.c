/*
* Created by Zhilong Zheng
*/

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_compat.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_timer.h>
#include <rte_cycles.h>
#include <port.h>

#include "args.h"
#include "mempool.h"
#include "port.h"
#include "util_dpdk.h"
#include "worker.h"
#include "tunnel.h"
#include "tag.h"
#include "gateway.h"

#define MBUF_PER_POOL (65535 * 8)
#define MAX_PKTS_BURST_RX 32
#define MAX_PKTS_BURST_TX 32
#define MAX_WORKER_DEQ_NB 32

#define VLAN_PORT 1
#define PEER_ID 0
#define RTE_LOGTYPE_GATEWAY          RTE_LOGTYPE_USER1

/*-------------- Global variables ---------------*/
volatile uint8_t quit_signal = 0;
struct rte_mempool *mbuf_pool;
struct worker_info workerinfo;
struct port_info portinfo;

struct tx_buffer_t{
    uint16_t num;
    struct rte_mbuf *buffer[MAX_PKTS_BURST_TX];
};

static void
clear_terminal(void) {
    const char clr[] = { 27, '[', '2', 'J', '\0' };
    const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

    printf("%s%s", clr, topLeft);
}

static int
main_thread(void) {
    const unsigned sleeptime = 1;

    RTE_LOG(INFO, GATEWAY, "main thread starts on core %u\n", rte_lcore_id());

    /* Longer initial pause so above printf is seen */
    sleep(sleeptime * 3);

    /* Loop forever: sleep always returns 0 or <= param */
    while (!quit_signal && sleep(sleeptime) <= sleeptime) {
        clear_terminal();
        display_ports(sleeptime, &portinfo);
        printf("\n\n");
        display_workers(sleeptime, &workerinfo);
        printf("\n\n");
    }

    return 0;
}


static int
rx_thread(void *args) {
    uint32_t rx_pkts, tx_pkts;
    uint8_t ports_nb;
    struct rte_mbuf *pkts[MAX_PKTS_BURST_RX];
    uint16_t rx_id = *(uint16_t *)args;
    ports_nb = portinfo.port_nb;

    if (ports_nb <= rx_id)
        rte_exit(EXIT_FAILURE, "number of ports is %u, which is not enough!\n", ports_nb);

    RTE_LOG(INFO, GATEWAY, "%s() started on lcore %u and receive packets on port %u\n", __func__, rte_lcore_id(), rx_id);

    while (!quit_signal) {

        if (rx_id == VLAN_PORT) {
            rx_pkts = vxlan_rx_pkts(rx_id, 0, pkts, MAX_PKTS_BURST_RX);
        } else {
            rx_pkts = rte_eth_rx_burst(rx_id, 0, pkts, MAX_PKTS_BURST_RX);
        }

        if (unlikely(rx_pkts == 0))
            continue;

        portinfo.rx_stats[rx_id].rx_pkts += rx_pkts;

        tx_pkts = rte_ring_sp_enqueue_burst(workers[rx_id].rx_q, (void *)pkts, rx_pkts, NULL);

        portinfo.rx_stats[rx_id].enqueue_pkts += tx_pkts;
        if (unlikely(tx_pkts < rx_pkts)) {
            pktmbuf_free_burst(pkts, tx_pkts, rx_pkts);
            portinfo.rx_stats[rx_id].drop_pkts += rx_pkts - tx_pkts;
        }
    }

    RTE_LOG(DEBUG, GATEWAY, "Rx exited\n");
    return 0;
}

static int
tx_thread(void *args) {
    uint32_t rx_pkts, tx_pkts;
    uint16_t i, port_id;
    uint8_t ports_nb;
    struct rte_mbuf *pkts[MAX_WORKER_DEQ_NB];
    uint16_t tx_id = *(uint16_t *)args;
    ports_nb = portinfo.port_nb;

    struct tx_buffer_t tx_buffer[ports_nb];

    for (i = 0; i < ports_nb; i++) {
        tx_buffer[i].num = 0;
    }

    RTE_LOG(INFO, GATEWAY, "%s() started on lcore %u and tx packets on port %u\n", __func__, rte_lcore_id(), tx_id);

    while (!quit_signal) {

        rx_pkts = rte_ring_sc_dequeue_burst(workers[tx_id].tx_q, (void *)pkts, MAX_WORKER_DEQ_NB, NULL);

        if (unlikely(rx_pkts == 0))
            continue;

        for (i = 0; i < rx_pkts; i++) {
            rte_prefetch0(rte_pktmbuf_mtod(pkts[i], void *));
            port_id = *get_port_tag(pkts[i]);
            tx_buffer[port_id].buffer[tx_buffer[port_id].num++] = pkts[i];
            if (tx_buffer[port_id].num == MAX_PKTS_BURST_TX) {
                if (port_id == VLAN_PORT) {
                    tx_pkts = vxlan_tx_pkts(port_id, tx_id, tx_buffer[port_id].buffer, MAX_PKTS_BURST_TX);
                } else {
                    tx_pkts = rte_eth_tx_burst(port_id, tx_id, tx_buffer[port_id].buffer, MAX_PKTS_BURST_TX);
                }
                tx_buffer[port_id].num = 0;
                portinfo.tx_stats[port_id].tx_pkts += tx_pkts;
                if (unlikely(tx_pkts < MAX_PKTS_BURST_TX)) {
                    portinfo.tx_stats[port_id].drop_pkts += MAX_PKTS_BURST_TX - tx_pkts;
                    pktmbuf_free_burst(tx_buffer[port_id].buffer, tx_pkts, MAX_PKTS_BURST_TX);
                }
            }
        }
    }

    RTE_LOG(DEBUG, GATEWAY, "Tx exited\n");

    return 0;
}

static int
worker_thread(void *args) {
    uint32_t rx_pkts, tx_pkts;
    uint16_t worker_id = *(uint16_t *)args;
    struct rte_mbuf *worker_pkts[MAX_WORKER_DEQ_NB];

    while (!quit_signal) {

        rx_pkts = rte_ring_sc_dequeue_burst(workers[worker_id].rx_q, (void *)worker_pkts, MAX_WORKER_DEQ_NB, NULL);

        if (unlikely(rx_pkts == 0))
            continue;

        workerinfo.rx_stats[worker_id].rx += rx_pkts;

        gateway_classify(worker_pkts, rx_pkts);

        tx_pkts = rte_ring_sp_enqueue_burst(workers[worker_id].tx_q, (void *) worker_pkts, rx_pkts, NULL);
        workerinfo.tx_stats[worker_id].tx += tx_pkts;
        if (unlikely(tx_pkts < rx_pkts)) {
            pktmbuf_free_burst(worker_pkts, tx_pkts, rx_pkts);
            workerinfo.tx_stats[worker_id].tx_drop += rx_pkts - tx_pkts;
        }
    }

    RTE_LOG(DEBUG, GATEWAY, "Worker exited\n");

    return 0;

}



static void
signal_handler(int sig_num)
{
    printf("Exiting on signal %d\n", sig_num);
    quit_signal = 1;
}

int main(int argc, char **argv) {
    unsigned long portmask;
    int ret;
    uint8_t rx_cores, tx_cores, worker_cores, nb_workers;
    uint16_t i, nb_ports;
    uint32_t cur_core;


    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        return -1;

    argc -= ret;
    argv += ret;

    /*---------------------init everything----------------*/
    init_mbuf_pool("gateway_mbuf_pool", MBUF_PER_POOL);
    nb_ports = get_nb_ports();
    portmask = get_portmask(argc, argv);
    portinfo = init_ports(nb_ports, portmask, mbuf_pool);

    /*---------------------init everything ends----------------*/

    /*--------------------- config ----------------*/

    rx_cores = 2;
    tx_cores = 2;
    worker_cores = 2;

    /*--------------------- config ends ----------------*/
    RTE_LOG(INFO, GATEWAY, "Serverless NFV gateway init ends\n");

    /*--------------------start rx/tx threads------------------*/
    /* Check if we have enought cores */
    if (rte_lcore_count() < 3)
        rte_exit(EXIT_FAILURE, "Error, This application needs at "
            "least 3 logical cores to run:\n"
            "1 lcore for packet RX\n"
            "1 lcore for packet TX\n"
            "1 lcore for worker\n");
    cur_core = rte_lcore_id();

    // allocate cores, the number of which equal to nb_ports
    RTE_LOG(INFO, GATEWAY, "Total cores: %u\n", rte_lcore_count());
    RTE_LOG(INFO, GATEWAY, "Allocate %d cores to rx thread and tx threads, respectively\n", nb_ports);
    RTE_LOG(INFO, GATEWAY, "Total worker cores %u\n", worker_cores);

    // mbuf_pool should be invoked
    nb_workers = worker_cores;
    workerinfo = init_workers(nb_workers);

    vxlan_init(PEER_ID);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    for (i = 0; i< nb_workers; ++i) {
        uint16_t *worker_id = calloc(1, sizeof(uint16_t));
        *worker_id = i;
        cur_core = rte_get_next_lcore(cur_core, 1, 1);
        RTE_LOG(INFO, GATEWAY, "Core %u assigned to worker_thread %u\n", cur_core, i);

        rte_eal_remote_launch(worker_thread, (void*)worker_id, cur_core);

    }

    // init tx threads
    for (i = 0; i < tx_cores; ++i) {
        uint16_t *tx_id = calloc(1, sizeof(uint16_t));
        *tx_id = i;
        cur_core = rte_get_next_lcore(cur_core, 1, 1);
        RTE_LOG(INFO, GATEWAY, "Core %u assigned to tx_thread %u\n", cur_core, i);

        rte_eal_remote_launch(tx_thread, (void*)tx_id, cur_core);
    }

    // init rx threas
    for (i = 0; i < rx_cores; ++i) {
        uint16_t *rx_id = calloc(1, sizeof(uint16_t));
        *rx_id = i;
        cur_core = rte_get_next_lcore(cur_core, 1, 1);
        RTE_LOG(INFO, GATEWAY, "Core %u assigned to rx_thread %u\n", cur_core, i);

        rte_eal_remote_launch(rx_thread, (void*)rx_id, cur_core);
    }

    main_thread();

    return 0;
}

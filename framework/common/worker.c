//
// Created by Zhilong Zheng on 2019/3/12.
//

#include <stdint.h>
#include <stdio.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include "mempool.h"
#include "worker.h"

#define WORKER_RXQ_NAME "Worker_%u_RX"
#define WORKER_TXQ_NAME "Worker_%u_TX"
#define WORKER_QUEUE_RINGSIZE (32 * 2)


/*
 * Given the rx queue name template above, get the queue name
 */
static inline const char *
get_rx_queue_name(unsigned id) {
    /* buffer for return value. Size calculated by %u being replaced
     * by maximum 3 digits (plus an extra byte for safety) */
    static char buffer[sizeof(WORKER_RXQ_NAME) + 2];

    snprintf(buffer, sizeof(buffer) - 1, WORKER_RXQ_NAME, id);
    return buffer;
}

/*
 * Given the tx queue name template above, get the queue name
 */
static inline const char *
get_tx_queue_name(unsigned id) {
    /* buffer for return value. Size calculated by %u being replaced
     * by maximum 3 digits (plus an extra byte for safety) */
    static char buffer[sizeof(WORKER_TXQ_NAME) + 2];

    snprintf(buffer, sizeof(buffer) - 1, WORKER_TXQ_NAME, id);
    return buffer;
}


struct worker_info init_workers(uint8_t nb_workers) {
    unsigned i;
    unsigned socket_id;
    struct worker_info local_worker_info;
    const char * rq_name;
    const char * tq_name;

    socket_id = rte_socket_id();
    workers = rte_calloc("worker details",
                         nb_workers, sizeof(*workers), 0);
    if (workers == NULL)
        rte_exit(EXIT_FAILURE, "Cannot allocate memory for workers program details\n");
    local_worker_info.nb_workers = 0;
    for(i=0; i < nb_workers; i++) {
        rq_name = get_rx_queue_name(i);
        tq_name = get_tx_queue_name(i);
        workers[i].rx_q = rte_ring_create(rq_name,
                                          WORKER_QUEUE_RINGSIZE * 8 * 2, socket_id,
                                          RING_F_SC_DEQ | RING_F_SP_ENQ);                 /* multi prod, single cons */
        workers[i].tx_q = rte_ring_create(tq_name,
                                          WORKER_QUEUE_RINGSIZE * 8 * 2, socket_id,
                                          RING_F_SC_DEQ | RING_F_SP_ENQ);                 /* multi prod, single cons */

        if (workers[i].rx_q == NULL)
            rte_exit(EXIT_FAILURE, "Cannot create rx ring queue for worker %u\n", i);

        if (workers[i].tx_q == NULL)
            rte_exit(EXIT_FAILURE, "Cannot create tx ring queue for worker %u\n", i);
        local_worker_info.rx_stats[local_worker_info.nb_workers].rx = 0;
        local_worker_info.tx_stats[local_worker_info.nb_workers].tx = 0;
        local_worker_info.tx_stats[local_worker_info.nb_workers].tx_drop = 0;
        local_worker_info.nb_workers++;
    }
    printf("Init %u workers\n", local_worker_info.nb_workers);
    return local_worker_info;
};


void display_workers(unsigned difftime, struct worker_info *workerinfo) {
    unsigned i;
    /* Arrays to store last TX/RX count to calculate rate */
    static uint64_t rx_last[MAX_WORKERS];
    static uint64_t tx_last[MAX_WORKERS];
    static uint64_t tx_drop_last[MAX_WORKERS];

    printf("Gateway workers\n");
    printf("-----\n");
    for (i = 0; i < workerinfo->nb_workers; i++) {
        printf("Worker %u - rx: %9"PRIu64"  (%9"PRIu64" pps)\t"
               "tx: %9"PRIu64"  (%9"PRIu64" pps)\t"
               "tx_drop: %9"PRIu64"  (%9"PRIu64" pps)\n", i,
               workerinfo->rx_stats[i].rx,
               (workerinfo->rx_stats[i].rx - rx_last[i])
               /difftime,
               workerinfo->tx_stats[i].tx,
               (workerinfo->tx_stats[i].tx - tx_last[i])
               /difftime,
               workerinfo->tx_stats[i].tx_drop,
               (workerinfo->tx_stats[i].tx_drop - tx_drop_last[i])
               /difftime);
//        printf("wait_toekn_cnt: %9"PRIu64"\n\n", workerinfo->wait_tokens_cnt[i]);

        rx_last[i] = workerinfo->rx_stats[i].rx;
        tx_last[i] = workerinfo->tx_stats[i].tx;
        tx_drop_last[i] = workerinfo->tx_stats[i].tx_drop;
    }
}


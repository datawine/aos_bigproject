//
// Created by Zhilong Zheng on 2019/3/12.
//

#ifndef SERVERLESS_NFV_WORKER_H
#define SERVERLESS_NFV_WORKER_H

#include <rte_mbuf.h>

#define MAX_WORKERS 16

struct worker {
    struct rte_ring *rx_q;
    struct rte_ring *tx_q;
};

struct worker_rx_stats {
    uint64_t rx;
};

struct worker_tx_stats {
    uint64_t tx;
    uint64_t tx_drop;
};

struct worker_info {
    uint8_t	nb_workers;
    volatile struct worker_rx_stats rx_stats[MAX_WORKERS];
    volatile struct worker_tx_stats tx_stats[MAX_WORKERS];
};

struct worker *workers;

struct worker_info init_workers(uint8_t nb_workers);
void display_workers(unsigned difftime, struct worker_info *workerinfo);

#endif //SERVERLESS_NFV_WORKER_H

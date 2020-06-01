/*
* Created by Zhilong Zheng
*/

#include <stdint.h>
#include <stdio.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include "mempool.h"

#define MBUF_POOL_CACHE_SIZE 256

struct rte_mempool *mbuf_pool;

int init_mbuf_pool(const char* mbuf_pool_name,
                   const uint32_t num_mbufs) {

    mbuf_pool = rte_pktmbuf_pool_create(mbuf_pool_name, num_mbufs,
                                        MBUF_POOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
                                        rte_socket_id());
    printf("init mem_pool %s on socket %d\n", mbuf_pool_name, rte_socket_id());

    if (mbuf_pool == NULL) {
        rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));
    }
    return 0;

}

void pktmbuf_free_bulk(struct rte_mbuf *mbuf_table[], unsigned n) {
    unsigned int i;

    for (i = 0; i < n; i++) {
        rte_pktmbuf_free(mbuf_table[i]);
    }
}

void pktmbuf_free_burst(struct rte_mbuf *mbuf_pkts[], unsigned enqueued, unsigned nb_pkts) {
    unsigned int i;

    for (i = enqueued; i < nb_pkts; ++i) {
        struct rte_mbuf *m = mbuf_pkts[i];
        rte_pktmbuf_free(m);
    }
}
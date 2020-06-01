/*
* Created by Zhilong Zheng
*/

#ifndef GOLF_MEMPOOL_H
#define GOLF_MEMPOOL_H

#include <stdint.h>
#include <rte_mempool.h>
extern struct rte_mempool *mbuf_pool;

int init_mbuf_pool(const char* mbuf_pool_name, const uint32_t num_mbufs);
void pktmbuf_free_bulk(struct rte_mbuf *mbuf_table[], unsigned n);
void pktmbuf_free_burst(struct rte_mbuf *mbuf_pkts[], unsigned enqueued, unsigned nb_pkts);
//TODO: Add ring_buffer and shared_memory interfaces.

#endif //GOLF_MEMPOOL_H

//
// Created by hy on 19-3-17.
//

#ifndef SERVERLESS_NFV_DISPATCHER_H
#define SERVERLESS_NFV_DISPATCHER_H

#include <stdint-gcc.h>
#include <signal.h>
#include <sys/types.h>

#include <rte_mbuf.h>

#include "mempool.h"
#include "tag.h"
#include "common.h"

#define DISPATCHER_BUFFER_PKTS 32

extern struct deliver_pool_t deliver_pool;
extern struct deliver_table_t deliver_table;

struct dispatcher_buffer_t{
    uint16_t num;
    struct rte_mbuf *buffer[DISPATCHER_BUFFER_PKTS];
};

struct dispatcher_buffer_t dispatcher_buffer[QUEUE_NUM];

void
init_dispatcher_buffer(void);

void
dispatcher(struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

uint16_t
aggregator(struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

#endif //SERVERLESS_NFV_DISPATCHER_H

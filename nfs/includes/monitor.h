//
// Created by sjx on 19-3-30.
//

#ifndef SERVERLESS_NFV_MONITOR_H
#define SERVERLESS_NFV_MONITOR_H

#include <pthread.h>

#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_hash.h>
#include <rte_mbuf.h>
#include <stdbool.h>

#include "helper.h"
#include "nfs_common.h"
#include "memzone.h"
#include "mempool.h"
#include "hash.h"
#include "util_dpdk.h"

int
func_monitor_local_init(void);

int
func_monitor_remote_init(void);

uint32_t
func_monitor_local_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts);

uint32_t
func_monitor_remote_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts);

void
monitor_end(void);

#endif //SERVERLESS_NFV_TEST_H

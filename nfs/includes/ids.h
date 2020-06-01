//
// Created by hy on 19-3-27.
//

#ifndef SERVERLESS_NFV_IDS_H
#define SERVERLESS_NFV_IDS_H

#include <pthread.h>

#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_hash.h>
#include <rte_mbuf.h>
#include <stdbool.h>

#include "mempool.h"
#include "helper.h"
#include "nfs_common.h"
#include "memzone.h"
#include "hash.h"
#include "util_dpdk.h"

int
func_ids_local_init(void);

int
func_ids_remote_init(void);

uint32_t
func_ids_local_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts);

uint32_t
func_ids_remote_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts);

void
ids_end(void);

#endif //SERVERLESS_NFV_TEST_H

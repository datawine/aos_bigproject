//
// Created by sjx on 19-3-30.
//

#ifndef SERVERLESS_NFV_VPN_H
#define SERVERLESS_NFV_VPN_H

#include <pthread.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_hash_crc.h>
#include <rte_byteorder.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_ip.h>
#include <rte_ether.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_hash.h>
#include <rte_mbuf.h>

#include "mempool.h"
#include "helper.h"
#include "aes.h"
#include "util_dpdk.h"
#include "nfs_common.h"

int
func_vpn_local_init(void);

int
func_vpn_remote_init(void);

uint32_t
func_vpn_local_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts);

uint32_t
func_vpn_remote_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts);

void
vpn_end(void);

#endif //SERVERLESS_NFV_TEST_H

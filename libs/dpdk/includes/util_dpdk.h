/*
* Created by Zhilong Zheng
*/

#ifndef SERVERLESS_NFV_UTIL_DPDK_H
#define SERVERLESS_NFV_UTIL_DPDK_H

#include <inttypes.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_branch_prediction.h>
#include <rte_hash.h>
#include <rte_malloc.h>

struct rte_mbuf;
struct tcp_hdr;
struct udp_hdr;
struct ipv4_hdr;

#define IP_PROTOCOL_TCP 6
#define IP_PROTOCOL_UDP 17

int
dpdk_pkt_mac_addr_swap(struct rte_mbuf* pkt, unsigned dst_port);

struct tcp_hdr*
dpdk_pkt_tcp_hdr(struct rte_mbuf* pkt);

struct ether_hdr*
dpdk_pkt_ether_hdr(struct rte_mbuf* pkt);

struct udp_hdr*
dpdk_pkt_udp_hdr(struct rte_mbuf* pkt);

struct ipv4_hdr*
dpdk_pkt_ipv4_hdr(struct rte_mbuf* pkt);

void *
dpdk_pkt_payload(struct rte_mbuf *pkt, int *payload_size);

int
dpdk_pkt_is_tcp(struct rte_mbuf* pkt);

int
dpdk_pkt_is_udp(struct rte_mbuf* pkt);

int
dpdk_pkt_is_ipv4(struct rte_mbuf* pkt);

void
dpdk_pkt_print(struct rte_mbuf* pkt);

void
dpdk_pkt_print_tcp(struct tcp_hdr* hdr);

void
dpdk_pkt_print_udp(struct udp_hdr* hdr);

void
dpdk_pkt_print_ipv4(struct ipv4_hdr* hdr);

void
dpdk_pkt_print_ether(struct ether_hdr* hdr);

struct ipv4_5tuple_t {
    uint32_t src_addr;
    uint32_t dst_addr;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  proto;
};

//uint32_t dpdk_calc_5tuple_hashkey(struct ipv4_5tuple_t *key);

void dpdk_fill_fkey(struct ipv4_hdr *ipv4, struct ipv4_5tuple_t *f_key);

#endif //SERVERLESS_NFV_UTIL_DPDK_H

//
// Created by hy on 19-3-16.
//

#ifndef SERVERLESS_NFV_TUNNEL_H
#define SERVERLESS_NFV_TUNNEL_H

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_hash_crc.h>
#include <rte_byteorder.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_ip.h>
#include <rte_ether.h>

#include "tag.h"

#define VXLAN_PEER_PORTS  2

#define VXLAN_FLAG 0x08
//#define VXLAN_VNI 0x01

#define DEFAULT_VXLAN_PORT 4789
#define PORT_MIN	49152
#define PORT_MAX	65535
#define PORT_RANGE ((PORT_MAX - PORT_MIN) + 1)

#define IP_VERSION 0x40
#define IP_HDRLEN  0x05 /* default IP header length == five 32-bits words. */
#define IP_DEFTTL  64   /* from RFC 1340. */
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)
#define IP_DN_FRAGMENT_FLAG 0x0040

/* Init vxlan info */
void
vxlan_init(uint8_t peer_id);

/* Transmit packets after encapsulating */
uint16_t
vxlan_tx_pkts(uint16_t port_id, uint16_t queue_id,
              struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

/* Receive packets before decapsulation */
uint16_t
vxlan_rx_pkts(uint16_t port_id, uint16_t queue_id,
              struct rte_mbuf **pkts_burst, uint16_t rx_count);

#endif //SERVERLESS_NFV_TUNNEL_H

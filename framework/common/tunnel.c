#include "tunnel.h"

struct ipv4_hdr app_ip_hdr[VXLAN_PEER_PORTS];
struct ether_hdr app_l2_hdr[VXLAN_PEER_PORTS];
uint8_t tx_checksum;

struct vxlan_port {
    uint32_t peer_ip;            /**< remote VTEP IP address */
    struct ether_addr peer_mac;  /**< remote VTEP MAC address */
} __rte_cache_aligned;

struct vxlan_conf {
    uint32_t port_ip;                      /**< DPDK port IP address*/
    struct ether_addr port_mac;            /**< DPDK port mac address*/
    struct vxlan_port port[VXLAN_PEER_PORTS]; /**< VXLAN configuration */
} __rte_cache_aligned;

struct vxlan_conf vxdev;

/* structure that caches offload info for the current packet */
union tunnel_offload_info {
    uint64_t data;
    struct {
        uint64_t l2_len:8; /**< L2 (MAC) Header Length. */
        uint64_t l3_len:8; /**< L3 (IP) Header Length. */
        uint64_t l4_len:8; /**< L4 Header Length. */
        uint64_t outer_l2_len:8; /**< outer L2 Header Length */
        uint64_t outer_l3_len:16; /**< outer L3 Header Length */
    };
} __rte_cache_aligned;

/* VTEP IP address */
uint8_t vxlan_peer_ips[2][4] = { {192, 168, 1, 1}, {192, 168, 1, 2} };

/* VTEP MAC address */
//uint8_t vxlan_peer_macs[2][6] = { {0xf8, 0xf2, 0x1e, 0x13, 0x40, 0xb2}, {0x68, 0x91, 0xd0, 0x61, 0x12, 0x4e} };
//uint8_t vxlan_peer_macs[2][6] = { {0xf8, 0xf2, 0x1e, 0x13, 0x40, 0xb2}, {0xf8, 0xf2, 0x1e, 0x13, 0x3b, 0x52} };
uint8_t vxlan_peer_macs[2][6] = { {0xf8, 0xf2, 0x1e, 0x13, 0x40, 0xb2}, {0xf8, 0xf2, 0x1e, 0x13, 0x3e, 0xb2} };

static uint16_t
get_psd_sum(void *l3_hdr, uint16_t ethertype, uint64_t ol_flags)
{
    if (ethertype == ETHER_TYPE_IPv4)
        return rte_ipv4_phdr_cksum(l3_hdr, ol_flags);
    else /* assume ethertype == ETHER_TYPE_IPv6 */
        return rte_ipv6_phdr_cksum(l3_hdr, ol_flags);
}

/**
 * Parse an ethernet header to fill the ethertype, outer_l2_len, outer_l3_len and
 * ipproto. This function is able to recognize IPv4/IPv6 with one optional vlan
 * header.
 */
static void
parse_ethernet(struct ether_hdr *eth_hdr, union tunnel_offload_info *info,
               uint8_t *l4_proto)
{
    struct ipv4_hdr *ipv4_hdr;
    struct ipv6_hdr *ipv6_hdr;
    uint16_t ethertype;

    info->outer_l2_len = sizeof(struct ether_hdr);
    ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);

    if (ethertype == ETHER_TYPE_VLAN) {
        struct vlan_hdr *vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);
        info->outer_l2_len  += sizeof(struct vlan_hdr);
        ethertype = rte_be_to_cpu_16(vlan_hdr->eth_proto);
    }

    switch (ethertype) {
        case ETHER_TYPE_IPv4:
            ipv4_hdr = (struct ipv4_hdr *)
                    ((char *)eth_hdr + info->outer_l2_len);
            info->outer_l3_len = sizeof(struct ipv4_hdr);
            *l4_proto = ipv4_hdr->next_proto_id;
            break;
        case ETHER_TYPE_IPv6:
            ipv6_hdr = (struct ipv6_hdr *)
                    ((char *)eth_hdr + info->outer_l2_len);
            info->outer_l3_len = sizeof(struct ipv6_hdr);
            *l4_proto = ipv6_hdr->proto;
            break;
        default:
            info->outer_l3_len = 0;
            *l4_proto = 0;
            break;
    }
}

/**
 * Calculate the checksum of a packet in hardware
 */
static uint64_t
process_inner_cksums(struct ether_hdr *eth_hdr, union tunnel_offload_info *info)
{
    void *l3_hdr = NULL;
    uint8_t l4_proto;
    uint16_t ethertype;
    struct ipv4_hdr *ipv4_hdr;
    struct ipv6_hdr *ipv6_hdr;
    struct udp_hdr *udp_hdr;
    struct tcp_hdr *tcp_hdr;
    uint64_t ol_flags = 0;

    info->l2_len = sizeof(struct ether_hdr);
    ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);

    if (ethertype == ETHER_TYPE_VLAN) {
        struct vlan_hdr *vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);
        info->l2_len  += sizeof(struct vlan_hdr);
        ethertype = rte_be_to_cpu_16(vlan_hdr->eth_proto);
    }

    l3_hdr = (char *)eth_hdr + info->l2_len;

    if (ethertype == ETHER_TYPE_IPv4) {
        ipv4_hdr = (struct ipv4_hdr *)l3_hdr;
        ipv4_hdr->hdr_checksum = 0;
        ol_flags |= PKT_TX_IPV4;
        ol_flags |= PKT_TX_IP_CKSUM;
        info->l3_len = sizeof(struct ipv4_hdr);
        l4_proto = ipv4_hdr->next_proto_id;
    } else if (ethertype == ETHER_TYPE_IPv6) {
        ipv6_hdr = (struct ipv6_hdr *)l3_hdr;
        info->l3_len = sizeof(struct ipv6_hdr);
        l4_proto = ipv6_hdr->proto;
        ol_flags |= PKT_TX_IPV6;
    } else
        return 0; /* packet type not supported, nothing to do */

    if (l4_proto == IPPROTO_UDP) {
        udp_hdr = (struct udp_hdr *)((char *)l3_hdr + info->l3_len);
        ol_flags |= PKT_TX_UDP_CKSUM;
        udp_hdr->dgram_cksum = get_psd_sum(l3_hdr,
                                           ethertype, ol_flags);
    } else if (l4_proto == IPPROTO_TCP) {
        tcp_hdr = (struct tcp_hdr *)((char *)l3_hdr + info->l3_len);
        ol_flags |= PKT_TX_TCP_CKSUM;
        tcp_hdr->cksum = get_psd_sum(l3_hdr, ethertype, ol_flags);

    }

    return ol_flags;
}

static int
decapsulation(struct rte_mbuf *pkt)
{
    // uint8_t l4_proto = 0;
    // uint16_t outer_header_len;
    uint32_t *function_tag;
    // struct udp_hdr *udp_hdr;
    struct vxlan_hdr *vxlan;
    // union tunnel_offload_info info = { .data = 0 };
    struct ether_hdr *phdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
    // uint64_t *tag;

    // parse_ethernet(phdr, &info, &l4_proto);

    // if (l4_proto != IPPROTO_UDP)
    //    return -1;

    // udp_hdr = (struct udp_hdr *)((char *)phdr +
    //                             info.outer_l2_len + info.outer_l3_len);

    /** check udp destination port, 4789 is the default vxlan port
     * (rfc7348) or that the rx offload flag is set (i40e only
     * currently)*/
    // if (udp_hdr->dst_port != rte_cpu_to_be_16(DEFAULT_VXLAN_PORT) &&
    //    (pkt->packet_type & RTE_PTYPE_TUNNEL_MASK) == 0)
    //    return -1;

    // vxlan = rte_pktmbuf_mtod(pkt, char *) + 42;
    vxlan = (struct vxlan_hdr *)((char *)phdr + 42);
    // vxlan = (struct vxlan_hdr *)((char *)udp_hdr + sizeof(struct udp_hdr));
    function_tag = get_function_tag(pkt);
    // struct ether_hdr *phdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
    // struct ipv4_hdr *ip = (struct ipv4_hdr *) &phdr[1];
        
    // *function_tag = ip->type_of_service;

    // tag = &(pkt->udata64);
    // *tag = 0;

    *function_tag = rte_cpu_to_be_32(vxlan->vx_vni);
//    *function_tag = 0;

//    return 0;

    // outer_header_len = info.outer_l2_len + info.outer_l3_len
                    //   + sizeof(struct udp_hdr) + sizeof(struct vxlan_hdr);

    rte_pktmbuf_adj(pkt, 50);
    // rte_pktmbuf_adj(pkt, outer_header_len);

    return 0;
}

static void
encapsulation(struct rte_mbuf *m)
{
    uint16_t executor_tag;
    uint32_t function_tag;
    // uint64_t ol_flags = 0;
    // uint32_t old_len = m->pkt_len, hash;
    // union tunnel_offload_info tx_offload = { .data = 0 };
    // struct ether_hdr *phdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

    /*Allocate space for new ethernet, IPv4, UDP and VXLAN headers*/
    // struct ether_hdr *pneth = (struct ether_hdr *) rte_pktmbuf_prepend(m,
    //                                                                   sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr)
    //                                                                   + sizeof(struct udp_hdr) + sizeof(struct vxlan_hdr));

    struct ether_hdr *pneth = (struct ether_hdr *) rte_pktmbuf_prepend(m, 50);

    // struct ipv4_hdr *ip = (struct ipv4_hdr *) &phdr[1];
    // struct ipv4_hdr *ip = (struct ipv4_hdr *) &pneth[1];
    // struct udp_hdr *udp = (struct udp_hdr *) &ip[1];
    // struct vxlan_hdr *vxlan = (struct vxlan_hdr *) &udp[1];

    struct vxlan_hdr *vxlan = (struct vxlan_hdr *)((char *)pneth + 42);

//    executor_tag = 0;
    executor_tag = *get_executor_tag(m);
    function_tag = *get_function_tag(m);
//    executor_tag = 0;
//    function_tag = 0;

    /* replace original Ethernet header with ours */
    pneth = rte_memcpy(pneth, &app_l2_hdr[executor_tag],
                       sizeof(struct ether_hdr));
    // phdr = rte_memcpy(phdr, &app_l2_hdr[executor_tag],
    //                    sizeof(struct ether_hdr));
    // ip->type_of_service = (uint8_t)function_tag;    

    /* copy in IP header */
    // ip = rte_memcpy(ip, &app_ip_hdr[executor_tag],
    //                 sizeof(struct ipv4_hdr));
    // ip->total_length = rte_cpu_to_be_16(m->pkt_len
    //                                     - sizeof(struct ether_hdr));

    /* outer IP checksum */
    // ol_flags |= PKT_TX_OUTER_IP_CKSUM;
    // ip->hdr_checksum = 0;

    /* inner IP checksum offload */
    // if (tx_checksum) {
    //     ol_flags |= process_inner_cksums(phdr, &tx_offload);
    //     m->l2_len = tx_offload.l2_len;
    //     m->l3_len = tx_offload.l3_len;
    //     m->l4_len = tx_offload.l4_len;
    //     m->l2_len += ETHER_VXLAN_HLEN;
    // }

    // m->outer_l2_len = sizeof(struct ether_hdr);
    // m->outer_l3_len = sizeof(struct ipv4_hdr);

    // ol_flags |= PKT_TX_TUNNEL_VXLAN;

    // m->ol_flags |= ol_flags;

    /*VXLAN HEADER*/
    // vxlan->vx_flags = rte_cpu_to_be_32(VXLAN_FLAG << 24);
    vxlan->vx_vni = rte_cpu_to_be_32(function_tag);
//    vxlan->vx_vni = rte_cpu_to_be_32(VXLAN_VNI << 8);

    /*UDP HEADER*/
    // udp->dgram_cksum = 0;
    // udp->dgram_len = rte_cpu_to_be_16(old_len
    //                                           + sizeof(struct udp_hdr)
    //                                           + sizeof(struct vxlan_hdr));

    // udp->dst_port = rte_cpu_to_be_16(DEFAULT_VXLAN_PORT);
    // hash = rte_hash_crc(phdr, 2 * ETHER_ADDR_LEN, phdr->ether_type);
    // udp->src_port = rte_cpu_to_be_16((((uint64_t) hash * PORT_RANGE) >> 32)
    //                                  + PORT_MIN);

}

/* Init vxlan info */
void
vxlan_init(uint8_t peer_id)
{
    int i, j;
    struct ipv4_hdr *ip;

    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        vxdev.port_mac.addr_bytes[i] = vxlan_peer_macs[peer_id][i];
        for (j = 0; j < VXLAN_PEER_PORTS; j++) {
            vxdev.port[j].peer_mac.addr_bytes[i] = vxlan_peer_macs[j][i];
        }
    }

    for (i = 0; i < 4; i++) {
        vxdev.port_ip |= vxlan_peer_ips[peer_id][i] << (8 * i);
        for (j = 0; j < VXLAN_PEER_PORTS; j++) {
            vxdev.port[j].peer_ip |= vxlan_peer_ips[j][i] << (8 * i);
        }
    }

    for (i = 0; i < VXLAN_PEER_PORTS; i++) {
        ether_addr_copy(&vxdev.port_mac, &app_l2_hdr[i].s_addr);
        ether_addr_copy(&vxdev.port[i].peer_mac, &app_l2_hdr[i].d_addr);
        app_l2_hdr[i].ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

        ip = &app_ip_hdr[i];
        ip->version_ihl = IP_VHL_DEF;
        ip->type_of_service = 0;
        ip->total_length = 0;
        ip->packet_id = 0;
        ip->fragment_offset = IP_DN_FRAGMENT_FLAG;
        ip->time_to_live = IP_DEFTTL;
        ip->next_proto_id = IPPROTO_UDP;
        ip->hdr_checksum = 0;
        ip->src_addr = vxdev.port_ip;
        ip->dst_addr = vxdev.port[i].peer_ip;
    }

}

/* Transmit packets after encapsulating */
uint16_t
vxlan_tx_pkts(uint16_t port_id, uint16_t queue_id,
              struct rte_mbuf **tx_pkts, uint16_t nb_pkts) {
    uint16_t i, ret = 0;
    for (i = 0; i < nb_pkts; i++) {
        rte_prefetch0(rte_pktmbuf_mtod(tx_pkts[i], void *));
        rte_prefetch0((void *)tx_pkts[i]->udata64);
        encapsulation(tx_pkts[i]);
    }
    ret = rte_eth_tx_burst(port_id, queue_id, tx_pkts, nb_pkts);

    return ret;
}

/* Receive packets before decapsulation */
uint16_t
vxlan_rx_pkts(uint16_t port_id, uint16_t queue_id,
              struct rte_mbuf **pkts_burst, uint16_t rx_count)
{
//    uint32_t nb_rx, i, *function_tag;
//
//    nb_rx = rte_eth_rx_burst(0, 0, pkts_burst, 32);
//    for (i = 0; i < nb_rx; i ++) {
////            function_tag = get_function_tag(pkts[i]);
////            *function_tag = 0;
//        struct vxlan_hdr *vxlan;
//        struct ether_hdr *phdr = rte_pktmbuf_mtod(pkts_burst[i], struct ether_hdr *);
//        vxlan = (struct vxlan_hdr *)((char *)phdr + 42);
//        function_tag = get_function_tag(pkts_burst[i]);
//        *function_tag = rte_cpu_to_be_32(vxlan->vx_vni);
//        rte_pktmbuf_adj(pkts_burst[i], 50);
//    }

    uint16_t i, count_tmp;
    // uint16_t i, count_tmp, count = 0;
    int ret;
//     struct rte_mbuf *pkts_tmp[rx_count];
//     uint32_t *function_tag;

    // count_tmp = rte_eth_rx_burst(port_id, queue_id, pkts_tmp, rx_count);
    count_tmp = rte_eth_rx_burst(port_id, queue_id, pkts_burst, rx_count);

    for (i = 0; i < count_tmp; i++) {
        rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[i], void *));
        rte_prefetch0((void *)pkts_burst[i]->udata64);

        // ret = decapsulation(pkts_tmp[i]);
        ret = decapsulation(pkts_burst[i]);
//         function_tag = get_function_tag(pkts_burst[i]);
//         struct ether_hdr *phdr = rte_pktmbuf_mtod(pkts_burst[i], struct ether_hdr *);
        // struct ipv4_hdr *ip = (struct ipv4_hdr *) &phdr[1];
        // *function_tag = ip->type_of_service;
        // pkts_burst[i]->udata64 = 0;
        // pkts_tmp[i]->udata64 = 0;
        // if (unlikely(ret < 0))
        //     rte_pktmbuf_free(pkts_tmp[i]);
        // else {
        //     pkts_burst[count] = pkts_tmp[i];
        //     count++;
        // }
    }

    return count_tmp;
    // return count;

}
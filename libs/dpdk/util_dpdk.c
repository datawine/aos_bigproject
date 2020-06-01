/*
* Created by Zhilong Zheng
 *
 * Modification from OpenNetVM
*/

#include "util_dpdk.h"

int
dpdk_pkt_mac_addr_swap(struct rte_mbuf* pkt, unsigned dst_port) {
    struct ether_hdr *eth;
    struct ether_addr addr;

    if (unlikely(pkt == NULL)) {
        return -1;
    }

    eth = dpdk_pkt_ether_hdr(pkt);

    ether_addr_copy(&eth->s_addr, &eth->d_addr);

    rte_eth_macaddr_get(dst_port, &eth->s_addr);
    ether_addr_copy(&addr, &eth->s_addr);

    return 0;
}

struct ether_hdr*
dpdk_pkt_ether_hdr(struct rte_mbuf* pkt) {
    if (unlikely(pkt == NULL)) {
        return NULL;
    }
    return rte_pktmbuf_mtod(pkt, struct ether_hdr *);
}

struct tcp_hdr*
dpdk_pkt_tcp_hdr(struct rte_mbuf* pkt) {
    struct ipv4_hdr* ipv4 = dpdk_pkt_ipv4_hdr(pkt);

    if (unlikely(ipv4 == NULL)) {
        return NULL;
    }

    if (ipv4->next_proto_id != IP_PROTOCOL_TCP) {
        return NULL;
    }

    uint8_t* pkt_data = rte_pktmbuf_mtod(pkt, uint8_t*) + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
    return (struct tcp_hdr*)pkt_data;
}

struct udp_hdr*
dpdk_pkt_udp_hdr(struct rte_mbuf* pkt) {
    struct ipv4_hdr* ipv4 = dpdk_pkt_ipv4_hdr(pkt);

    if (unlikely(ipv4 == NULL)) {
        return NULL;
    }

    if (ipv4->next_proto_id != IP_PROTOCOL_UDP) {
        return NULL;
    }

    uint8_t* pkt_data = rte_pktmbuf_mtod(pkt, uint8_t*) + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
    return (struct udp_hdr*)pkt_data;
}

struct ipv4_hdr*
dpdk_pkt_ipv4_hdr(struct rte_mbuf* pkt) {
    struct ipv4_hdr* ipv4 = (struct ipv4_hdr*)(rte_pktmbuf_mtod(pkt, uint8_t*) + sizeof(struct ether_hdr));

    uint8_t version = (ipv4->version_ihl >> 4) & 0b1111;
    if (unlikely(version != 4)) {
        return NULL;
    }
    return ipv4;
}

void *
dpdk_pkt_payload(struct rte_mbuf *pkt, int *payload_size) {
    uint8_t* pkt_data = rte_pktmbuf_mtod(pkt, uint8_t*) + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
    *payload_size = ((struct ipv4_hdr *)pkt_data)->total_length - sizeof(struct ipv4_hdr);

    if (((struct ipv4_hdr *)pkt_data)->next_proto_id == IP_PROTOCOL_TCP) {
        pkt_data = pkt_data + sizeof(struct tcp_hdr);
        *payload_size -= sizeof(struct tcp_hdr);
    } else if (((struct ipv4_hdr *)pkt_data)->next_proto_id == IP_PROTOCOL_UDP) {
        pkt_data = pkt_data + sizeof(struct udp_hdr);
        *payload_size -=sizeof(struct udp_hdr);
    } else {
        pkt_data = NULL;
    }

    return pkt_data;
}

int
dpdk_pkt_is_tcp(struct rte_mbuf* pkt) {
    return dpdk_pkt_tcp_hdr(pkt) != NULL;
}

int
dpdk_pkt_is_udp(struct rte_mbuf* pkt) {
    return dpdk_pkt_udp_hdr(pkt) != NULL;
}

int
dpdk_pkt_is_ipv4(struct rte_mbuf* pkt) {
    return dpdk_pkt_ipv4_hdr(pkt) != NULL;
}

void
dpdk_pkt_print(struct rte_mbuf* pkt) {
    struct ipv4_hdr* ipv4 = dpdk_pkt_ipv4_hdr(pkt);
    if (likely(ipv4 != NULL)) {
        dpdk_pkt_print_ipv4(ipv4);
    }

    struct tcp_hdr* tcp = dpdk_pkt_tcp_hdr(pkt);
    if (tcp != NULL) {
        dpdk_pkt_print_tcp(tcp);
    }

    struct udp_hdr* udp = dpdk_pkt_udp_hdr(pkt);
    if (udp != NULL) {
        dpdk_pkt_print_udp(udp);
    }
}

void
dpdk_pkt_print_tcp(struct tcp_hdr* hdr) {
    printf("Source Port: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->src_port));
    printf("Destination Port: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->dst_port));
    printf("Sequence number: %" PRIu32 "\n", rte_be_to_cpu_32(hdr->sent_seq));
    printf("Acknowledgement number: %" PRIu32 "\n", rte_be_to_cpu_32(hdr->recv_ack));
    printf("Data offset: %" PRIu8 "\n", hdr->data_off);

    uint16_t flags = ((hdr->data_off << 8) | hdr->tcp_flags) & 0b111111111;

    printf("Flags: %" PRIx16 "\n", flags);
    printf("\t(");
    if ((flags >> 8) & 0x1) printf("NS,");
    if ((flags >> 7) & 0x1) printf("CWR,");
    if ((flags >> 6) & 0x1) printf("ECE,");
    if ((flags >> 5) & 0x1) printf("URG,");
    if ((flags >> 4) & 0x1) printf("ACK,");
    if ((flags >> 3) & 0x1) printf("PSH,");
    if ((flags >> 2) & 0x1) printf("RST,");
    if ((flags >> 1) & 0x1) printf("SYN,");
    if (flags        & 0x1) printf("FIN,");
    printf(")\n");

    printf("Window Size: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->rx_win));
    printf("Checksum: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->cksum));
    printf("Urgent Pointer: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->tcp_urp));
}

void
dpdk_pkt_print_udp(struct udp_hdr* hdr) {
    printf("Source Port: %" PRIu16 "\n", hdr->src_port);
    printf("Destination Port: %" PRIu16 "\n", hdr->dst_port);
    printf("Length: %" PRIu16 "\n", hdr->dgram_len);
    printf("Checksum: %" PRIu16 "\n", hdr->dgram_cksum);
}

void
dpdk_pkt_print_ipv4(struct ipv4_hdr* hdr) {
    printf("IHL: %" PRIu8 "\n", hdr->version_ihl & 0b1111);
    printf("DSCP: %" PRIu8 "\n", hdr->type_of_service & 0b111111);
    printf("ECN: %" PRIu8 "\n", (hdr->type_of_service >> 6) & 0b11);
    printf("Total Length: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->total_length));
    printf("Identification: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->packet_id));

    uint8_t flags = (hdr->fragment_offset >> 13) & 0b111;
    printf("Flags: %" PRIx8 "\n", flags);
    printf("\t(");
    if ((flags >> 1) & 0x1) printf("DF,");
    if ( flags       & 0x1) printf("MF,");
    printf("\n");

    printf("Fragment Offset: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->fragment_offset) & 0b1111111111111);
    printf("TTL: %" PRIu8 "\n", hdr->time_to_live);
    printf("Protocol: %" PRIu8, hdr->next_proto_id);

    if (hdr->next_proto_id == IP_PROTOCOL_TCP) {
        printf(" (TCP)");
    }

    if (hdr->next_proto_id == IP_PROTOCOL_UDP) {
        printf(" (UDP)");
    }

    printf("\n");

    printf("Header Checksum: %" PRIu16 "\n", hdr->hdr_checksum);
    printf("Source IP: %" PRIu32 " (%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 ")\n", hdr->src_addr,
           hdr->src_addr & 0xFF, (hdr->src_addr >> 8) & 0xFF, (hdr->src_addr >> 16) & 0xFF, (hdr->src_addr >> 24) & 0xFF);
    printf("Destination IP: %" PRIu32 " (%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 ")\n", hdr->dst_addr,
           hdr->dst_addr & 0xFF, (hdr->dst_addr >> 8) & 0xFF, (hdr->dst_addr >> 16) & 0xFF, (hdr->dst_addr >> 24) & 0xFF);
}

void dpdk_pkt_print_ether(struct ether_hdr* hdr) {
    const char *type = NULL;
    if (unlikely(hdr == NULL)) {
        return;
    }
    printf("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           hdr->s_addr.addr_bytes[0], hdr->s_addr.addr_bytes[1],
           hdr->s_addr.addr_bytes[2], hdr->s_addr.addr_bytes[3],
           hdr->s_addr.addr_bytes[4], hdr->s_addr.addr_bytes[5]);
    printf("Dest MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           hdr->d_addr.addr_bytes[0], hdr->d_addr.addr_bytes[1],
           hdr->d_addr.addr_bytes[2], hdr->d_addr.addr_bytes[3],
           hdr->d_addr.addr_bytes[4], hdr->d_addr.addr_bytes[5]);
    switch(hdr->ether_type) {
        case ETHER_TYPE_IPv4:
            type = "IPv4";
            break;
        case ETHER_TYPE_IPv6:
            type = "IPv6";
            break;
        case ETHER_TYPE_ARP:
            type = "ARP";
            break;
        case ETHER_TYPE_RARP:
            type = "Reverse ARP";
            break;
        case ETHER_TYPE_VLAN:
            type = "VLAN";
            break;
        case ETHER_TYPE_1588:
            type = "1588 Precise Time";
            break;
        case ETHER_TYPE_SLOW:
            type = "Slow";
            break;
        case ETHER_TYPE_TEB:
            type = "Transparent Ethernet Bridging (TEP)";
            break;
        default:
            type = "unknown";
            break;
    }
    printf("Type: %s\n", type);
}

//static uint16_t shift_8 = 1UL << 8;
//static uint32_t shift_16 = 1UL << 16;
//static uint64_t shift_32 = 1UL << 32;
//static uint32_t BIG_PRIME = 10019;

/*
uint32_t dpdk_calc_5tuple_hashkey(struct ipv4_5tuple_t *key) {
    uint64_t hash_val = 0;
    hash_val = key->src_addr;
    hash_val = (hash_val * shift_32 + key->dst_addr);
    hash_val = (hash_val * shift_16 + key->src_port);
    hash_val = (hash_val * shift_16 + key->dst_port);
    hash_val = (hash_val * shift_8 + key->proto);

    return (uint32_t)hash_val;
}
*/

void dpdk_fill_fkey(struct ipv4_hdr *ipv4, struct ipv4_5tuple_t *f_key) {
    struct tcp_hdr *tcp_hdr;
    struct udp_hdr *udp_hdr;
    f_key->proto = ipv4->next_proto_id;
    f_key->src_addr = rte_be_to_cpu_32(ipv4->src_addr);
    f_key->dst_addr = rte_be_to_cpu_32(ipv4->dst_addr);
    if (f_key->proto == IP_PROTOCOL_TCP) {
        tcp_hdr = (struct tcp_hdr *)((uint8_t*)ipv4 + sizeof(struct ipv4_hdr));
        f_key->src_port = rte_be_to_cpu_16(tcp_hdr->src_port);
        f_key->dst_port = rte_be_to_cpu_16(tcp_hdr->dst_port);
    } else if (f_key->proto == IP_PROTOCOL_UDP) {
        udp_hdr = (struct udp_hdr *)((uint8_t*)ipv4 + sizeof(struct ipv4_hdr));
        f_key->src_port = rte_be_to_cpu_16(udp_hdr->src_port);
        f_key->dst_port = rte_be_to_cpu_16(udp_hdr->dst_port);
    } else {
        f_key->src_port = 0;
        f_key->dst_port = 0;
    }
}
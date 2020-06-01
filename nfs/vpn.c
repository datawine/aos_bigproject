//
// Created by hy on 19-3-27.
//

#include "vpn.h"

#define IP_VERSION 0x40
#define IP_HDRLEN  0x05 /* default IP header length == five 32-bits words. */
#define IP_DEFTTL  64   /* from RFC 1340. */
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)
#define IP_DN_FRAGMENT_FLAG 0x0040

#define IS_VPN_SERVER 1
// server end contains encryption
// clinet end contains decryption

struct ipv4_hdr vpn_ip_hdr;
struct ether_hdr vpn_eth_hdr;

// hdr config
struct ipv4_5tuple_t vpn_hdr_config;
// encrypt config
WORD key_schedule[60]; //word Schedule

uint8_t tx_checksum;

/* VTEP MAC address */
uint8_t vpn_server_mac[6] = {0xf8, 0xf2, 0x1e, 0x13, 0x40, 0xb2};
uint8_t vpn_client_mac[6] = {0x68, 0x91, 0xd0, 0x61, 0x12, 0x4e};

/* VTEP IP address */
#define VPN_SERVER_IP 3232236033
#define VPN_CLIENT_IP 3232236034

#define VPN_SERVER_PORT 4567
#define VPN_CLIENT_PORT 3456

/* AES encryption parameters */
static BYTE key[1][32] = {
  {0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81,0x1f,0x35,0x2c,0x07,0x3b,0x61,0x08,0xd7,0x2d,0x98,0x10,0xa3,0x09,0x14,0xdf,0xf4}
};
static BYTE iv[1][16] = {
  {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f}
};

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

static uint16_t
get_psd_sum(void *l3_hdr, uint16_t ethertype, uint64_t ol_flags)
{
    if (ethertype == ETHER_TYPE_IPv4)
        return rte_ipv4_phdr_cksum(l3_hdr, ol_flags);
    else /* assume ethertype == ETHER_TYPE_IPv6 */
        return rte_ipv6_phdr_cksum(l3_hdr, ol_flags);
}

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

static void
vpn_add_hdr(struct rte_mbuf *m)
{
//    printf("adding hdrs\n");
    uint64_t ol_flags = 0;
    uint32_t old_len = m->pkt_len;
    union tunnel_offload_info tx_offload = { .data = 0 };
    struct ether_hdr *phdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

    /*Allocate space for new ethernet, IPv4, UDP and VXLAN headers*/
    struct ether_hdr *pneth =
            (struct ether_hdr *) rte_pktmbuf_prepend(m, sizeof(struct ether_hdr)
                                                        + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));

    struct ipv4_hdr *ip = (struct ipv4_hdr *) &pneth[1];
    struct udp_hdr *udp = (struct udp_hdr *) &ip[1];

    /* replace original Ethernet header with ours */
    rte_memcpy(pneth, &vpn_eth_hdr, sizeof(struct ether_hdr));

    /* copy in IP header */
    rte_memcpy(ip, &vpn_ip_hdr, sizeof(struct ipv4_hdr));
    ip->total_length =
            rte_cpu_to_be_16(m->pkt_len - sizeof(struct ether_hdr));
    /* outer IP checksum */
    ol_flags |= PKT_TX_OUTER_IP_CKSUM;
    ip->hdr_checksum = 0;

    /* inner IP checksum offload */
    if (tx_checksum) {
        ol_flags |= process_inner_cksums(phdr, &tx_offload);
        m->l2_len = tx_offload.l2_len;
        m->l3_len = tx_offload.l3_len;
        m->l4_len = tx_offload.l4_len;
    }

    m->outer_l2_len = sizeof(struct ether_hdr);
    m->outer_l3_len = sizeof(struct ipv4_hdr);

    m->ol_flags |= ol_flags;

    /*UDP HEADER*/
    udp->dgram_cksum = 0;
    udp->dgram_len = rte_cpu_to_be_16(old_len + sizeof(struct udp_hdr));

    udp->dst_port = rte_cpu_to_be_16(VPN_CLIENT_PORT);
    udp->src_port = rte_cpu_to_be_16(VPN_SERVER_PORT);
}

static void
vpn_add_hdr_remote(struct rte_mbuf *m, struct ipv4_hdr *vpn_ip_hdr, struct ether_hdr *vpn_eth_hdr,
            uint16_t vpn_server_port, uint16_t vpn_client_port) {
//    printf("adding hdrs\n");
    uint64_t ol_flags = 0;
    uint32_t old_len = m->pkt_len;
    union tunnel_offload_info tx_offload = { .data = 0 };
    struct ether_hdr *phdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

    /*Allocate space for new ethernet, IPv4, UDP and VXLAN headers*/
    struct ether_hdr *pneth =
            (struct ether_hdr *) rte_pktmbuf_prepend(m, sizeof(struct ether_hdr)
                                                        + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));

    struct ipv4_hdr *ip = (struct ipv4_hdr *) &pneth[1];
    struct udp_hdr *udp = (struct udp_hdr *) &ip[1];

    /* replace original Ethernet header with ours */
    pneth = rte_memcpy(pneth, vpn_eth_hdr, sizeof(struct ether_hdr));

    /* copy in IP header */
    ip = rte_memcpy(ip, vpn_ip_hdr, sizeof(struct ipv4_hdr));
    ip->total_length =
            rte_cpu_to_be_16(m->pkt_len - sizeof(struct ether_hdr));
    /* outer IP checksum */
    ol_flags |= PKT_TX_OUTER_IP_CKSUM;
    ip->hdr_checksum = 0;

    /* inner IP checksum offload */
    if (tx_checksum) {
        ol_flags |= process_inner_cksums(phdr, &tx_offload);
        m->l2_len = tx_offload.l2_len;
        m->l3_len = tx_offload.l3_len;
        m->l4_len = tx_offload.l4_len;
    }

    m->outer_l2_len = sizeof(struct ether_hdr);
    m->outer_l3_len = sizeof(struct ipv4_hdr);

    m->ol_flags |= ol_flags;

    /*UDP HEADER*/
    udp->dgram_cksum = 0;
    udp->dgram_len = rte_cpu_to_be_16(old_len + sizeof(struct udp_hdr));

    udp->dst_port = rte_cpu_to_be_16(vpn_client_port);
    udp->src_port = rte_cpu_to_be_16(vpn_server_port);
}

static void
vpn_encrypt(struct rte_mbuf *pkt) {
    struct udp_hdr *udp;
    struct tcp_hdr *tcp;
    uint16_t plen, hlen;
    uint8_t *pkt_data, *eth;
    uint8_t tmp_data[2000];

    /* Check if we have a valid UDP packet */
    udp = dpdk_pkt_udp_hdr(pkt);
    if (udp != NULL) {
        /* Get at the payload */
        pkt_data = ((uint8_t *) udp) + sizeof(struct udp_hdr);
        /* Calculate length */
        eth = rte_pktmbuf_mtod(pkt, uint8_t *);
        hlen = pkt_data - eth;
        plen = pkt->pkt_len - hlen;

        aes_encrypt_ctr(pkt_data, plen, tmp_data, key_schedule, 256, iv[0]);
    }
    /* Check if we have a valid TCP packet */
    tcp = dpdk_pkt_tcp_hdr(pkt);
    if (tcp != NULL) {
        /* Get at the payload */
        pkt_data = ((uint8_t *) tcp) + sizeof(struct tcp_hdr);
        /* Calculate length */
        eth = rte_pktmbuf_mtod(pkt, uint8_t *);
        hlen = pkt_data - eth;
        plen = pkt->pkt_len - hlen;

        aes_encrypt_ctr(pkt_data, plen, tmp_data, key_schedule, 256, iv[0]);
    }
}

int
func_vpn_local_init(void)
{
    printf("init vpn\n");
	aes_key_setup(key[0], key_schedule, 256);

    int i;
    struct ipv4_hdr *ip;

    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        vpn_eth_hdr.s_addr.addr_bytes[i] = vpn_server_mac[i];
        vpn_eth_hdr.d_addr.addr_bytes[i] = vpn_client_mac[i];
    }

    vpn_eth_hdr.ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

    ip = &vpn_ip_hdr;
    ip->version_ihl = IP_VHL_DEF;
    ip->type_of_service = 0;
    ip->total_length = 0;
    ip->packet_id = 0;
    ip->fragment_offset = IP_DN_FRAGMENT_FLAG;
    ip->time_to_live = IP_DEFTTL;
    ip->next_proto_id = IPPROTO_UDP;
    ip->hdr_checksum = 0;
    ip->src_addr = VPN_SERVER_IP;
    ip->dst_addr = VPN_CLIENT_IP;

    return 0;
}

int
func_vp_remote_init(void)
{
    printf("vpnsg init\n");

    /* AES encryption parameters */
    static BYTE key[1][32] = {{0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,
                                      0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81,
                                      0x1f,0x35,0x2c,0x07,0x3b,0x61,0x08,0xd7,
                                      0x2d,0x98,0x10,0xa3,0x09,0x14,0xdf,0xf4}};
    static BYTE iv[1][16] = {{0x00,0x01,0x02,0x03,
                                     0x04,0x05,0x06,0x07,
                                     0x08,0x09,0x0a,0x0b,
                                     0x0c,0x0d,0x0e,0x0f}};

    aes_key_setup(key[0], key_schedule, 256);

    int i;

    /* VTEP MAC address */
    uint8_t vpn_server_mac[6] = {0xf8, 0xf2, 0x1e, 0x13, 0x40, 0xb2};
    uint8_t vpn_client_mac[6] = {0x68, 0x91, 0xd0, 0x61, 0x12, 0x4e};

    const char* hostname = "127.0.0.1";
    int port = 6379;
    char tmp_key[64], tmp_val[64];
    c = init_redis(hostname, port);
    flush_all(c);
    strcpy(tmp_key, "VPN_SERVER_IP");
    strcpy(tmp_val, "3232236033");
    set_str_key(tmp_key, tmp_val, c);
    strcpy(tmp_key, "VPN_CLIENT_IP");
    strcpy(tmp_val, "3232236034");
    set_str_key(tmp_key, tmp_val, c);
    strcpy(tmp_key, "VPN_SERVER_PORT");
    strcpy(tmp_val, "4567");
    set_str_key(tmp_key, tmp_val, c);
    strcpy(tmp_key, "VPN_CLIENT_PORT");
    strcpy(tmp_val, "3456");
    set_str_key(tmp_key, tmp_val, c);
    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        set_key(i, vpn_server_mac[i], c);
        set_key(i + ETHER_ADDR_LEN, vpn_client_mac[i], c);
    }
    for (i = 0; i < 16; i ++) {
        set_key(i + ETHER_ADDR_LEN * 2, iv[0][i], c);
    }

    return 0;
}

uint32_t
func_vpn_local_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts) {

    struct rte_mbuf **pkts = in_pkts;
    uint32_t i, batch_size = in_nb_pkts;

    for (i = 0; i < batch_size; ++i) {
        vpn_add_hdr(pkts[i]);
        vpn_encrypt(pkts[i]);
        rte_pktmbuf_adj(pkts[i], sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
    }

    // Required for each runtime returns
    out_pkts = pkts;
    return batch_size;
}


uint32_t
func_vpn_remote_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts) {

    struct rte_mbuf **pkts = in_pkts;
    uint32_t i, batch_size = in_nb_pkts;
    struct ipv4_hdr ip;
//    struct ether_hdr vpn_eth_hdr;
    char val[1024];
    uint16_t vpn_client_port, vpn_server_port;

    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        vpn_eth_hdr.s_addr.addr_bytes[i] = get_val(i, c);
        vpn_eth_hdr.d_addr.addr_bytes[i] = get_val(i + ETHER_ADDR_LEN, c);
    }
    for (i = 0; i < 16; i ++) {
        iv[0][i] = (BYTE)get_val(i + ETHER_ADDR_LEN * 2, c);
    }

    vpn_eth_hdr.ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

    ip.version_ihl = IP_VHL_DEF;
    ip.type_of_service = 0;
    ip.total_length = 0;
    ip.packet_id = 0;
    ip.fragment_offset = IP_DN_FRAGMENT_FLAG;
    ip.time_to_live = IP_DEFTTL;
    ip.next_proto_id = IPPROTO_UDP;
    ip.hdr_checksum = 0;
    char tmp_key[64];
    strcpy(tmp_key, "VPN_SERVER_IP");
    get_str_val(tmp_key, val, sizeof(val), c);
    ip.src_addr = atoi(val);
    strcpy(tmp_key, "VPN_CLIENT_IP");
    get_str_val(tmp_key, val, sizeof(val), c);
    ip.dst_addr = atoi(val);
    strcpy(tmp_key, "VPN_SERVER_PORT");
    get_str_val(tmp_key, val, sizeof(val), c);
    vpn_client_port = atoi(val);
    strcpy(tmp_key, "VPN_CLIENT_PORT");
    get_str_val(tmp_key, val, sizeof(val), c);
    vpn_server_port = atoi(val);

    for (i = 0; i < batch_size; ++i) {
        vpn_add_hdr_remote(pkts[i], &ip, &vpn_eth_hdr, vpn_server_port, vpn_client_port);
        vpn_encrypt(pkts[i]);
        rte_pktmbuf_adj(pkts[i], sizeof(struct ether_hdr)
                                 + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
    }

    // Required for each runtime returns
    out_pkts = pkts;
    return batch_size;
}

void
vpn_end(void) {}

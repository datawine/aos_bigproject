/*
* Created by Zhilong Zheng
*/


#include <unistd.h>
#include <rte_config.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include "mempool.h"
#include "port.h"

#define RX_DESC_PER_QUEUE (512 * 2)
#define TX_DESC_PER_QUEUE (512 * 2)

#define RX_RING_SIZE (512 * 2)
#define TX_RING_SIZE (512 * 4)

//source from Netbricks (pmd.c)
#define HW_RXCSUM 0
#define HW_TXCSUM 0
static const struct rte_eth_conf default_eth_conf = {
    .link_speeds = ETH_LINK_SPEED_AUTONEG, /* auto negotiate speed */
    /*.link_duplex = ETH_LINK_AUTONEG_DUPLEX,	[> auto negotiation duplex <]*/
    .lpbk_mode = 0,
    .rxmode =
        {
            .mq_mode = ETH_MQ_RX_RSS,    /* Use RSS without DCB or VMDQ */
            .max_rx_pkt_len = 0,         /* valid only if jumbo is on */
            .split_hdr_size = 0,         /* valid only if HS is on */
//            .header_split = 0,           /* Header Split off */
//            .hw_ip_checksum = HW_RXCSUM, /* IP checksum offload */
//            .hw_vlan_filter = 0,         /* VLAN filtering */
//            .hw_vlan_strip = 0,          /* VLAN strip */
//            .hw_vlan_extend = 0,         /* Extended VLAN */
//            .jumbo_frame = 0,            /* Jumbo Frame support */
//            .hw_strip_crc = 1,           /* CRC stripped by hardware */
        },
    .txmode =
        {
            .mq_mode = ETH_MQ_TX_NONE, /* Disable DCB and VMDQ */
        },
    /* FIXME: Find supported RSS hashes from rte_eth_dev_get_info */
    .rx_adv_conf.rss_conf =
        {
//            .rss_hf = ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP, 
//            .rss_hf = 
            .rss_key = NULL,
        },
    /* No flow director */
    .fdir_conf =
        {
            .mode = RTE_FDIR_MODE_NONE,
        },
    /* No interrupt */
    .intr_conf =
        {
            .lsc = 0,
        },
};

//static const struct rte_eth_conf default_eth_conf = {
//    .rxmode = {
//        .mq_mode = ETH_MQ_RX_RSS,
//        .max_rx_pkt_len = ETHER_MAX_LEN,
//        .ignore_offload_bitfield = 1,
//    },
//    .txmode = {
//        .mq_mode = ETH_MQ_TX_NONE,
//    },
//    .rx_adv_conf = {
//        .rss_conf = {
//            .rss_hf = ETH_RSS_IP | ETH_RSS_UDP |
//                      ETH_RSS_TCP | ETH_RSS_SCTP,
//        }
//    },
//};



//Init for a single port
static int init_single_port(struct rte_mempool *mbuf_pool, uint8_t port_num) {

    struct rte_eth_conf port_conf = default_eth_conf;
    const uint16_t rxRings = 1, txRings = 2;
    int retval;
    uint16_t q;
    uint16_t nb_rxd = RX_RING_SIZE;
    uint16_t nb_txd = TX_RING_SIZE;
    struct rte_eth_dev_info dev_info;
//    struct rte_eth_rxconf rxconf;
    struct rte_eth_txconf txconf;

    uint32_t try_nb = 0;

    if (port_num >= rte_eth_dev_count_avail())
//    if (port_num >= rte_eth_dev_count())
        return -1;

    rte_eth_dev_info_get(port_num, &dev_info);
    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
        port_conf.txmode.offloads |=
            DEV_TX_OFFLOAD_MBUF_FAST_FREE;

    port_conf.rx_adv_conf.rss_conf.rss_hf &= dev_info.flow_type_rss_offloads;

    retval = rte_eth_dev_configure(port_num, rxRings, txRings, &port_conf);
    if (retval != 0)
        return retval;

    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port_num, &nb_rxd, &nb_txd);
    if (retval != 0)
        return retval;

//    rxconf = dev_info.default_rxconf;
//    rxconf.offloads = port_conf.rxmode.offloads;
    for (q = 0; q < rxRings; q++) {
        retval = rte_eth_rx_queue_setup(port_num, q, nb_rxd,
                                        rte_eth_dev_socket_id(port_num),
                                        NULL, mbuf_pool);
/*        retval = rte_eth_rx_queue_setup(port_num, q, nb_rxd,
                                        rte_eth_dev_socket_id(port_num),
                                        &rxconf, mbuf_pool);
*/
        if (retval < 0)
            return retval;
    }

    txconf = dev_info.default_txconf;
//    txconf.txq_flags = ETH_TXQ_FLAGS_IGNORE;
    txconf.offloads = port_conf.txmode.offloads;
    for (q = 0; q < txRings; q++) {
        retval = rte_eth_tx_queue_setup(port_num, q, nb_txd,
                                        rte_eth_dev_socket_id(port_num),
                                        &txconf);
        if (retval < 0)
            return retval;
    }

    retval = rte_eth_dev_start(port_num);
    if (retval < 0)
        return retval;

    struct rte_eth_link link;
    rte_eth_link_get_nowait(port_num, &link);
    while (!link.link_status) {
        printf("Waiting for Link up on port %"PRIu16"\n", port_num);
//        retval  = rte_eth_dev_start(port_num);
//        if (retval < 0) {
//            rte_exit(EXIT_FAILURE, "failure when init port %u\n", port_num);
//        }
        sleep(1);
        rte_eth_link_get_nowait(port_num, &link);
        try_nb++;
        if (try_nb > 5)
            break;
    }

    if (!link.link_status) {
        printf("Link down on port %"PRIu16"\n", port_num);
        return 0;
    }

    struct ether_addr addr;
    rte_eth_macaddr_get(port_num, &addr);
    printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
               " %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
           port_num,
           addr.addr_bytes[0], addr.addr_bytes[1],
           addr.addr_bytes[2], addr.addr_bytes[3],
           addr.addr_bytes[4], addr.addr_bytes[5]);

    rte_eth_promiscuous_enable(port_num);

    return 0;
}

uint16_t get_nb_ports(void) {
    return rte_eth_dev_count_avail();
//    return rte_eth_dev_count();
}

//init all ports
struct port_info
init_ports(uint32_t nb_ports, unsigned long portmask, struct rte_mempool *mbuf_pool) {
    uint8_t port_id;
    struct port_info local_port_info;
    local_port_info.port_nb = 0;
    for (port_id = 0; port_id < nb_ports; port_id++) {
        /* skip ports that are not enabled */
        if ((portmask & (1 << port_id)) == 0) {
            printf("\nSkipping disabled port %d\n", port_id);
            continue;
        }
        /* init port */
        init_single_port(mbuf_pool, port_id);
        local_port_info.port_id[local_port_info.port_nb] = port_id;
        local_port_info.rx_stats[local_port_info.port_nb].rx_pkts = 0;
        local_port_info.rx_stats[local_port_info.port_nb].drop_pkts = 0;
        local_port_info.rx_stats[local_port_info.port_nb].enqueue_pkts = 0;
        local_port_info.tx_stats[local_port_info.port_nb].tx_pkts = 0;
        local_port_info.tx_stats[local_port_info.port_nb].drop_pkts = 0;
        local_port_info.tx_stats[local_port_info.port_nb].dequeue_pkts = 0;
        local_port_info.port_nb++;

    }
    return local_port_info;
}


static const char *
print_MAC(uint8_t port) {
        static const char err_address[] = "00:00:00:00:00:00";
        static char addresses[RTE_MAX_ETHPORTS][sizeof(err_address)];

        if (unlikely(port >= RTE_MAX_ETHPORTS))
                return err_address;
        if (unlikely(addresses[port][0] == '\0')) {
                struct ether_addr mac;
                rte_eth_macaddr_get(port, &mac);
                snprintf(addresses[port], sizeof(addresses[port]),
                                "%02x:%02x:%02x:%02x:%02x:%02x\n",
                                mac.addr_bytes[0], mac.addr_bytes[1],
                                mac.addr_bytes[2], mac.addr_bytes[3],
                                mac.addr_bytes[4], mac.addr_bytes[5]);
        }
        return addresses[port];
}


void display_ports(unsigned difftime, struct port_info *portinfo) {
        unsigned i;
        /* Arrays to store last TX/RX count to calculate rate */
        static uint64_t rx_last[RTE_MAX_ETHPORTS];
        static uint64_t rx_enqueue_last[RTE_MAX_ETHPORTS];
        static uint64_t rx_drop_last[RTE_MAX_ETHPORTS];
        static uint64_t tx_dequeue_last[RTE_MAX_ETHPORTS];
        static uint64_t tx_last[RTE_MAX_ETHPORTS];
        static uint64_t tx_drop_last[RTE_MAX_ETHPORTS];

        printf("PORTS\n");
        printf("-----\n");
        for (i = 0; i < portinfo->port_nb; i++)
                printf("Port %u: '%s'\t", (unsigned)portinfo->port_id[i],
                                print_MAC(portinfo->port_id[i]));
        printf("\n\n");
        for (i = 0; i < portinfo->port_nb; i++) {
                printf("Port %u - rx: %9"PRIu64"  (%9"PRIu64" pps)\t"
                                "rx_enqueue: %9"PRIu64"  (%9"PRIu64" pps)\t"
                                "rx_drop: %9"PRIu64"  (%9"PRIu64" pps)\n"
                                "tx_dequeue: %9"PRIu64"  (%9"PRIu64" pps)\t"
                                "tx: %9"PRIu64"  (%9"PRIu64" pps)\t"
                                "tx_drop: %9"PRIu64"  (%9"PRIu64" pps)\n\n",
                                (unsigned)portinfo->port_id[i],
                                portinfo->rx_stats[portinfo->port_id[i]].rx_pkts,
                                (portinfo->rx_stats[portinfo->port_id[i]].rx_pkts - rx_last[i])
                                        /difftime,
                                portinfo->rx_stats[portinfo->port_id[i]].enqueue_pkts,
                                (portinfo->rx_stats[portinfo->port_id[i]].enqueue_pkts - rx_enqueue_last[i])
                                        /difftime,
                                portinfo->rx_stats[portinfo->port_id[i]].drop_pkts,
                                (portinfo->rx_stats[portinfo->port_id[i]].drop_pkts - rx_drop_last[i])
                                        /difftime,
                                portinfo->tx_stats[portinfo->port_id[i]].dequeue_pkts,
                                (portinfo->tx_stats[portinfo->port_id[i]].dequeue_pkts - tx_dequeue_last[i])
                                        /difftime,
                                        portinfo->tx_stats[portinfo->port_id[i]].tx_pkts,
                                (portinfo->tx_stats[portinfo->port_id[i]].tx_pkts - tx_last[i])
                                        /difftime,
                                        portinfo->tx_stats[portinfo->port_id[i]].drop_pkts,
                                (portinfo->tx_stats[portinfo->port_id[i]].drop_pkts - tx_drop_last[i])
                                        /difftime);

                rx_last[i] = portinfo->rx_stats[portinfo->port_id[i]].rx_pkts;
                rx_enqueue_last[i] = portinfo->rx_stats[portinfo->port_id[i]].enqueue_pkts;
                rx_drop_last[i] = portinfo->rx_stats[portinfo->port_id[i]].drop_pkts;
                tx_dequeue_last[i] = portinfo->tx_stats[portinfo->port_id[i]].dequeue_pkts;
                tx_last[i] = portinfo->tx_stats[portinfo->port_id[i]].tx_pkts;
                tx_drop_last[i] = portinfo->tx_stats[portinfo->port_id[i]].drop_pkts;
        }
}

/*
 * A multiple flow generator, support different pktsize in one flow
 * and different rx rate among multiple flows
 *
 *   Created by Haiping Wang on 2018/4/5.
 */
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#define RX_RING_SIZE 512
#define TX_RING_SIZE 512
#define NUM_MBUFS 8192 * 4
#define MBUF_SIZE (1600 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define MBUF_CACHE_SIZE 0
#define BURST_SIZE 32
#define NO_FLAGS 0
#define FLOWS_NUM_MAX 20
#define DSTIP_RANGE_BASE 10000
#define RING_MAX 1
#define LATENCY_ENTRY_NUM 2000000
//baseline(tx:30):ipsec:650us  nids:560us  firewall:414us router:296us
//parameters to set

//pkt size: 100, 128, 256, 512, 1024, 2048
#define BASE_PKT_SIZE 64
#define MIN_PKT_SIZE 100
#define PKT_SIZE_STEP 5
#define DST_IP_LEN 10000000
//port and rx/tx
#define NUM_PORTS 1
#define FLOWS_NUM 1

//launch TX_THREAD_NUM/RX_NUM tx/rx and each assigned to one port selected from left to right in port_id
static uint8_t port_id[NUM_PORTS] = {0, 1};
static uint16_t tx_level[FLOWS_NUM_MAX] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
//TODO:发流速度的delay参数在这里设置
//6:
static uint16_t tx_delay[FLOWS_NUM_MAX] = {500, 500, 500, 500, 500, 500, 500};

//flows information
//note the dst_ip%DSTIP_RANGE_BASE of each flow must be different
static uint32_t dst_ips[FLOWS_NUM_MAX]={
        67217600, 33663168,16885952 ,50440384,
        33663166, 16885953 ,  50440385, 67217604,
        33663167, 16885954, 50440386, 67217608,};

int rx_thread_num;

//stats
static uint64_t f_bytes_cnt[FLOWS_NUM +1][FLOWS_NUM];
static uint64_t f_bytes_cnt_last[FLOWS_NUM +1][FLOWS_NUM];
static uint64_t f_latency[FLOWS_NUM +1][FLOWS_NUM][LATENCY_ENTRY_NUM];
static uint64_t f_pkt_cnt[FLOWS_NUM +1][FLOWS_NUM];
static uint64_t f_pkt_send_cnt[FLOWS_NUM +1][FLOWS_NUM];
static uint64_t f_pkt_send_cnt_last[FLOWS_NUM +1][FLOWS_NUM];
static uint64_t f_bytes_send[FLOWS_NUM +1][FLOWS_NUM];
static uint64_t f_bytes_send_last[FLOWS_NUM +1][FLOWS_NUM];
//not used now
static uint64_t p_pkt_cnt[NUM_PORTS];
static uint64_t p_pkt_cnt_last[NUM_PORTS];
static uint64_t p_bytes_cnt[NUM_PORTS];
static uint64_t p_bytes_cnt_last[NUM_PORTS];
static uint64_t p_latency[NUM_PORTS];
static uint64_t p_latency_last[NUM_PORTS];

//helper variables
uint8_t dstip_flowid_mapping[DSTIP_RANGE_BASE + 10] = {0};
static void ** f_headers[FLOWS_NUM];
static struct rte_mempool *pktmbuf_pool;
static uint8_t keep_running = 1;
static uint16_t len; //the length of header
char *chars;


//scale point
int dynamic_tx_delay = 100;  //初始化
int should_stop_tx = 0;


//now each rx/tx work for one flow
struct port_info
{
    uint8_t port;
    uint8_t queue;
    uint16_t level;
    uint8_t thread_id;
    uint16_t fid;
};

static const struct rte_eth_conf port_conf_default = {
        .rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN },
        .txmode = {
                .mq_mode = ETH_MQ_TX_NONE,
        },
};

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static int port_init(uint8_t port) {
    struct rte_eth_conf port_conf = port_conf_default;
    const uint16_t rx_rings = RING_MAX;
    const uint16_t tx_rings = RING_MAX;

    int retval;
    uint16_t q;

    if (port >= rte_eth_dev_count())
        return -1;

    /* Configure the Ethernet device. */
    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    /* Allocate and set up 1 RX queue per Ethernet port. */
    for (q = 0; q < rx_rings; q++) {
        retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
                                        rte_eth_dev_socket_id(port), NULL, pktmbuf_pool);
        if (retval < 0)
            return retval;
    }

    /* Allocate and set up 1 TX queue per Ethernet port. */
    for (q = 0; q < tx_rings; q++) {
        retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
                                        rte_eth_dev_socket_id(port), NULL);
        if (retval < 0)
            return retval;
    }

    /* Start the Ethernet port. */
    retval = rte_eth_dev_start(port);
    if (retval < 0)
        return retval;

    /* Display the port MAC address. */
    struct ether_addr addr;
    rte_eth_macaddr_get(port, &addr);
    printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
    " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
            (unsigned)port,
            addr.addr_bytes[0], addr.addr_bytes[1],
            addr.addr_bytes[2], addr.addr_bytes[3],
            addr.addr_bytes[4], addr.addr_bytes[5]);

    /* Enable RX in promiscuous mode for the Ethernet device. */
    printf("try to enable port %d\n", port);
    rte_eth_promiscuous_enable(port);
//    printf(">success\n");
    struct rte_eth_link  link;
    int cnt = 0;
    do{
        rte_eth_link_get(port, &link);
        cnt++;
        printf("try get link %d\n", cnt);
        if(cnt==6)
            break;
    }while(link.link_status != ETH_LINK_UP);
    if(link.link_status == ETH_LINK_DOWN){
//        rte_exit(EXIT_FAILURE, ":: error: link is still down\n");
        printf("link down with port %d\n", port);
        return 0;
    }
//    rte_delay_ms(1);
    printf("finish init\n");
    return 0;
}

static void
init_stats(void){
    int i;
    int j;
    int k;
    uint16_t p_id;
    for (i = 0;i<FLOWS_NUM;i++){
        for(j = 0;j<FLOWS_NUM;j++){
            f_pkt_cnt[i][j] = 0;
            f_pkt_send_cnt[i][j] = 0;
            f_pkt_send_cnt_last[i][j] = 0;
            f_bytes_send[i][j] = 0;
            f_bytes_send_last[i][j] = 0;
            f_bytes_cnt[i][j] = 0;
            f_bytes_cnt_last[i][j] = 0;
            for(k = 0;k<LATENCY_ENTRY_NUM;k++){
                f_latency[i][j][k] = 0;
            }
        }
    }
    for(i = 0;i<NUM_PORTS;i++){

        p_id = port_id[i];
        p_pkt_cnt[p_id] = 0;
        p_pkt_cnt_last[p_id] = 0;
        p_latency[p_id] = 0;
    }
}

static int
init_mbuf_pools(void) {

    const unsigned num_mbufs = NUM_MBUFS * NUM_PORTS;

    /* don't pass single-producer/single-consumer flags to mbuf create as it
     * seems faster to use a cache instead */
    printf("Creating mbuf pool '%s' [%u mbufs] ...\n",
           "MBUF_POOL", num_mbufs);
    pktmbuf_pool = rte_mempool_create("MBUF_POOL", num_mbufs,
                                      MBUF_SIZE, MBUF_CACHE_SIZE,
                                      sizeof(struct rte_pktmbuf_pool_private), rte_pktmbuf_pool_init,
                                      NULL, rte_pktmbuf_init, NULL, rte_socket_id(), NO_FLAGS);

    return (pktmbuf_pool == NULL); /* 0  on success */
}



static void
init_headers(void){
    struct ether_hdr *eth;
    struct ipv4_hdr *ipv4;
    struct udp_hdr *udp;
    int i;
    uint8_t pkt[42] = {
            0x68, 0x05, 0xca, 0x1e, 0xa5, 0xe4,  0x68, 0x05, 0xca, 0x2a, 0x95, 0x62,  0x08, 0x00,
            0x45, 0x00, 0x00, 0x20,  0x8d, 0x4a,  0x40, 0x00,  0x40, 0x11,  0x8f, 0x7c,  0x0a, 0x00,
            0x05, 0x04,  0x0a, 0x00, 0x05, 0x03,  0xe4, 0x91,  0x09, 0xc4,  0x00, 0x0c,  0x8f, 0x3d,  //start data here
            //0x00, 0x00,  0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00
    };//udp
    len = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr);
    printf("header len : %d\n", len);
    char header_name[FLOWS_NUM_MAX][20] = {"header0", "header1","header2", "header3", "header4", "header5", "header6"
            "header7", "header8", "header9", "header10", "header11", "header12"};

    for (i = 0;i<FLOWS_NUM;i++){
        //allocate memory
        //header_name[6] = i - '0';
        f_headers[i] = rte_malloc(header_name[i], len, 0);
        memcpy(f_headers[i], pkt, len);

        //init flow info
        eth = (struct ether_hdr *)f_headers[i];
        eth->s_addr.addr_bytes[0] = 0x08;
        eth->s_addr.addr_bytes[1] = 0x00;
        eth->s_addr.addr_bytes[2] = 0x27;
        eth->s_addr.addr_bytes[3] = 0xbb;
        eth->s_addr.addr_bytes[4] = 0x13;
        eth->s_addr.addr_bytes[5] = 0xcf;

        //send to server 202.112.237.33
        eth->d_addr.addr_bytes[0] = 0xf8;
        eth->d_addr.addr_bytes[1] = 0xf2;
        eth->d_addr.addr_bytes[2] = 0x1e;
        eth->d_addr.addr_bytes[3] = 0x13;
        eth->d_addr.addr_bytes[4] = 0x3a;
        eth->d_addr.addr_bytes[5] = 0xb2;

        ipv4 = (struct ipv4_hdr *)((uint8_t *)f_headers[i] + sizeof(struct ether_hdr));
        ipv4->next_proto_id = 17;
        ipv4->src_addr = 16885952 + i;
        ipv4->dst_addr = dst_ips[i];
        dstip_flowid_mapping[dst_ips[i] % DSTIP_RANGE_BASE] = i;
        printf("ip %lu : flow %d\n", dst_ips[i] % DSTIP_RANGE_BASE, dstip_flowid_mapping[dst_ips[i] % DSTIP_RANGE_BASE]);
        udp = (struct udp_hdr *)((uint8_t *)f_headers[i] + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr));
        udp->src_port = 9;
        udp->dst_port = 9;
    }

}

struct ipv4_hdr*
get_pkt_ipv4_hdr(struct rte_mbuf* pkt) {
    struct ipv4_hdr* ipv4 = (struct ipv4_hdr*)(rte_pktmbuf_mtod(pkt, uint8_t*) + sizeof(struct ether_hdr));

    /* In an IP packet, the first 4 bits determine the version.
     * The next 4 bits are called the Internet Header Length, or IHL.
     * DPDK's ipv4_hdr struct combines both the version and the IHL into one uint8_t.
     */
    uint8_t version = (ipv4->version_ihl >> 4) & 0b1111;
    if (unlikely(version != 4)) {
        return NULL;
    }
    return ipv4;
}

static void handle_signal(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
        keep_running = 0;
}

static int get_mbufs(struct rte_mbufs **pkts){

    int ret;

    while (keep_running){
        ret = rte_pktmbuf_alloc_bulk(pktmbuf_pool, pkts, BURST_SIZE);
        if(ret == 0)
            return 1;
    }
    return 0;
}

/*
 *  send pkts through one port for flows identified from start_fid to end_fid,
 *  tx rate is set by the parameter tx_delay[tx_id]
 */
static int lcore_tx_main(void *arg) {
    uint16_t i, j, sent;
    int ret;
    void *data;

    uint16_t fid = 0;
    uint16_t size_cnt = PKT_SIZE_STEP;
    uint16_t pkt_size_max = BASE_PKT_SIZE;
    uint16_t pkt_size_min = MIN_PKT_SIZE;
    uint16_t pkt_size;
    uint16_t level;
    uint16_t thread_id;
    uint16_t queue_id;

    struct port_info *tx = (struct port_info *)arg;
    struct rte_mbuf *pkts[BURST_SIZE];
    struct timespec *payload;
    struct ipv4_hdr *ipv4;
    struct udp_hdr *udp;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);


    chars = (char *)rte_malloc("1111", 2000, 0);

    level = tx->level;
    fid = tx->fid;
    thread_id = tx->thread_id;
    printf("Core %d: Running TX thread %d, level %d\ndealing fid: %d, delaytime:  %u\n", rte_lcore_id(), tx->thread_id, tx->level,
           fid, tx_delay[level]);

    for(i = 0;i<size_cnt - 1;i++)
        pkt_size_max = pkt_size_max * 2;

    pkt_size= pkt_size_max;
    printf("pkt size start: %"PRIu16"\n", pkt_size);

    for (i = 0; i < 2000; ++i) {
        chars[i] = 'a';
    }

    rte_delay_ms(100);

    for(;keep_running && !should_stop_tx;){

            get_mbufs(pkts);

            for(i = 0;i<BURST_SIZE;i++){

//                pkts[i]->pkt_len = pkts[i]->data_len = pkt_size;
                pkts[i]->pkt_len = pkts[i]->data_len = 128;

                data = rte_pktmbuf_mtod(pkts[i], void*);
                rte_memcpy(data, f_headers[fid], len);
                payload = (struct timespec *)((uint8_t *)data + len );
                clock_gettime(CLOCK_REALTIME, payload);
                void *pay_data;
//                pay_data = (char *)((uint8_t *)data + len + 24);
//                rte_memcpy(pay_data, chars, 128 - len - 24);
            }

            sent = rte_eth_tx_burst(tx->port, tx->queue, pkts, BURST_SIZE);
            //printf("send size of %"PRIu16"\n", pkt_size);
            if (unlikely(sent < BURST_SIZE)) {
                for (j = sent; j < BURST_SIZE; j++)
                    rte_pktmbuf_free(pkts[j]);
            }
            f_pkt_send_cnt[thread_id][fid] += sent;
//            printf("send pkts pktt_send_cnt[%d][%d]=%d\n", thread_id, fid, f_pkt_send_cnt[thread_id][fid]);

            f_bytes_send[thread_id][fid] += sent * pkt_size;

            //vary-size packets
            pkt_size = pkt_size / 2;
            if(pkt_size == pkt_size_min/2)
                pkt_size = pkt_size_max;
            else if(pkt_size<pkt_size_min)
                pkt_size = pkt_size_min;

            // rte_delay_us(tx_delay[level]);
            rte_delay_us(dynamic_tx_delay);

    }

    return 0;
}

/*
 * receive pkts from all queues of one port and update the stats of ports and flows
 */
static int lcore_rx_main(void *arg) {

    uint16_t i, queue_id, receive, p_id, thread_id;
    uint32_t tmp, f_index, packet_index;
    struct ipv4_hdr *ipv4_hdr;
    struct port_info *rx = (struct port_info *)arg;
    struct rte_mbuf *pkts[BURST_SIZE];
    struct timespec *payload, now = {0, 0};
    FILE *fp;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    p_id = rx->port;
    queue_id = rx->queue;
    thread_id = rx->thread_id;

    int recv_cnt = 0;
    char *payload_chars;

    printf("Core %d: Running RX thread %d for port %d queue %d\n", rte_lcore_id(), thread_id, p_id, queue_id);
    rte_delay_ms(1000);
    for (; keep_running;) {
         receive = rte_eth_rx_burst(p_id, queue_id, pkts, BURST_SIZE);

            if (likely(receive > 0)) {
                for (i = 0; i < receive; i++) {

                    void *data;
                    data = rte_pktmbuf_mtod(pkts[i], void*);
                    payload = (struct timespec *)(rte_pktmbuf_mtod(pkts[i], uint8_t *) + len);
                    clock_gettime(CLOCK_REALTIME, &now);
                    ipv4_hdr = get_pkt_ipv4_hdr(pkts[i]);
//                    printf("%u\n", pkts[i]->pkt_len);

//                    if(ipv4_hdr->dst_addr != 67217600){
//                        rte_pktmbuf_free(pkts[i]);
//                        continue;
//
//                    }
//                        printf("dst: %d\n", ipv4_hdr->dst_addr);
//                    if ((1.0e9*now.tv_sec + now.tv_nsec) > (1.0e9*payload->tv_sec + payload->tv_nsec)) {
                        tmp = ((1.0e9*now.tv_sec + now.tv_nsec) - (1.0e9*payload->tv_sec + payload->tv_nsec));
//                        printf("tmp: %u\n", tmp);
//                    } else
//                        continue;
//                    f_index = dstip_flowid_mapping[(int)ipv4_hdr->dst_addr % DSTIP_RANGE_BASE];
                    f_index = 0;
                    packet_index = f_pkt_cnt[thread_id][f_index];
                    f_latency[thread_id][f_index][packet_index] = tmp;
//                    f_bytes_cnt[thread_id][f_index] += pkts[i]->pkt_len;
                    f_pkt_cnt[thread_id][f_index] += 1;
                    if (f_pkt_cnt[thread_id][f_index] > LATENCY_ENTRY_NUM)
                        f_pkt_cnt[thread_id][f_index] -= LATENCY_ENTRY_NUM;
//                    f_pkt_cnt[thread_id][f_index] %= LATENCY_ENTRY_NUM;
//
////                    p_pkt_cnt[p_id] += 1;
////                    p_latency[p_id] += tmp;
////                    p_bytes_cnt[p_id] += pkts[i]->pkt_len;
//
//                    // 打印回来的数据
//                    recv_cnt++;
//                    if (recv_cnt % 5000 == 0) {
//                        payload_chars = (char *)(rte_pktmbuf_mtod(pkts[i], char *) + len + 24);
////                        printf("Recved: %s\n", payload_chars);
//                    }

                    rte_pktmbuf_free(pkts[i]);
                }
            }else {
                continue;
            }
    }
    return 0;
}

int comp(const void*a, const void *b){

    return  *(int *)b - *(int *)a;
}
struct timespec t_start;
struct timespec t_end;
int first_visit = 0;
int count_piece[20]={0};
void getStatistic(uint8_t rx_thread_id, uint8_t flow_id, uint64_t *average, uint64_t *latency_99, uint64_t *latency_95){


    if(first_visit==0){
        first_visit = 1;
        clock_gettime(CLOCK_REALTIME, &t_start);

    }
//    printf("\n calculate t_start\n");
//    clock_gettime(CLOCK_REALTIME, &t_start);
//    printf("finish\n");
    uint64_t tmp_total = 0;
    int i, j, cnt, tail_latency_k, tail_latency_k95;
    int cnt_all=0;
    *average = * latency_99 = *latency_95 = 0;;
    tail_latency_k = cnt * 0.01;
    tail_latency_k95 = cnt * 0.05;
    int max_lantency = 0;
//    int min_latency = 9999999999;
    int piece = 10;
    int span;

    for(flow_id = 0;flow_id<FLOWS_NUM;flow_id++){
        for(rx_thread_id = 0; rx_thread_id<rx_thread_num;rx_thread_id++){
            cnt = f_pkt_cnt[rx_thread_id][flow_id];
            if(cnt<=0)
                continue;
            cnt_all += cnt;
            for(i = 0;i<cnt;i++){
                tmp_total += f_latency[rx_thread_id][flow_id][i];
                if(f_latency[rx_thread_id][flow_id][i]>max_lantency){
                    max_lantency = f_latency[rx_thread_id][flow_id][i];
                }
            }
        }

    }

    if(cnt_all > 0){
        *average = tmp_total /cnt_all;
    }else{
        *average = 0;
    }
    //延时值域区间直方图频数统计
    span = max_lantency/piece;
    for(i = 0;i<piece;i++){
        count_piece[i] = 0;
    }
    for(flow_id = 0;flow_id<FLOWS_NUM;flow_id++){
        for(rx_thread_id =0;rx_thread_id<rx_thread_num;rx_thread_id++){
            cnt = f_pkt_cnt[rx_thread_id][flow_id];
            if(cnt<=0)
                continue;
            for(i = 0;i<cnt;i++){
                count_piece[f_latency[rx_thread_id][flow_id][i]/span]+=1;
            }
        }
    }

    printf("\n\nLatency:\n");
    printf("    %8s    -%8s     %8s\n", "t1", "t2", "count");
    for(i = 0;i<piece;i++){
        printf("    %8d    -%8d us: %8d\n", span*i/1000, span*(i+1)/1000, count_piece[i]);
    }
    printf("\n\n");

    //count 99/95 latency
    //只打印一条流
    int l99 = 0;
    int finish = 0;
    for(flow_id = 0; flow_id < FLOWS_NUM; flow_id++){
        for(rx_thread_id=0;rx_thread_id<rx_thread_num;rx_thread_id++){
            cnt = f_pkt_cnt[rx_thread_id][flow_id];
            if(cnt<=0)
                continue;
            if(finish==1)
                continue;
            tail_latency_k = cnt * 0.01;
            qsort(f_latency[rx_thread_id][flow_id], cnt, sizeof(uint64_t), comp);
            l99 = f_latency[rx_thread_id][flow_id][tail_latency_k];
            clock_gettime(CLOCK_REALTIME, &t_end);
            printf("%us: (avg) %5d us, (99\%) %5d us\n",t_end.tv_sec-t_start.tv_sec, (*average)/1000, l99/1000);
            finish = 1;

//            int time_point = t_end.tv_sec-t_start.tv_sec;
//            if (time_point == 5) {
//                printf(">>>>>>>>>>>>> Increasing traffic rate <<<<<<<<<<<<<\n");
//                dynamic_tx_delay = 100;
//            // }
//
//            }
//            if (time_point == 10) {
//                printf(">>>>>>>>>>>>> Traffic generation ended <<<<<<<<<<<<\n");
//                should_stop_tx = 1;
//            }
        }
    }

    for(flow_id = 0;flow_id<FLOWS_NUM;flow_id++){
        for(rx_thread_id=0;rx_thread_id<rx_thread_num;rx_thread_id++){
            f_pkt_cnt[rx_thread_id][flow_id] = 0;
        }
    }
//    printf("calculate t_end\n");
//    clock_gettime(CLOCK_REALTIME, &t_end);
//    printf("\nstart: %9lu.%9lu, end: %9lu.%9lu\n", t_start.tv_sec, t_start.tv_nsec, t_end.tv_sec, t_end.tv_nsec);
//    printf("\nduration:%9lu\n", ((1.0e9 * t_end.tv_sec + t_end.tv_nsec) - (1.0e9* t_start.tv_sec + t_start.tv_nsec)));

}
int hasscale = 0;

int main(int argc, char *argv[]) {

    int ret, i, j;
    uint8_t total_ports, cur_lcore;
    struct port_info *tx[2 * FLOWS_NUM];//infor of port using by thread
    struct port_info *rx[2 * FLOWS_NUM];
    int latency_tmp;
//    int rx_thread_num;
//    int delay_time = 1000 - FLOWS_NUM * 200;

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

    argc -= ret;
    argv += ret;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    total_ports = rte_eth_dev_count();
    printf("available ports: %d\n", total_ports);

    //configuration of each tx thread, each tx thread send one flow
    for(i = 0; i< FLOWS_NUM;i++){

        tx[i] = calloc(1, sizeof(struct port_info));
        //50-50 distribute to 2 ports
        tx[i]->port = 0;//1:1 allocate tx thread to 2 ports
        tx[i]->queue = 0;
        tx[i]->level = tx_level[i];
        tx[i]->thread_id = i;
        tx[i]->fid = i;

    }

//    rx_thread_num = (FLOWS_NUM>total_ports?FLOWS_NUM:total_ports);
    rx_thread_num = NUM_PORTS;
    printf("rx_thread_num:%d\n", rx_thread_num);
    //configuration of each rx thread, each rx thread hold FLOWS_NUM arrays to independently count packets for flows
    int cnt_p1 = 0, cnt_p2 = 0;
    for (i = 0; i < rx_thread_num ; ++i) {
        rx[i] = calloc(1, sizeof(struct port_info));
        //50-50 distribute to 2 ports
        rx[i]->port = 0;
        if(i%2==0){
            rx[i]->queue = cnt_p1;
            cnt_p1++;
        } else{
            rx[i]->queue = cnt_p2;
            cnt_p2++;
        }
        printf("rx[%d]->port:%d\n", i, rx[i]->port);
        rx[i]->thread_id = i;
    }

    cur_lcore = rte_lcore_id();
    if (total_ports < 1)
        rte_exit(EXIT_FAILURE, "ports is not enough\n");
    ret = init_mbuf_pools();
    if (ret != 0)
        rte_exit(EXIT_FAILURE, "Cannot create needed mbuf pools\n");

    for(i = 0;i<NUM_PORTS;i++){
        ret = port_init(port_id[i]);
        if (ret != 0)
            rte_exit(EXIT_FAILURE, "Cannot init tx port %u\n", tx[i]->port);
    }

    init_headers();

    for(i = 0;i<FLOWS_NUM;i++){
        cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
        if (rte_eal_remote_launch(lcore_tx_main, (void *) tx[i], cur_lcore) == -EBUSY) {
            printf("Core %d is already busy, can't use for tx \n", cur_lcore);
            return -1;
        }
    }

    for( i = 0;i< rx_thread_num ;i++){
        cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
        printf("in launching rx %d port %d\n", i, rx[i]->port);
        if (rte_eal_remote_launch(lcore_rx_main, (void *)rx[i],  cur_lcore) == -EBUSY) {
            printf("Core %d is already busy, can't use for rx %d \n", cur_lcore, i);
            return -1;
        }

    }
    const char clr[] = { 27, '[', '2', 'J', '\0' };
    const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

    int flow_id = 0;
    long long total_sent = 0;
    long long total_tx_pps = 0;
    long long total_rx_pps = 0;
    long long round = 0;

    long long total_sent_pkts = 0;
    long long total_recv_pkts = 0;

    for (; keep_running;){
//        clock_gettime(CLOCK_REALTIME, &t_start);

        /* Clear screen and move to top left */
//        printf("%s%s", clr, topLeft);
        const unsigned sleeptime = 1;
        int i, rx_id, tx_id;
        uint16_t p_id;
        uint64_t average = 0, latency_99 = 0, latency_95 = 0;
        total_rx_pps = 0;
        total_tx_pps = 0;

        printf("\n-----------------\nTraffic Generator\n-----------------\n\nTransmission Rate:\n");
        for(tx_id = 0;tx_id<FLOWS_NUM/2;tx_id++){
//        for(tx_id = 0;tx_id<1;tx_id++){
            i = tx_id;
//            printf("Port %d, send flow %d ", tx_id>=(FLOWS_NUM/2)? 1 : 0, dst_ips[i]);
//            printf("FLOW %d: ", dst_ips[i]);
//            printf("Send: %8lu pps, %8lf Gbps\n", (f_pkt_send_cnt[tx_id][i] - f_pkt_send_cnt_last[tx_id][i]),
//                   (double) (f_bytes_send[tx_id][i] - f_bytes_send_last[tx_id][i]) * 8 / (1024 * 1024 * 1024));
            total_tx_pps += ((f_pkt_send_cnt[tx_id][i] - f_pkt_send_cnt_last[tx_id][i]));
            f_pkt_send_cnt_last[tx_id][i] = f_pkt_send_cnt[tx_id][i];
            f_bytes_send_last[tx_id][i] = f_bytes_send[tx_id][i];



        }

        total_sent_pkts += total_tx_pps;

        printf("total sent: %lld\n", total_sent_pkts);

//        printf("\n\nThroughput:\n");
        for (i = 0; i < FLOWS_NUM; ++i) {
            for (rx_id = 0; rx_id < rx_thread_num; rx_id++) {
                if(f_pkt_cnt[rx_id][i]<=0)
                    continue;
//                printf("FLOW %d: ", dst_ips[i]);
//                printf("Recv: %8lu pps, %8lf Gbps\n", (f_pkt_cnt[rx_id][i]),
//                       (double) ((int) f_bytes_cnt[rx_id][i] - (int) f_bytes_cnt_last[rx_id][i]) * 8 /
//                       (1024 * 1024 * 1024));
                total_rx_pps += (f_pkt_cnt[rx_id][i]);
                f_bytes_cnt_last[rx_id][i] = f_bytes_cnt[rx_id][i];
//                printf("avg latency: %9d ns, latency_99:%9d ns, latency_95:%9d\n", average, latency_99, latency_95);
//                printf("avg latency: %9d us, latency_99:%9d us, latency_95:%9dus\n", average/1000, latency_99/1000, latency_95/1000);

            }
        }

        total_recv_pkts += total_rx_pps;

        printf("total recv: %lld\n", total_recv_pkts);

        total_sent += total_tx_pps;
        // printf("Tx Rate: %8lld pps, Rx Rate: %8lld pps, Total sent: %10lld, dropped: %8lld\n", total_tx_pps, total_rx_pps, total_sent, total_sent_pkts - total_recv_pkts);

        printf("Tx Rate: %8lld kpps, Rx Rate: %8lld kpps\n", total_tx_pps/1000, total_rx_pps/1000);
        getStatistic(rx_id, i, &average, &latency_99, &latency_95);

        rte_delay_ms(950);
//        rte_delay_ms(500);

    }
    return 0;
}


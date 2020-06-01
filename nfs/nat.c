//
// Created by sjx on 19-3-31.
//

#include "nat.h"

//static long long time_sum = 0;
//static uint32_t time_cnt = 0;
static uint32_t seed;

static uint32_t new_static_ip = 3232236033;
static uint8_t used_port[65536];
#ifdef USE_SPINLOCK
    static pthread_spinlock_t spinlock;
#else
    static pthread_mutex_t mutex;
#endif

#define TABLE_CNT 20480
//#define USE_SPINLOCK 0

struct nat_state_entry_t {
    uint32_t new_src_ip, new_dst_ip;
    uint16_t new_src_port, new_dst_port;
};

int
func_nat_local_init(void)
{
    printf("init nat hash table\n");
    struct hash_table_t** mz;
    if (rte_memzone_lookup("nat") != NULL) {
        mz = memzone_lookup("nat");
        hash_table = *mz;
    } else {
        char table_name[64] = "nat_hashtable", data_name[64] = "nat_data";
        hash_table = init_hash_table(TABLE_CNT, sizeof(struct nat_state_entry_t), table_name, data_name);
        mz = memzone_reserve("nat", sizeof(struct hash_table_t *), 1);
        *mz = hash_table;
    }

#ifdef USE_SPINLOCK
    pthread_spin_init(&(spinlock), 0);
#else
    pthread_mutex_init(&(mutex), 0);
#endif

    uint32_t i;
    for (i = 0; i < 65536; i ++) {
        used_port[i] = 0;
    }

    seed = get_hashseed();
    return 0;
}

int
func_nat_remote_init(void)
{
    printf("init natsg hash table\n");

    uint32_t i;

    const char* hostname = "127.0.0.1";
    int port = 6379;
    c = init_redis(hostname, port);
    flow_map = init_redis(hostname, port + 1);

    flush_all(c);
    flush_all(flow_map);

    for (i = 0; i < 65536; i ++) {
        set_key(i, 0, c);
    }
    uint32_t tmp_new_static_ip = 3232236033;
    set_key(65536, tmp_new_static_ip, c);

    seed = get_hashseed();

    return 0;
}


static int find_new_port(void) {
    uint32_t i;
    int ret = -1;

#ifdef USE_SPINLOCK
        pthread_spin_lock(&spinlock);
        for (i = 0; i < 65536; i ++) {
            if (used_port[i] == 0) {
                used_port[i] = 1;
                ret = i;
                break;
            }
        }
        pthread_spin_unlock(&spinlock);
#else
        pthread_mutex_lock(&mutex);
        for (i = 0; i < 65536; i ++) {
            if (used_port[i] == 0) {
                used_port[i] = 1;
                ret = i;
                break;
            }
        }
        pthread_mutex_unlock(&mutex);
#endif
    return ret;
}

static int find_new_port_remote(redisContext *c) {
    uint32_t i;
    int ret = -1;
    for (i = 0; i < 65536; i ++) {
//        if (get_val(i, c) == 0) {
//            set_key(i, 1, c);
        if (get_val(i, c) == 0 && set_key_with_lock(i, 1, c) == 1) {
            ret = i;
            break;
        }
    }

    return ret;
}

static int find_new_port_remote2(redisContext *c) {
    uint32_t i;
    int ret = -1;
    for (i = 0; i < 65536; i ++) {
        if (get_val(i, c) == 0) {
            redisCommand(c, "SET %u 1", i);
            ret = i;
            break;
        }
    }

    return ret;
}

static void update_header(struct ipv4_hdr *ipv4, struct nat_state_entry_t *entry) {
    struct udp_hdr* udp;
    struct tcp_hdr* tcp;
    if (ipv4->next_proto_id == IP_PROTOCOL_TCP) {
        tcp = (struct tcp_hdr *)((uint8_t*)ipv4 + sizeof(struct ipv4_hdr));
        ipv4->src_addr = rte_be_to_cpu_32(entry->new_src_ip);
        ipv4->dst_addr = rte_be_to_cpu_32(entry->new_dst_ip);
        tcp->src_port = rte_be_to_cpu_16(entry->new_src_port);
        tcp->dst_port = rte_be_to_cpu_16(entry->new_dst_port);
    } else if (ipv4->next_proto_id == IP_PROTOCOL_UDP) {
        udp = (struct udp_hdr *)((uint8_t*)ipv4 + sizeof(struct ipv4_hdr));
        ipv4->src_addr = rte_be_to_cpu_32(entry->new_src_ip);
        ipv4->dst_addr = rte_be_to_cpu_32(entry->new_dst_ip);
        udp->src_port = rte_be_to_cpu_16(entry->new_src_port);
        udp->dst_port = rte_be_to_cpu_16(entry->new_dst_port);
    } else {
        // it shouldn't happen
    }
}

uint32_t
func_nat_local_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts) {

    struct rte_mbuf **pkts = in_pkts;
    uint32_t i, hash_key, batch_size = in_nb_pkts;
    struct ipv4_5tuple_t f_key;
    struct ipv4_hdr *ipv4;
    struct nat_state_entry_t *entry = NULL;
    int tbl_index, *k;
    uint32_t new_ip;
    uint16_t new_port;
    k = malloc(sizeof(int));

    for (i = 0; i < batch_size; ++i) {

        uint32_t j;
        for (j = 0; j < 15; j ++)
            *k = (*k + j) % 100000;

        ipv4 = dpdk_pkt_ipv4_hdr(pkts[i]);
        dpdk_fill_fkey(ipv4, &f_key);
        hash_key = hash_5tuple(&f_key, seed);

        tbl_index = hash_get_state(hash_table, hash_key, (void **)&entry);

        if (tbl_index < 0) {
            // create new entry
            int port_index = find_new_port();
            new_ip = new_static_ip;
            if (port_index < 0) {
                printf("no free port!\n");
                continue;
            } else {
                new_port = (uint16_t) port_index;
                tbl_index = hash_add_state(hash_table, hash_key, (void **)&entry);
                if (tbl_index >= 0) {
                    entry->new_src_ip = new_ip;
                    entry->new_src_port = new_port;
                    entry->new_dst_ip = f_key.dst_addr;
                    entry->new_src_port = f_key.dst_port;
                    update_header(ipv4, entry);
                }

                // reverse entry
                uint32_t tmp_ip = f_key.src_addr;
                uint16_t tmp_port = f_key.src_port;
                f_key.src_addr = f_key.dst_addr;
                f_key.src_port = f_key.dst_port;
                f_key.dst_addr = new_ip;
                f_key.dst_port = new_port;
                new_ip = tmp_ip;
                new_port = tmp_port;
                hash_key = hash_5tuple(&f_key, seed);

                tbl_index = hash_add_state(hash_table, hash_key, (void **)&entry);
                if (tbl_index >= 0) {
                    entry->new_src_ip = f_key.src_addr;
                    entry->new_src_port = f_key.src_port;
                    entry->new_dst_ip = new_ip;
                    entry->new_dst_port = new_port;
                }
            }
        } else {
            update_header(ipv4, entry);
        }
    }

    free(k);
    // Required for each runtime returns
    out_pkts = pkts;
    return batch_size;
}

uint32_t
func_nat_remote_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts) {

    struct rte_mbuf **pkts = in_pkts;
    uint32_t i, hash_key, batch_size = in_nb_pkts;
    struct ipv4_5tuple_t f_key;
    struct ipv4_hdr *ipv4;
    struct nat_state_entry_t entry;
    uint32_t new_ip;
    uint16_t new_port;
    int *k;
    k = malloc(sizeof(int));
    new_static_ip = get_val(65536, c);

    for (i = 0; i < batch_size; ++i) {
        uint32_t j;
        for (j = 0; j < 15; j++)
            *k = (*k + j) % 100000;

        ipv4 = dpdk_pkt_ipv4_hdr(pkts[i]);
        dpdk_fill_fkey(ipv4, &f_key);
        hash_key = hash_5tuple(&f_key, seed);
        redisAppendCommand(flow_map,"GET %u", hash_key);
    }

    for (i = 0; i < batch_size; ++i) {
        ipv4 = dpdk_pkt_ipv4_hdr(pkts[i]);
        dpdk_fill_fkey(ipv4, &f_key);
        hash_key = hash_5tuple(&f_key, seed);
        redisReply *reply;

        if (redisGetReply(flow_map, (void **) &reply) != REDIS_OK) {
            if (reply->type == REDIS_REPLY_NIL) {
                // create new entry
                int port_index = find_new_port_remote2(c);
                new_ip = new_static_ip;
                if (port_index < 0) {
//                    printf("no free port!\n");
                    continue;
                } else {
                    new_port = (uint16_t) port_index;
                    entry.new_src_ip = new_ip;
                    entry.new_src_port = new_port;
                    entry.new_dst_ip = f_key.dst_addr;
                    entry.new_dst_port = f_key.dst_port;

                    redisCommand(flow_map,"SET %u 1", hash_key, (char *)&entry);
//                    redisAppendCommand(flow_map[in_msg->index],"SET %u %s", hash_key, (char *)&entry);

//                update_header(ipv4, &entry);
                    // reverse entry
                    uint32_t tmp_ip = f_key.src_addr;
                    uint16_t tmp_port = f_key.src_port;
                    f_key.src_addr = f_key.dst_addr;
                    f_key.src_port = f_key.dst_port;
                    f_key.dst_addr = new_ip;
                    f_key.dst_port = new_port;
                    new_ip = tmp_ip;
                    new_port = tmp_port;
                    hash_key = hash_5tuple(&f_key, seed);

                    entry.new_src_ip = f_key.src_addr;
                    entry.new_src_port = f_key.src_port;
                    entry.new_dst_ip = f_key.dst_addr;
                    entry.new_dst_port = f_key.dst_port;
                    redisCommand(flow_map,"SET %u 1", hash_key, (char *)&entry);
//                redisAppendCommand(flow_map[in_msg->index],"SET %u %s", hash_key, (char *)&entry);
                }
            } else {
//            strncpy((char *)&entry, reply->str, sizeof(entry));
//            update_header(ipv4, &entry);
            }
            freeReplyObject(reply);
        }
    }

    free(k);

    out_pkts = pkts;
    return batch_size;
}

void
nat_end(void) {}
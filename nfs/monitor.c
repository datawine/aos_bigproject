//
// Created sjx hy on 19-3-27.
//

#include "monitor.h"

static uint32_t seed;

#define TABLE_CNT 20480

struct monitor_state_entry_t {
#ifdef USE_SPINLOCK
    pthread_spinlock_t spinlock;
#else
    pthread_mutex_t mutex;
#endif
    uint64_t packet_nb_cnt;
    uint64_t packet_size_cnt;
};

struct monitorsg_state_entry_t {
    uint64_t packet_nb_cnt;
    uint64_t packet_size_cnt;
};

int
func_monitor_local_init(void)
{
    printf("init monitor hash table\n");

    struct hash_table_t** mz;
    if (rte_memzone_lookup("monitor") != NULL) {
        mz = memzone_lookup("monitor");
        hash_table = *mz;
    } else {
        char table_name[64] = "monitor_hashtable", data_name[64] = "monitor_data";
        hash_table = init_hash_table(TABLE_CNT, sizeof(struct monitor_state_entry_t), table_name, data_name);
        mz = memzone_reserve("monitor", sizeof(struct hash_table_t *), 1);
        *mz = hash_table;
    }

    if (rte_memzone_lookup("monitor2") != NULL) {
        mz = memzone_lookup("monitor2");
        hash_table2 = *mz;
    } else {
        char table_name[64] = "monitor_hashtable2", data_name[64] = "monitor_data2";
        hash_table2 = init_hash_table(TABLE_CNT, sizeof(struct monitor_state_entry_t), table_name, data_name);
        mz = memzone_reserve("monitor2", sizeof(struct hash_table_t *), 1);
        *mz = hash_table2;
    }

    seed = get_hashseed();
    return 0;
}

int func_monitor_remote_init(void) {
    printf("init monitorsg hash table\n");

    const char* hostname = "127.0.0.1";
    int port = 6379;
    ip_state = init_redis(hostname, port);
    ip_state2 = init_redis(hostname, port + 1);
    flush_all(ip_state);
    flush_all(ip_state2);

    seed = get_hashseed();
    return 0;
}

static void
monitor_update(uint32_t key, uint32_t packet_size, struct hash_table_t *monitor_tbl) {
    struct monitor_state_entry_t *entry = NULL;
    int tbl_index;

    tbl_index = hash_get_state(monitor_tbl, key, (void **)&entry);

    if (tbl_index < 0) {
        tbl_index = hash_add_state(monitor_tbl, key, (void **)&entry);
#ifdef USE_SPINLOCK
        pthread_spin_init(&(entry->spinlock), 0);
#else
        pthread_mutex_init(&(entry->mutex), 0);
#endif
        entry->packet_nb_cnt = 0;
        entry->packet_size_cnt = 0;
    }
#ifdef USE_SPINLOCK
    pthread_spin_lock(&(entry->spinlock));
    entry->packet_nb_cnt += 1;
    entry->packet_size_cnt += packet_size;
    pthread_spin_unlock(&(entry->spinlock));
#else
    pthread_mutex_lock(&(entry->mutex));
    entry->packet_nb_cnt += 1;
    entry->packet_size_cnt += packet_size;
    pthread_mutex_unlock(&(entry->mutex));
#endif

}

static void
monitor_update_remote(uint32_t key, uint32_t val, redisContext *tmp_c) {
    int tbl_index;
    struct monitorsg_state_entry_t entry;

    tbl_index = get_struct_val(key, (char *)&entry, sizeof(struct monitorsg_state_entry_t), tmp_c);

    if (tbl_index == 0) {
        entry.packet_nb_cnt = 1;
        entry.packet_size_cnt = val;
    } else {
        entry.packet_nb_cnt += 1;
        entry.packet_size_cnt += val;
    }

    set_str_key((char *)&key, (char *)&entry, tmp_c);
}

uint32_t
func_monitor_local_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts) {
    struct rte_mbuf **pkts = in_pkts;
    struct ipv4_hdr* ipv4;
    struct ipv4_5tuple_t f_key;
    uint32_t i, batch_size = in_nb_pkts, hash_key;
    int *k;
    k = malloc(sizeof(int));

    for (i = 0; i < batch_size; ++i) {

        uint32_t j;
        for (j = 0; j < 20; j ++)
            *k = (*k + j) % 100000;

        ipv4 = dpdk_pkt_ipv4_hdr(pkts[i]);
        dpdk_fill_fkey(ipv4, &f_key);

        // ip_table
        hash_key = rte_be_to_cpu_32(ipv4->src_addr);
        monitor_update(hash_key, pkts[i]->pkt_len, hash_table);

//        if (f_key.proto == IP_PROTOCOL_TCP) { // tcp table
//            monitor_update(hash_key, pkts[i]->pkt_len, tcp_table);
//        } else if (f_key.proto == IP_PROTOCOL_UDP) { // udp table
//            monitor_update(hash_key, pkts[i]->pkt_len, udp_table);
//        }

        hash_key = hash_5tuple(&f_key, seed);
        monitor_update(hash_key, pkts[i]->pkt_len, hash_table);
    }

    free(k);

    out_pkts = pkts;
    return batch_size;
}

uint32_t
func_monitor_remote_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts) {
    struct rte_mbuf **pkts = in_pkts;
    struct ipv4_hdr* ipv4;
    struct ipv4_5tuple_t f_key;
    uint32_t i, batch_size = in_nb_pkts, hash_key = 0;
    int *k;
    k = malloc(sizeof(int));
    struct monitorsg_state_entry_t entry;

    for (i = 0; i < batch_size; ++i) {
        uint32_t j;
        for (j = 0; j < 20; j ++)
            *k = (*k + j) % 100000;

        ipv4 = dpdk_pkt_ipv4_hdr(pkts[i]);
        dpdk_fill_fkey(ipv4, &f_key);

        // ip_table
        hash_key = rte_be_to_cpu_32(ipv4->src_addr);
        redisAppendCommand(ip_state,"GET %u", hash_key);

        hash_key = hash_5tuple(&f_key, seed);
        redisAppendCommand(ip_state2,"GET %u", hash_key);
    }

    for (i = 0; i < batch_size; ++i) {
        redisReply *reply;
        while (redisGetReply(ip_state, (void **) &reply) != REDIS_OK) {
        }
        if (reply->type == REDIS_REPLY_NIL) {
            entry.packet_nb_cnt = 1;
            entry.packet_size_cnt = pkts[i]->pkt_len;
            redisAppendCommand(ip_state,"SET %u %s", hash_key, (char *)&entry);
        } else {
            strncpy((char *)&entry, reply->str, sizeof(entry));
            entry.packet_nb_cnt ++;
            entry.packet_size_cnt += pkts[i]->pkt_len;
            redisAppendCommand(ip_state,"SET %u %s", hash_key, (char *)&entry);
        }
        freeReplyObject(reply);

        while (redisGetReply(ip_state2, (void **) &reply) != REDIS_OK) {
        }
        if (reply->type == REDIS_REPLY_NIL) {
            entry.packet_nb_cnt = 1;
            entry.packet_size_cnt = pkts[i]->pkt_len;
            redisAppendCommand(ip_state2,"SET %u %s", hash_key, (char *)&entry);
        } else {
            strncpy((char *)&entry, reply->str, sizeof(entry));
            entry.packet_nb_cnt ++;
            entry.packet_size_cnt += pkts[i]->pkt_len;
            redisAppendCommand(ip_state2,"SET %u %s", hash_key, (char *)&entry);
        }
        freeReplyObject(reply);
    }
    free(k);

    out_pkts = pkts;
    return batch_size;
}

void
monitor_end(void) {}
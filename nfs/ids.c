//
// Created by sjx on 19-3-31.
//

#include "ids.h"

#define TABLE_CNT 20480
//#define USE_SPINLOCK 0
static uint32_t seed;

struct ids_state_entry_t {
#ifdef USE_SPINLOCK
    pthread_spinlock_t spinlock;
#else
    pthread_mutex_t mutex;
#endif

    uint32_t malicious_time;
};

#define IDS_RULE_NUM 2
#define STR_LEN 30

struct ids_statistics {
	char str[IDS_RULE_NUM][STR_LEN];
} ids_asset;

static bool
naive_strcmp(char* str, int s_len) {
    int index, p_len, s_head, p_head;
    char* p;
    bool found;

    for (index = 0; index < IDS_RULE_NUM; index ++) {
        p = ids_asset.str[index];
        p_len = STR_LEN;
        found = true;
        for (p_head = 0; p_head < p_len; p_head ++) {
            s_head = 0;
            while ((s_head < s_len) && (p_head + s_head < p_len)) {
                if (p[p_head + s_head] != str[s_head])
                    found = false;
                s_head ++;
            }
        }

        if (found) break;
    }

    return found;
}

static bool snort(struct rte_mbuf *pkt) {
    struct udp_hdr *udp;
    struct tcp_hdr *tcp;
    uint16_t plen, hlen;
    uint8_t *pkt_data, *eth;

    /* Check if we have a valid UDP packet */
    udp = dpdk_pkt_udp_hdr(pkt);
    if (udp != NULL) {
        /* Get at the payload */
        pkt_data = ((uint8_t *) udp) + sizeof(struct udp_hdr);
        /* Calculate length */
        eth = rte_pktmbuf_mtod(pkt, uint8_t *);
        hlen = pkt_data - eth;
        plen = pkt->pkt_len - hlen;

        return naive_strcmp((char *)pkt_data, plen);
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

        return naive_strcmp((char *)pkt_data, plen);
    }

    return false;
}

int func_ids_local_init(void) {
    printf("init ids hash table\n");
    struct hash_table_t** mz;
    if (rte_memzone_lookup("ids") != NULL) {
        mz = memzone_lookup("ids");
        hash_table = *mz;
    } else {
        char table_name[64] = "ids_hashtable", data_name[64] = "ids_data";
        hash_table = init_hash_table(TABLE_CNT, sizeof(struct ids_state_entry_t), table_name, data_name);
        mz = memzone_reserve("ids", sizeof(struct hash_table_t *), 1);
        *mz = hash_table;
    }

    uint16_t index, j;
    for (index = 0; index < IDS_RULE_NUM; index ++) {
        for (j = 0; j < STR_LEN - 1; j ++) {
            ids_asset.str[index][j] = 'a' + (j % 26);
        }
        ids_asset.str[index][STR_LEN - 1] = '\0';
    }

    seed = get_hashseed();
    return 0;
}

int func_ids_remote_init(void) {
    printf("init idssg hash table\n");

    uint16_t index, j;
    const char* hostname = "127.0.0.1";
    int port = 6379;
    c = init_redis(hostname, port);
    flush_all(c);

    for (index = 0; index < IDS_RULE_NUM; index ++) {
        for (j = 0; j < STR_LEN - 1; j ++) {
            ids_asset.str[index][j] = 'a' + (j % 26);
        }
        ids_asset.str[index][STR_LEN - 1] = '\0';
    }

    seed = get_hashseed();
    return 0;
}

uint32_t
func_ids_local_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts) {
    struct ipv4_hdr *ipv4;
    struct ipv4_5tuple_t f_key;
    struct rte_mbuf **pkts = in_pkts;
    struct ids_state_entry_t *entry;
    uint32_t i, hash_key, batch_size = in_nb_pkts, tbl_index;
    uint8_t action;

    for (i = 0; i < batch_size; ++i) {
        ipv4 = dpdk_pkt_ipv4_hdr(pkts[i]);
        dpdk_fill_fkey(ipv4, &f_key);
        hash_key = hash_5tuple(&f_key, seed);
        tbl_index = hash_get_state(hash_table, hash_key, (void **)&entry);

        if (tbl_index > 0) {
#ifdef USE_SPINLOCK
            pthread_spin_lock(&(entry->spinlock));
            entry->malicious_time ++;
            pthread_spin_unlock(&(entry->spinlock));
#else
            pthread_mutex_lock(&(entry->mutex));
            entry->malicious_time ++;
            pthread_mutex_unlock(&(entry->mutex));
#endif
        } else {
            if (snort(pkts[i])) {
                tbl_index = hash_add_state(hash_table, hash_key, (void **)&entry);
#ifdef USE_SPINLOCK
                pthread_spin_init(&(entry->spinlock), 0);
#else
                pthread_mutex_init(&(entry->mutex), 0);
#endif
                entry->malicious_time = 1;
                action = 0; // drop
            } else {
                action = 1;
            }
        }
    }

    out_pkts = pkts;
    return batch_size;
}

uint32_t
func_ids_remote_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts) {
    struct ipv4_hdr *ipv4;
    struct ipv4_5tuple_t f_key;
    struct rte_mbuf **pkts = in_pkts;
    uint32_t i, hash_key, batch_size = in_nb_pkts;
    uint8_t action;
    int tbl_index;

    for (i = 0; i < batch_size; ++i) {
        ipv4 = dpdk_pkt_ipv4_hdr(pkts[i]);
        dpdk_fill_fkey(ipv4, &f_key);
        hash_key = hash_5tuple(&f_key, seed);
        redisAppendCommand(c,"GET %u", hash_key);
    }

    for (i = 0; i < batch_size; ++i) {
        redisReply *reply;
        while(redisGetReply(c, (void **)&reply) != REDIS_OK) {
        }
        if (reply->type == REDIS_REPLY_NIL) {
            tbl_index = -1;
        } else
            tbl_index = atoi(reply->str);
        freeReplyObject(reply);
        if (tbl_index > 0) {
//            add_val_with_lock(hash_key, 1, c[in_msg->index]);
//            set_key(hash_key, 1, c[in_msg->index]);
            redisAppendCommand(c,"SET %u %u", hash_key, 1);
        } else {
            if (snort(pkts[i])) {
//                set_key_with_lock(hash_key, 1, c[in_msg->index]);
//                set_key(hash_key, 1, c[in_msg->index]);
                redisAppendCommand(c,"SET %u %u", hash_key, 1);
            } else {
                action = 1;
            }
        }
    }

    out_pkts = pkts;
    return batch_size;
}

void
ids_end(void) {}
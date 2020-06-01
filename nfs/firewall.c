//
// Created by sjx on 19-3-28.
//

#include "firewall.h"

#define TABLE_CNT 20480

struct fw_state_entry_t {
    uint32_t is_drop;
};

#define firewall_entry_nb 2

struct firewall_entry {
    struct ipv4_5tuple_t flow_key;
    uint32_t action;
};

static struct firewall_entry
        firewall_table[firewall_entry_nb]
        = {{{3232235521, 3232235777, 1234, 5678, 17}, 0},
           {{3232235521, 3232235777, 1234, 5678, 6}, 1}};
static uint32_t seed;

static void init_firewall_local(void) {
    int i;
    uint32_t hash_key;
    struct fw_state_entry_t *entry = NULL;
    for (i = 0; i < firewall_entry_nb; i ++) {
        hash_key = hash_5tuple(&(firewall_table[i].flow_key), seed);
        hash_add_state(hash_table, hash_key, (void **)&entry);
        entry->is_drop = firewall_table[i].action;
    }
}

static void init_firewall_remote(redisContext *c) {
    int i;
    uint32_t hash_key;
    for (i = 0; i < firewall_entry_nb; i ++) {
        hash_key = hash_5tuple(&(firewall_table[i].flow_key), seed);
        // set key
        set_key(hash_key, firewall_table[i].action, c);
    }
}

int func_fw_local_init(void) {
    struct hash_table_t** mz;
//    if (rte_memzone_lookup("firewall") != NULL) {
////        printf("found\n");
//        mz = memzone_lookup("firewall");
//        hash_table = *mz;
//
//    } else {
////        printf("not found\n");
//        char table_name[64] = "firewall_hashtable", data_name[64] = "firewall_data";
//        hash_table = init_hash_table(TABLE_CNT, sizeof(struct fw_state_entry_t), table_name, data_name);
//        mz = memzone_reserve("firewall", sizeof(struct hash_table_t *), 1);
//        *mz = hash_table;
//    }

//    seed = get_hashseed();
//    init_firewall_local();
    return 0;
}

int func_fw_remote_init(void) {
    printf("init fw remote hash table\n");

    const char* hostname = "127.0.0.1";
    int port = 6379;

    c = init_redis(hostname, port);

    flush_all(c);
    init_firewall_remote(c);

    seed = get_hashseed();
    return 0;
}

uint32_t
func_fw_local_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts) {
    struct rte_mbuf **pkts = in_pkts;
    uint32_t i, batch_size = in_nb_pkts, hash_key, out_nb = 0;
    struct ipv4_hdr* ipv4;
    struct ipv4_5tuple_t flow_key;
    struct fw_state_entry_t *entry = NULL;
    int tbl_index = 0, action, *k;
    k = malloc(sizeof(int));

    for (i = 0; i < batch_size; ++i) {

        asm volatile("":::"memory");
        uint32_t j;
        for (j = 0; j < 10; j ++)
            *k = (*k + j) % 100000;

        ipv4 = dpdk_pkt_ipv4_hdr(pkts[i]);
        if (ipv4 == NULL) {
            continue;
        }
        dpdk_fill_fkey(ipv4, &flow_key);

        hash_key = hash_5tuple(&flow_key, seed);
        // get key
        tbl_index = hash_get_state(hash_table, hash_key, (void **)&entry);

        if (tbl_index < 0 || entry->is_drop == 0) {
            action = 0;
            pkts[out_nb] = pkts[i];
            out_nb ++;
        } else {
            action = 1;
            pkts[out_nb] = pkts[i];
            out_nb ++;
        }
    }

    // Required for each runtime returns
    free(k);
    out_pkts = pkts;
    return batch_size;
}

uint32_t
func_fw_remote_handler(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts) {
    struct rte_mbuf **pkts = in_pkts;
    uint32_t i, hash_key, batch_size = in_nb_pkts, out_nb = 0;
    struct ipv4_hdr* ipv4;
    struct ipv4_5tuple_t flow_key;
    int action, *k;
    k = malloc(sizeof(int));

    for (i = 0; i < batch_size; ++i) {
        uint32_t j;
        for (j = 0; j < 10; j++)
            *k = (*k + j) % 100000;
        ipv4 = dpdk_pkt_ipv4_hdr(pkts[i]);
        dpdk_fill_fkey(ipv4, &flow_key);

        hash_key = hash_5tuple(&flow_key, seed);
        // get key

        redisAppendCommand(c,"GET %u", hash_key);
    }

    for (i = 0; i < batch_size; ++i) {
        redisReply *reply;
        while(redisGetReply(c, (void **)&reply) != REDIS_OK) {
        }
        if (reply->type == REDIS_REPLY_NIL) {
            action = -1;
        } else
            action = atoi(reply->str);
        freeReplyObject(reply);
        if (action < 0 || action == 0) {
            pkts[out_nb] = pkts[i];
            out_nb ++;
        } else {
            pkts[out_nb] = pkts[i];
            out_nb ++;
        }
    }

    free(k);

    out_pkts = pkts;
    return out_nb;
}

void fw_end(void) {}
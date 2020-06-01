//
// Created by junxian shen on 2019-08-18.
//

#include "hash.h"

struct hash_table_t *
init_hash_table(uint32_t cnt, uint32_t entry_size, char* table_name, char* data_name) {
    struct hash_table_t* hash_table;
    struct rte_hash_parameters ipv4_hash_params = {
            .name = NULL,
            .entries = cnt,
            .key_len = sizeof(uint32_t),
            .hash_func = NULL,
            .hash_func_init_val = 0,
    };

    char s_table[64];
    snprintf(s_table, sizeof(s_table), "%s", table_name);
//    snprintf(s_table, sizeof(s_table), "fw_hash_table_%u", (uint32_t)rte_get_tsc_cycles());
    hash_table = (struct hash_table_t *)rte_calloc(s_table, 1, sizeof(struct hash_table_t), 0);
    if (hash_table == NULL) {
        return NULL;
    }

    char s[64];
    ipv4_hash_params.name = s;
    ipv4_hash_params.socket_id = rte_socket_id();
    snprintf(s, sizeof(s), "hash_table_%u", (uint32_t)rte_get_tsc_cycles());
    hash_table->hash = rte_hash_create(&ipv4_hash_params);
    if (hash_table->hash == NULL) {
        rte_free(hash_table);
        return NULL;
    }

    printf("hash create\n");

    char s_data[64];
    snprintf(s_data, sizeof(s_data), "%s", data_name);
//    snprintf(s_data, sizeof(s_data), "fw_data_hash_table_%u", (uint32_t)rte_get_tsc_cycles());
    hash_table->cnt = cnt;
    hash_table->entry_size = entry_size;
    hash_table->data = (char*)rte_calloc(s_data, cnt, entry_size, 0);

    if (hash_table->data == NULL) {
        rte_hash_free(hash_table->hash);
        rte_free(hash_table);
        return NULL;
    }
    printf("data create\n");

    return hash_table;
}

int
hash_get_state(struct hash_table_t* hash_table, uint32_t hash_rss, void **state_entry) {
    int tbl_index;
    uint32_t hash_key = hash_rss;

    tbl_index = rte_hash_lookup_with_hash(hash_table->hash, (void *)&hash_key, hash_rss);

    if (tbl_index >= 0) {
        *state_entry = (void *)&hash_table->data[tbl_index * hash_table->entry_size];
    }

    return tbl_index;

}

int
hash_add_state(struct hash_table_t* hash_table, uint32_t hash_rss, void **state_entry) {
    int tbl_index;

    uint32_t hash_key = hash_rss;

    if (hash_table == NULL) {
        printf("hash table is null\n");
        return -1;
    }

    tbl_index = rte_hash_add_key_with_hash(hash_table->hash,
                                           (void *)&hash_key, hash_rss);

    if (tbl_index >= 0) {
        *state_entry = (void *)&hash_table->data[tbl_index * hash_table->entry_size];
    }

    return tbl_index;

}

//static uint16_t shift_8 = 1UL << 8;
static uint32_t shift_16 = 1UL << 16;
//static uint64_t shift_32 = 1UL << 32;

uint32_t hash_5tuple(struct ipv4_5tuple_t *key, uint32_t seed) {
    uint64_t hash_val;
    hash_val = hash_crc32c_two(seed, (uint64_t)key->src_addr, (uint64_t)key->dst_addr);
    hash_val = hash_crc32c_two(seed, hash_val, (uint64_t)key->src_port * shift_16 + key->dst_port);
    hash_val = hash_crc32c_two(seed, hash_val, (uint64_t)key->proto);
    return hash_val;
}

// this function will only be called one time
uint32_t get_hashseed(void) {
//    srand()
    return 100017;
}

//
// Created by junxian shen on 2019-08-18.
//

#ifndef SERVERLESS_NFV_HASH_H
#define SERVERLESS_NFV_HASH_H

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_branch_prediction.h>
#include <rte_hash.h>
#include <rte_malloc.h>
#include "ops.h"
#include "util_dpdk.h"

struct hash_table_t {
    struct rte_hash* hash;
    char* data;
    uint32_t cnt;
    uint32_t entry_size;
};

struct hash_table_t *
init_hash_table(uint32_t cnt, uint32_t entry_size, char* table_name, char* data_name);

int
hash_get_state(struct hash_table_t* hash_table, uint32_t hash_rss, void **state_entry);

int
hash_add_state(struct hash_table_t* hash_table, uint32_t hash_rss, void **state_entry);

/**
 * hash_crc32c_one - hashes one 64-bit word
 * @seed: useful for creating multiple hash functions
 * @val: the word to hash
 *
 * Returns a 32-bit hash value.
 */
static inline uint32_t hash_crc32c_one(uint32_t seed, uint64_t val)
{
    return __mm_crc32_u64(seed, val);
}

/**
 * hash_crc32c_two - hashes two 64-bit words
 * @seed: useful for creating multiple hash functions
 * @a: the first word to hash
 * @b: the second word to hash
 *
 * Returns a 32-bit hash value.
 */
static inline uint32_t hash_crc32c_two(uint32_t seed, uint64_t a, uint64_t b)
{
    seed = __mm_crc32_u64(seed, a);
    return __mm_crc32_u64(seed, b);
}

/*
 * These functions are a simplified subset of CityHash, modified to focus
 * just on small input values. They are based on Google's original CityHash
 * implementation, and are intended to be functionally equivalent.
 *
 * Copyright (c) 2011 Google, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * CityHash was created by Geoff Pike and Jyrki Alakuijala
 */

#define HASH_CITY_K2	0x9ae16a3b2f90404fULL

static inline uint64_t __hash_city_len16(uint64_t u, uint64_t v, uint64_t mul)
{
    uint64_t a, b;
    a = (u ^ v) * mul;
    a ^= (a >> 47);
    b = (v ^ a) * mul;
    b ^= (b >> 47);
    b *= mul;
    return b;
}

static inline uint64_t __hash_city_rotate(uint64_t val, int shift)
{
    return ((val >> shift) | (val << (64 - shift)));
}

/**
 * hash_city_one - hashes one 64-bit word
 * @val: the word to hash
 *
 * This uses Google's CityHash algorithm.
 *
 * Returns a 64-bit hash value.
 */
static inline uint64_t hash_city_one(uint64_t val)
{
    uint64_t mul = HASH_CITY_K2 + 16;
    uint64_t a = val + HASH_CITY_K2;
    uint64_t b = val;
    uint64_t c = __hash_city_rotate(b, 37) * mul + a;
    uint64_t d = (__hash_city_rotate(a, 25) + b) * mul;
    return __hash_city_len16(c, d, mul);
}

/**
 * hash_city_one - hashes one 64-bit word
 * @val_a: the first word to hash
 * @val_b: the second word to hash
 *
 * This uses Google's CityHash algorithm.
 *
 * Returns a 64-bit hash value.
 */
static inline uint64_t hash_city_two(uint64_t val_a, uint64_t val_b)
{
    uint64_t mul = HASH_CITY_K2 + 32;
    uint64_t a = val_a + HASH_CITY_K2;
    uint64_t b = val_b;
    uint64_t c = __hash_city_rotate(b, 37) * mul + a;
    uint64_t d = (__hash_city_rotate(a, 25) + b) * mul;
    return __hash_city_len16(c, d, mul);
}

extern uint32_t jenkins_hash(const void *key, size_t length);

/**
 * rand_crc32c - generates a very fast pseudorandom value using crc32c
 * @seed: a seed-value for the hash
 *
 * WARNING: not a cryptographic hash.
 */
static inline uint64_t rand_crc32c(uint32_t seed)
{
    return hash_crc32c_one(seed, rdtsc());
}

/**
 * rand_city - generates a very fast pseudorandom value using city hash
 *
 * WARNING: not a cryptographic hash.
 */
static inline uint64_t rand_city(void)
{
    return hash_city_one(rdtsc());
}

uint32_t hash_5tuple(struct ipv4_5tuple_t *key, uint32_t seed);

uint32_t get_hashseed(void);

#endif //SERVERLESS_NFV_HASH_H

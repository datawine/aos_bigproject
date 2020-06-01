#ifndef _HELPER_H_
#define _HELPER_H_

#include <inttypes.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_branch_prediction.h>
#include <rte_hash.h>

#include <hiredis.h>

//
// redis related functions
//

redisContext* init_redis(const char* hostname, int port);

void set_key(uint32_t key, uint32_t val, redisContext *c);

void set_str_key(char *key, char *val, redisContext* c);

uint8_t set_key_with_lock(uint32_t key, uint32_t val, redisContext *c);

void add_val_with_lock(uint32_t key, uint32_t val_to_add, redisContext *c);

int get_val(uint32_t key, redisContext *c);

long get_long_val(uint32_t key, redisContext *c);

uint8_t get_str_val(char *key, char *val, int size, redisContext* c);

uint8_t set_struct_with_lock(uint32_t key, char* val, redisContext *c);

uint8_t get_struct_val(uint32_t key, char* val, int size, redisContext *c);

void flush_all(redisContext *c);

#endif
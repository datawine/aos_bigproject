#ifndef SERVERLESS_NFV_COMMON_H
#define SERVERLESS_NFV_COMMON_H

#define MAX_THREADS_NUM 16

#include <hiredis.h>

struct hash_table_t *hash_table, *hash_table2;
redisContext *c;
redisContext *flow_map;
redisContext *ip_state;
redisContext *ip_state2;

#endif
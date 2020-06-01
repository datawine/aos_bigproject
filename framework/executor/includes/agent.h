//
// Created by junxian shen on 2019-05-10.
//

#ifndef SERVERLESS_NFV_AGENT_H
#define SERVERLESS_NFV_AGENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "memzone.h"
#include "manager.h"

#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_hash.h>
#include <rte_mbuf.h>

struct hash_table_t {
    struct rte_hash* hash;
    char* data;
    uint32_t cnt;
    uint32_t entry_size;
};

struct fw_state_entry_t {
    uint32_t is_drop;
};

struct ids_state_entry_t {
#ifdef USE_SPINLOCK
    pthread_spinlock_t spinlock;
#else
    pthread_mutex_t mutex;
#endif

    uint32_t malicious_time;
};

struct monitor_state_entry_t {
#ifdef USE_SPINLOCK
    pthread_spinlock_t spinlock;
#else
    pthread_mutex_t mutex;
#endif
    uint64_t packet_nb_cnt;
    uint64_t packet_size_cnt;
};

struct nat_state_entry_t {
    uint32_t new_src_ip, new_dst_ip;
    uint16_t new_src_port, new_dst_port;
};

struct ipv4_5tuple_t {
    uint32_t src_addr;
    uint32_t dst_addr;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  proto;
};

void agent_start(void* arg);

void agent_init(void* arg);

void agent_single(void *arg);

#endif //SERVERLESS_NFV_AGENT_H

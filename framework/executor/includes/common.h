//
// Created by hy on 19-3-17.
//

#ifndef SERVERLESS_NFV_COMMON_H
#define SERVERLESS_NFV_COMMON_H

#include <sys/types.h>
#include <stdbool.h>
#include <inttypes.h>

#include <rte_ring.h>
#include <rte_spinlock.h>

#include "instance.h"
#include "queue.h"
#include "memzone.h"

#define INSTANCE_NUM 1024
#define QUEUE_NUM 256
#define QUEUE_SIZE 1024
#define FUNC_NUM 64
#define CORE_NUM 32
#define ADJUST_MSG_NUM 1024

#define FUNC_IDLE 0
#define FUNC_EXIST 1
#define FUNC_SLEEPING 2
#define FUNC_RUNNING 3

#define INSTANCE_STATUS_NAME "Instance_%u_status"
#define INSTANCE_RXQ_NAME "Instance_rx_%u_queue"
#define INSTANCE_TXQ_NAME "Instance_tx_%u_queue"
#define INSTANCE_LOCK_NAME "Instance_%u_lock"
#define MANAGER_ADJUSTQ_NAME "Manager_adjust_queue"
#define MANAGER_RXQ_NAME "Manager_rx_queue"
#define MANAGER_TXQ_NAME "Manager_tx_queue"
#define MANAGER_LOCK_NAME "Manager_lock"

#define RTE_LOGTYPE_DISPATCHER          RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_MANAGER          RTE_LOGTYPE_USER2

static volatile bool keep_running = 1;

struct adjust_message_t {
    bool used;
    int queue_id;
};

struct adjust_message_pool_t {
    struct adjust_message_t adjust_messages[ADJUST_MSG_NUM];
};

struct instance_queue_t {
    struct rte_ring *rx_queue;
    struct rte_ring *tx_queue;
    rte_spinlock_t *rte_spinlock;
};

struct manager_queue_t {
    struct rte_ring *adjust_queue;
    struct rte_ring *rx_queue;
    struct rte_ring *tx_queue;
    rte_spinlock_t *rte_spinlock;
    uint32_t rx_nb;
};

struct queue_info_t {
    volatile uint64_t tx;
    volatile uint64_t tx_drop;
    volatile uint64_t rx;
};

struct deliver_pool_t {
    //instance status
    struct instance_status_t *instance_status[QUEUE_NUM];
    //instance queues
    struct instance_queue_t instance_queue[QUEUE_NUM];
    struct queue_info_t instance_queue_info[QUEUE_NUM];
    int batch_cnt[QUEUE_NUM];
    //manager queues
    struct manager_queue_t manager_queue;
    struct queue_info_t manager_queue_info;

};

struct deliver_table_t {
    bool queue_tag[QUEUE_NUM];
    uint16_t used_queue_num;
    int used_queue[QUEUE_NUM];
    int function_instances[FUNC_NUM];
    int function_to_queue[FUNC_NUM][CORE_NUM];
};

/* Get available adjust message from pool */
struct adjust_message_t *
get_available_adjust_message(void);

/* Get the instance status with queue number */
const char *
get_instance_status_name(uint16_t id);

/* Get the rx queue name with queue number */
const char *
get_rx_queue_name(uint16_t id);

/* Get the rx queue name with queue number */
const char *
get_tx_queue_name(uint16_t id);

const char *
get_lock_name(uint16_t id);

/* Init deliver pool for dispatcher and worker */
void
deliver_pool_init(struct deliver_pool_t *deliver_pool);

/* Init deliver table for dispatcher and worker */
void
deliver_table_init(struct deliver_table_t *deliver_table);

/* Display the queues info of pool */
void
display_deliver_info(unsigned difftime, struct deliver_table_t *deliver_table, struct deliver_pool_t *deliver_pool);

#endif //SERVERLESS_NFV_COMMON_H

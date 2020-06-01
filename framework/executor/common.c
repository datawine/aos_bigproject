//
// Created by hy on 19-3-17.
//
#include "common.h"

struct adjust_message_pool_t adjust_message_pool;

/* Get available adjust message from pool */
struct adjust_message_t *
get_available_adjust_message(void) {
    static int pos = 0;
    int i, index;

    for (i = 0; i < ADJUST_MSG_NUM; i++) {
        index = (pos + i) % ADJUST_MSG_NUM;
        if (adjust_message_pool.adjust_messages[index].used == false) {
            adjust_message_pool.adjust_messages[index].used = true;
            pos = index + 1;
            return &adjust_message_pool.adjust_messages[index];
        }
    }
    return NULL;
}

/* Get the instance status with queue number */
const char *
get_instance_status_name(uint16_t id)
{
    static char buffer[sizeof(INSTANCE_STATUS_NAME) + 2];
    snprintf(buffer, sizeof(buffer) - 1, INSTANCE_STATUS_NAME, id);
    return buffer;
}

/* Get the rx queue name with queue number */
const char *
get_rx_queue_name(uint16_t id)
{
    static char buffer[sizeof(INSTANCE_RXQ_NAME) + 2];
    snprintf(buffer, sizeof(buffer) - 1, INSTANCE_RXQ_NAME, id);
    return buffer;
}

/* Get the rx queue name with queue number */
const char *
get_tx_queue_name(uint16_t id)
{
    static char buffer[sizeof(INSTANCE_TXQ_NAME) + 2];
    snprintf(buffer, sizeof(buffer) - 1, INSTANCE_TXQ_NAME, id);
    return buffer;
}

/* Get the lock name with queue number */
const char *
get_lock_name(uint16_t id)
{
    static char buffer[sizeof(INSTANCE_LOCK_NAME) + 2];
    snprintf(buffer, sizeof(buffer) - 1, INSTANCE_LOCK_NAME, id);
    return buffer;
}

/* Init deliver pool for dispatcher and worker */
void
deliver_pool_init(struct deliver_pool_t *deliver_pool)
{
    uint16_t i;



    for (i = 0; i < QUEUE_NUM; i++) {
        deliver_pool->instance_status[i] = memzone_reserve(get_instance_status_name(i), sizeof(struct instance_status_t), 1);
        deliver_pool->instance_queue[i].rx_queue = queue_init(get_rx_queue_name(i), QUEUE_SIZE);
        deliver_pool->instance_queue[i].tx_queue = queue_init(get_tx_queue_name(i), QUEUE_SIZE);
        deliver_pool->instance_queue[i].rte_spinlock = memzone_reserve(get_lock_name(i), sizeof(rte_spinlock_t), 1);

        deliver_pool->instance_queue_info[i].tx = 0;
        deliver_pool->instance_queue_info[i].tx_drop = 0;
        deliver_pool->instance_queue_info[i].rx = 0;

        deliver_pool->batch_cnt[i] = 0;
    }

    deliver_pool->manager_queue.adjust_queue = queue_init(MANAGER_ADJUSTQ_NAME, QUEUE_SIZE);
    deliver_pool->manager_queue.rx_queue = queue_init(MANAGER_RXQ_NAME, QUEUE_SIZE);
    deliver_pool->manager_queue.tx_queue = queue_init(MANAGER_TXQ_NAME, QUEUE_SIZE);
    deliver_pool->manager_queue.rte_spinlock = memzone_reserve(MANAGER_LOCK_NAME, sizeof(rte_spinlock_t), 1);
    deliver_pool->manager_queue.rx_nb = 0;
}

/* Init deliver table for dispatcher and worker */
void
deliver_table_init(struct deliver_table_t *deliver_table)
{
    uint16_t i;

    deliver_table->used_queue_num = 0;
    for (i = 0; i < QUEUE_NUM; i++) {
        deliver_table->used_queue[i] = -1;
        deliver_table->queue_tag[i] = false;
    }
    for (i = 0; i < FUNC_NUM; i++) {
        deliver_table->function_instances[i] = 0;
    }
}

/* Display the queues info of pool */
void
display_deliver_info(unsigned difftime, struct deliver_table_t *deliver_table, struct deliver_pool_t *deliver_pool)
{
    uint16_t i;
    int queue_id;
    static uint64_t manager_tx_last;
    static uint64_t manager_tx_drop_last;
    static uint64_t manager_rx_last;
    static uint64_t instnace_tx_last[QUEUE_NUM];
    static uint64_t instance_tx_drop_last[QUEUE_NUM];
    static uint64_t instance_rx_last[QUEUE_NUM];

    printf("Pool info\n");
    printf("-----Manager\n");
    printf("Queue - tx: %9"PRIu64"  (%9"PRIu64" pps)\t"
           "tx_drop: %9"PRIu64"  (%9"PRIu64" pps)\t"
           "rx: %9"PRIu64"  (%9"PRIu64" pps)\n",
           deliver_pool->manager_queue_info.tx,
           (deliver_pool->manager_queue_info.tx - manager_tx_last) / difftime,
           deliver_pool->manager_queue_info.tx_drop,
           (deliver_pool->manager_queue_info.tx_drop - manager_tx_drop_last) / difftime,
           deliver_pool->manager_queue_info.rx,
           (deliver_pool->manager_queue_info.rx - manager_rx_last) / difftime);

    manager_tx_last = deliver_pool->manager_queue_info.tx;
    manager_tx_drop_last = deliver_pool->manager_queue_info.tx_drop;
    manager_rx_last = deliver_pool->manager_queue_info.rx;

    printf("-----Instance\n");
    for (i = 0; i < deliver_table->used_queue_num; i++) {
        queue_id = deliver_table->used_queue[i];
        printf("Queue %u - tx: %9"PRIu64"  (%9"PRIu64" pps)\t"
               "tx_drop: %9"PRIu64"  (%9"PRIu64" pps)\t"
               "rx: %9"PRIu64"  (%9"PRIu64" pps)\n", i,
               deliver_pool->instance_queue_info[queue_id].tx,
               (deliver_pool->instance_queue_info[queue_id].tx - instnace_tx_last[queue_id]) / difftime,
               deliver_pool->instance_queue_info[queue_id].tx_drop,
               (deliver_pool->instance_queue_info[queue_id].tx_drop - instance_tx_drop_last[queue_id]) / difftime,
               deliver_pool->instance_queue_info[queue_id].rx,
               (deliver_pool->instance_queue_info[queue_id].rx - instance_rx_last[queue_id]) / difftime);

        instnace_tx_last[queue_id] = deliver_pool->instance_queue_info[queue_id].tx;
        instance_tx_drop_last[queue_id] = deliver_pool->instance_queue_info[queue_id].tx_drop;
        instance_rx_last[queue_id] = deliver_pool->instance_queue_info[queue_id].rx;
    }
}
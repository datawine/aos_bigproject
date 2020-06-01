//
// Created by hy on 19-3-17.
//

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <sys/time.h>
#include "dispatcher.h"

static void
notify_instance_overload(int queue_id) {
    struct adjust_message_t *adjust_message;
    int ret;

    adjust_message = get_available_adjust_message();
    if (adjust_message == NULL) {
        RTE_LOG(INFO, DISPATCHER, "Can not get available adjust message\n");
        return;
    }
    adjust_message->queue_id = queue_id;

    ret = rte_ring_enqueue(deliver_pool.manager_queue.adjust_queue, (void *)adjust_message);
    if (ret != 0) {
        adjust_message->used = false;
        RTE_LOG(INFO, DISPATCHER, "Can not enqueue adjust message\n");
    }
}

static void
wakeup_instance_judge(int queue_id) {
    if (deliver_pool.instance_status[queue_id]->status == FUNC_RUNNING)
        return;

    /* TODO: instances wakeup strategy */

    /* nofity instance with 4 accumulated batches */
    deliver_pool.batch_cnt[queue_id]++;
    if (deliver_pool.batch_cnt[queue_id] == 4) {
        kill(deliver_pool.instance_status[queue_id]->pid, SIGCONT);
        deliver_pool.batch_cnt[queue_id] = 0;
    }
}

void
init_dispatcher_buffer(void)
{
    uint16_t i;

    for (i = 0; i < QUEUE_NUM; i++) {
        dispatcher_buffer[i].num = 0;
    }
}

void
dispatcher(struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
    uint16_t i;
    uint32_t function_tag, ret, pkts_num = 0;
    struct rte_mbuf *pkts[DISPATCHER_BUFFER_PKTS];
    int queue_id;

    for (i = 0; i < nb_pkts; i++) {
        function_tag = *get_function_tag(rx_pkts[i]);

        if (deliver_table.function_instances[function_tag] == 0) {
            pkts[pkts_num ++] = rx_pkts[i];
            deliver_pool.manager_queue_info.rx += 1;
            if (pkts_num == DISPATCHER_BUFFER_PKTS) {
                printf("send a new burst to manager enqueue\n");

                ret = rte_ring_sp_enqueue_burst(deliver_pool.manager_queue.rx_queue, (void *)pkts,
                        DISPATCHER_BUFFER_PKTS, NULL);

                pkts_num = 0;
                deliver_pool.manager_queue_info.tx += ret;
                if (ret < DISPATCHER_BUFFER_PKTS) {
                    deliver_pool.manager_queue_info.tx_drop += (DISPATCHER_BUFFER_PKTS - ret);
                    pktmbuf_free_bulk(&(pkts[ret]), (DISPATCHER_BUFFER_PKTS - ret));
                }
            }
        } else {
            queue_id = deliver_table.function_to_queue[function_tag][rx_pkts[i]->hash.rss % deliver_table.function_instances[function_tag]];

            dispatcher_buffer[queue_id].buffer[dispatcher_buffer[queue_id].num++] = rx_pkts[i];
            if (dispatcher_buffer[queue_id].num == DISPATCHER_BUFFER_PKTS) {

                ret = rte_ring_sp_enqueue_burst(deliver_pool.instance_queue[queue_id].rx_queue, (
                        void *)dispatcher_buffer[queue_id].buffer, DISPATCHER_BUFFER_PKTS, NULL);

                wakeup_instance_judge(queue_id);

                dispatcher_buffer[queue_id].num = 0;
                deliver_pool.instance_queue_info[queue_id].tx += ret;
                if (ret < DISPATCHER_BUFFER_PKTS) {
                    notify_instance_overload(queue_id);
                    deliver_pool.instance_queue_info[queue_id].tx_drop += (DISPATCHER_BUFFER_PKTS - ret);
                    pktmbuf_free_bulk(&(dispatcher_buffer[queue_id].buffer[ret]), (DISPATCHER_BUFFER_PKTS - ret));
                }
            }
        }
    }
}

uint16_t
aggregator(struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
    static uint16_t pos = 0;
    uint16_t i, j, ret, queue_num, index;

    queue_num = deliver_table.used_queue_num;
    for (i = 0 ; i < queue_num; i++) {
        index = (pos + i) % queue_num;
        j = deliver_table.used_queue[index];
        ret = rte_ring_sc_dequeue_burst(deliver_pool.instance_queue[j].tx_queue, (
                                        void *)tx_pkts, nb_pkts, NULL);

        deliver_pool.instance_queue_info[j].rx += ret;
        pos = i + 1;
        if (unlikely(ret == 0))
            continue;
        return ret;
    }
    return 0;
}
//
// Created by hy on 19-3-19.
//

#include <gateway.h>
#include "gateway.h"

static uint16_t classifier_table[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint16_t function_table[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
static uint8_t func_flag = 0;

void
gateway_classify(struct rte_mbuf **pkts, uint32_t num)
{
    uint32_t i, j;
    uint32_t *function_tag, *function_index;
    uint16_t *port_tag, *executor_tag;

    for (i = 0; i < num; i++) {
        rte_prefetch0(rte_pktmbuf_mtod(pkts[i], void *));
        rte_prefetch0((void *)pkts[i]->udata64);
/*        if (pkts[i]->port == 0) {
            function_tag = get_function_tag(pkts[i]);
            port_tag = get_port_tag(pkts[i]);
            executor_tag = get_executor_tag(pkts[i]);
            *function_tag = 0;
            *port_tag = 1;
            *executor_tag = 1;
        } else {
            port_tag = get_port_tag(pkts[i]);
            *port_tag = 0;
        }
*/
        if (pkts[i]->port == 0) {
            function_tag = get_function_tag(pkts[i]);
            port_tag = get_port_tag(pkts[i]);
            executor_tag = get_executor_tag(pkts[i]);
//            *function_tag = func_flag;
//            if (func_flag == 0)
//                func_flag = 1;
//            else if (func_flag == 1)
//                func_flag = 2;
//            else if (func_flag == 2)
//                func_flag = 3;
//            else if (func_flag == 3)
//                func_flag = 4;
//            else
//                func_flag = 0;
            *function_tag = 0;
            *port_tag = 1;
            *executor_tag = 1;
        } else {
            function_tag = get_function_tag(pkts[i]);
            executor_tag = get_executor_tag(pkts[i]);
            port_tag = get_port_tag(pkts[i]);

            *function_tag = function_table[*function_tag];
            *executor_tag = classifier_table[*function_tag];
//            printf("function_tag: %u, executor_tag: %u\n", *function_tag, *executor_tag);

            if (*executor_tag == 0) {
                *port_tag = 0;
            } else {
                *port_tag = 1;
            }
        }
    }

}
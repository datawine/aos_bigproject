//
// Created by hy on 19-3-19.
//

#include "gateway.h"

void
gateway_classify(struct rte_mbuf **pkts, uint32_t num)
{
    uint32_t i;
    uint32_t *function_tag;
    uint16_t *port_tag, *executor_tag;

    for (i = 0; i < num; i++) {
        rte_prefetch0(rte_pktmbuf_mtod(pkts[i], void *));
        if (pkts[i]->port == 0) {
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
    }
}
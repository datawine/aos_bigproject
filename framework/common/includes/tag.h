//
// Created by hy on 19-3-19.
//

#ifndef SERVERLESS_NFV_TAG_H
#define SERVERLESS_NFV_TAG_H

#include <stdint.h>

#include <rte_mbuf.h>

struct pkt_tag {
    uint32_t function_tag;
    uint16_t port_tag;
    uint16_t executor_tag;
};

uint32_t*
get_function_tag(struct rte_mbuf *pkt);

uint16_t*
get_port_tag(struct rte_mbuf *pkt);

uint16_t*
get_executor_tag(struct rte_mbuf *pkt);

#endif //SERVERLESS_NFV_TAG_H

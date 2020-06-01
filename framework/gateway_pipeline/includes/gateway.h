//
// Created by hy on 19-3-19.
//

#ifndef SERVERLESS_NFV_GATEWAY_H
#define SERVERLESS_NFV_GATEWAY_H

#include <stdint.h>

#include <rte_mbuf.h>

#include "tag.h"

void
gateway_classify(struct rte_mbuf **pkts, uint32_t num);

#endif //SERVERLESS_NFV_GATEWAY_H

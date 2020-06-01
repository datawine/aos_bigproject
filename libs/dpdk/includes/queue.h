//
// Created by hy on 19-3-17.
//

#ifndef SERVERLESS_NFV_QUEUE_H
#define SERVERLESS_NFV_QUEUE_H

#include <rte_ring.h>

/* init a queue with queue_size packets  */
struct rte_ring *
queue_init(const char *queue_name, uint32_t queue_size);

/* lookup queue through queue_name */
struct rte_ring *
queue_lookup(const char *queue_name);

#endif //SERVERLESS_NFV_QUEUE_H

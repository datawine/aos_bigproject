//
// Created by hy on 19-3-23.
//

#ifndef SERVERLESS_NFV_FUNCWORKER_1_H
#define SERVERLESS_NFV_FUNCWORKER_1_H

#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <signal.h>

#include <rte_mbuf.h>

#include "mempool.h"

#include "funcbox_1.h"

#define MAX_PKT_BURST 32
#define FUNC_IDLE 0
#define FUNC_EXIST 1
#define FUNC_SLEEPING 2
#define FUNC_RUNNING 3

#define RTE_LOGTYPE_FUNCBOX          RTE_LOGTYPE_USER3

/* start worker */
int
func_worker_start_with_load(char *request, struct instance_channel_t *instance_channel);

#endif //SERVERLESS_NFV_FUNCWORKER_1_H

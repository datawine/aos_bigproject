//
// Created by hy on 19-3-23.
//

#ifndef SERVERLESS_NFV_FUNCWORKER_0_H
#define SERVERLESS_NFV_FUNCWORKER_0_H

#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <signal.h>

#include <rte_mbuf.h>

#include "mempool.h"

#include "funcbox_0.h"


#define MAX_PKT_BURST 32
#define FUNC_IDLE 0
#define FUNC_EXIST 1
#define FUNC_SLEEPING 2
#define FUNC_RUNNING 3

/* init worker */
int
func_worker_init(int argc, char *argv[]);

/* run worker */
int
func_worker_run(void);

#endif //SERVERLESS_NFV_FUNCWORKER_0_H

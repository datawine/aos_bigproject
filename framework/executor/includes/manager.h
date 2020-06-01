//
// Created by hy on 19-3-18.
//

#ifndef SERVERLESS_NFV_MANAGER_H
#define SERVERLESS_NFV_MANAGER_H

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <rte_log.h>

#include "common.h"


extern struct deliver_pool_t deliver_pool;
extern struct deliver_table_t deliver_table;

/* Manager to accept and launch function */
int
manager_thread(void *arg);

#endif //SERVERLESS_NFV_MANAGER_H

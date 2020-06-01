//
// Created by hy on 2019/8/19.
//

#ifndef SERVERLESS_NFV_INSTANCE_H
#define SERVERLESS_NFV_INSTANCE_H

#include <stdint.h>
#include <signal.h>

struct instance_status_t {
    pid_t pid;
    uint8_t status;
};

struct instance_channel_t {
    int mgr_to_ins_pipe[2];
    int ins_to_mgr_pipe[2];
};

#endif //SERVERLESS_NFV_INSTANCE_H

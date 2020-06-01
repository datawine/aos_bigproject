//
// Created by hy on 19-3-17.
//
#include <unistd.h>

#include <rte_log.h>

#include "instance.h"

#include "funcbox_2.h"
#include "funcworker_2.h"

#define PIPE_BUFFER_LEN 4096

int
launch_funcbox_with_pool(struct instance_channel_t *instance_channel) {
    pid_t pid;
    char buffer[PIPE_BUFFER_LEN];
    int ret;

    pid = fork();
    if (pid < 0) {
        rte_exit(EXIT_FAILURE, "Can not fork new instance\n");
    } else if (pid > 0) {
        //parent process
        close(instance_channel->mgr_to_ins_pipe[0]);
        close(instance_channel->ins_to_mgr_pipe[1]);
        return pid;
    }

    close(instance_channel->mgr_to_ins_pipe[1]);
    close(instance_channel->ins_to_mgr_pipe[0]);
    ret = read(instance_channel->mgr_to_ins_pipe[0], buffer, sizeof(buffer));
    if (ret == 0)
        return 0;
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Instance receive request fail\n");
    }

    func_worker_start_with_pool(buffer, instance_channel);

    printf("Funcbox exited\n");
    return 0;
}


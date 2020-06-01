//
// Created by hy on 19-8-9.
//
#define _GNU_SOURCE
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <dlfcn.h>
#include <sys/shm.h>
#include <sys/mman.h>

#include <rte_log.h>

#include "instance.h"
#include "list.h"
#include "tag.h"
#include "utilization.h"
#include "funcbox_1.h"
#include "funcbox_2.h"
#include "manager.h"

#define FUNC_NAME_LEN 64
#define SOCK_LISTEN_LEN 1024
#define SOCK_BUFFER_LEN 4096
#define MAX_PKT_BURST 32
#define MAX_MSG_BURST 32
#define CPU_UPDATE_PERIOD 1000 * 1000 * 1000 //1s
#define MANAGER_SOCK_ADDR "/var/tmp/manager_server.sock"
#define FUNC_PATH "/home/amax/projects/serverless_nfv/framework/funcboxes/funcbox_0/build/app/funcbox_0"
#define NFSLIB_PATH "../target/nfs/libnfs.so"
#define PAGE_SIZE 4096
#define SHM_SIZE PAGE_SIZE * 10
#define SHM_MODE 0600

struct function_table_t {
    char function_name[FUNC_NUM][FUNC_NAME_LEN];
    int function_status[FUNC_NUM];
    int function_to_queue[FUNC_NUM];

};

struct function_table_t function_table;

struct instance_info_t {
    uint32_t function_tag;
    pid_t pid;
    int core_id;
};

struct instances_info_t {
    uint16_t instance_num;
    struct instance_info_t instance_infos[INSTANCE_NUM];
    struct instance_channel_t instance_channel[INSTANCE_NUM];
};

struct instances_info_t instances_info;

struct core_instance_list_t {
    int instance_id;
    struct list_head list;
};

struct core_info_t {
    uint16_t instance_num;
    struct core_instance_list_t core_instance_head;

};

struct cores_info_t {
    int core_group[CORE_NUM];
    struct core_info_t core_infos[CORE_NUM];
};

struct cores_info_t cores_info = {
    .core_group = {17, 18, 19, 20}
};

struct cpu_utilization_t cpus_utilization[CORE_NUM];
sigset_t set;

struct pool_addr_table_t {
    void *function_addr[FUNC_NUM];
    int func_offset[FUNC_NUM][2];
    key_t shm_key[FUNC_NUM];
    int shmid[FUNC_NUM];
    int nflib_length[FUNC_NUM];
};

struct pool_addr_table_t pool_addr_table;

static int
get_next_core(void) {
    /* TODO: choose the next core according to cpu utilization */
    static uint16_t prev_core_index = 0;

    return cores_info.core_group[prev_core_index++];
}

static int
manager_socket_init(void) {
    int listen_fd;
    struct sockaddr_un addr;

    if ((listen_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        rte_exit(EXIT_FAILURE, "Can not init manager socket\n");
    }

    //bind
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MANAGER_SOCK_ADDR, sizeof(MANAGER_SOCK_ADDR));
    unlink(MANAGER_SOCK_ADDR);
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        rte_exit(EXIT_FAILURE, "Can not bind manager socket\n");
    }

    //listen
    if (listen(listen_fd, SOCK_LISTEN_LEN) < 0) {
        rte_exit(EXIT_FAILURE, "Can not listen manager socket\n");
    }

    RTE_LOG(INFO, MANAGER, "Init socket success, fd: %d\n", listen_fd);

    return listen_fd;
}

static int
pool_load_single_func(uint32_t function_tag) {
    void *lib_handler, *begin_addr, *end_addr, *func_addr;
    int ret, func_offset;
    Dl_info dl_info;

    lib_handler = dlopen(NFSLIB_PATH, RTLD_NOW | RTLD_LOCAL);
    if (!lib_handler) {
        rte_exit(EXIT_FAILURE, "Dlopen error: %s\n", dlerror());
    }

    char init_func_name[64], handler_func_name[64], end_func_name[64];
    snprintf(init_func_name, 63, "func_%s_local_init", function_table.function_name[function_tag]);
    snprintf(handler_func_name, 63, "func_%s_local_handler", function_table.function_name[function_tag]);
    snprintf(end_func_name, 63, "%s_end", function_table.function_name[function_tag]);
    func_addr = dlsym(lib_handler, init_func_name);
    if (dlerror() != NULL) {
        printf("dlsym func_addr error: %s\n", dlerror());
        return -1;
    }

    ret = dladdr(func_addr, &dl_info);
    if (!ret) {
        printf("dladdr error: %s\n", dlerror());
        return -1;
    }
    begin_addr = dl_info.dli_fbase;
    func_offset = (long)func_addr - (long)begin_addr;
    pool_addr_table.func_offset[function_tag][0] = func_offset;

    printf("begin addr: %ld\n", (long)begin_addr);
    printf("init addr: %ld init offset: %d\n", (long)func_addr, func_offset);

    end_addr = dlsym(lib_handler, end_func_name);
    if (dlerror() != NULL) {
        printf("dlsym end_addr error: %s\n", dlerror());
        return -1;
    }

    func_addr = dlsym(lib_handler, handler_func_name);
    func_offset = (long)func_addr - (long)begin_addr;
    pool_addr_table.func_offset[function_tag][1] = func_offset;

    printf("handler addr: %ld, handler offset: %d\n", (long)func_addr, func_offset);
    printf("end addr: %ld\n", (long)end_addr);
    printf("copy init size: %ld\n", (long)end_addr - (long)begin_addr);

    int shmid, lib_length;
    void *shmptr;
    key_t key;
    lib_length = (((long)end_addr - (long)begin_addr) / PAGE_SIZE + 1) * PAGE_SIZE;
    printf("lib_length: %d\n", lib_length);

    if ((key = ftok("/dev/null", function_tag)) < 0) {
        printf("ftok error\n");
        return -1;
    }
    if ((shmid = shmget(key, lib_length, SHM_MODE|IPC_CREAT)) < 0) {
        printf("shmget error and cannot remove\n");
        return -1;
    }
    if ((shmptr = shmat(shmid, 0, 0)) == (void *)-1) {
        printf("shmat error\n");
        return -1;
    }

    if (mprotect(shmptr, lib_length, PROT_READ|PROT_WRITE|PROT_EXEC) == -1) {
        printf("mprotect error\n");
        return -1;
    }
    memcpy(shmptr, begin_addr, (long)end_addr - (long)begin_addr);

    pool_addr_table.function_addr[function_tag] = shmptr;
    pool_addr_table.shm_key[function_tag] = key;
    pool_addr_table.shmid[function_tag] = shmid;
    pool_addr_table.nflib_length[function_tag] = lib_length;
    printf("shmptr: %ld, key: %ld, shmid: %d\n", (long)shmptr, (long)key, shmid);
}

static int
manager_socket_handle(char *request) {
    char *function_char_tag, *function_name;
    uint32_t function_tag;

    function_name = strtok(request, " ,.-");
    function_char_tag = strtok(NULL, " ,.-");

    function_tag = strtol(function_char_tag, NULL, 10);

    printf("function name: %s, function tag: %u\n", function_name, function_tag);
    strcpy(function_table.function_name[function_tag], function_name);

    pool_load_single_func(function_tag);

    return 0;
}

static void
manager_socket_run(void *arg) {
    int listen_fd, accept_fd, recv_len, write_len, handle_result;
    char buffer_recv[SOCK_BUFFER_LEN] = {0};
    char buffer_write[SOCK_BUFFER_LEN] = {"Handle success!"};

    listen_fd = *(int *)arg;

    RTE_LOG(INFO, MANAGER, "Running manager socket thread, fd: %d\n", listen_fd);

    while (keep_running) {
        //accept
        accept_fd = accept(listen_fd, NULL, NULL);
        if (accept_fd < 0) {
            RTE_LOG(INFO, MANAGER, "Accept failure\n");
            continue;
        }

        while (keep_running) {
            RTE_LOG(INFO, MANAGER, "Accept client socket: %d\n", accept_fd);
            recv_len = recv(accept_fd, buffer_recv, SOCK_BUFFER_LEN, 0);
            if (recv_len < 0) {
                RTE_LOG(INFO, MANAGER, "Recv failure\n");
            } else if (recv_len == 0) {
                RTE_LOG(INFO, MANAGER, "Close client socket: %d\n", accept_fd);
                close(accept_fd);
                break;
            } else {
                //handle client request
                handle_result = manager_socket_handle(buffer_recv);
                write_len = write(accept_fd, buffer_write, sizeof(buffer_write));
                if (write_len < 0) {
                    RTE_LOG(INFO, MANAGER, "Write failure\n");
                }
            }
        }
    }
}

static void
manager_socket_thread(void) {
    pthread_t tid;
    int sock_fd, ret;

    sock_fd = manager_socket_init();

    ret = pthread_create(&tid, NULL, (void *)&manager_socket_run, (void *)&sock_fd);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Pthread create failure\n");
    }
    ret = pthread_detach(tid);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Pthread detach failure\n");
    }
}

static void
manager_instance_init_with_pool(void) {
    uint16_t i;
    pid_t pid;

    for (i = 0; i < FUNC_NUM; i++) {
        if (pipe(instances_info.instance_channel[i].mgr_to_ins_pipe)) {
            RTE_LOG(INFO, MANAGER, "pipe init error\n");
            return;
        }
        if (pipe(instances_info.instance_channel[i].ins_to_mgr_pipe)) {
            RTE_LOG(INFO, MANAGER, "pipe init error\n");
            return;
        }
        pid = launch_funcbox_with_pool(&instances_info.instance_channel[i]);
        instances_info.instance_infos[i].pid = pid;
    }
}

static void
manager_instance_init_with_load(void) {
    uint16_t i;
    pid_t pid;

    for (i = 0; i < FUNC_NUM; i++) {
        if (pipe(instances_info.instance_channel[i].mgr_to_ins_pipe)) {
            RTE_LOG(INFO, MANAGER, "pipe init error\n");
            return;
        }
        if (pipe(instances_info.instance_channel[i].ins_to_mgr_pipe)) {
            RTE_LOG(INFO, MANAGER, "pipe init error\n");
            return;
        }
        pid = launch_funcbox_with_load(&instances_info.instance_channel[i]);
        instances_info.instance_infos[i].pid = pid;
    }

}

static void
manager_instance_init_on_demand(void) {
    uint16_t i;
    for (i = 0; i < FUNC_NUM; i++) {
        if (pipe(instances_info.instance_channel[i].mgr_to_ins_pipe) < 0) {
            RTE_LOG(INFO, MANAGER, "pipe init error\n");
            return;
        }
        if (pipe(instances_info.instance_channel[i].ins_to_mgr_pipe)) {
            RTE_LOG(INFO, MANAGER, "pipe init error\n");
            return;
        }
    }
}

static int
manager_launch_init(void) {
    uint16_t i;

    /* init function table */
    for (i = 0; i < FUNC_NUM; i++) {
        function_table.function_to_queue[i] = -1;
        function_table.function_status[i] = FUNC_IDLE;
    }

    /* init instances info */
    instances_info.instance_num = 0;
    for (i = 0; i < INSTANCE_NUM; i++) {
        instances_info.instance_infos[i].core_id = -1;
    }

    /* init cores info */
    for (i = 0; i < CORE_NUM; i++) {
        cores_info.core_infos[i].instance_num = 0;
        INIT_LIST_HEAD(&(cores_info.core_infos[i].core_instance_head.list));
    }

    /* pre-launch instances for load and pool */
//    manager_instance_init_on_demand();
//    manager_instance_init_with_load();
    manager_instance_init_with_pool();

    return 0;
}

static void
launch_instance_on_demand(uint32_t function_tag, uint16_t queue_id) {
    const char *function_name, *rx_queue, *tx_queue, *instance_status;
    char core_char_id[4], function_path[sizeof(FUNC_PATH) + 60];
    int sig, ret, core_id, instance_id;
    pid_t pid;
    struct core_instance_list_t *core_instance_node;

    function_name = function_table.function_name[function_tag];
    instance_status = get_instance_status_name(queue_id);
    rx_queue = get_rx_queue_name(queue_id);
    tx_queue = get_tx_queue_name(queue_id);
    core_id = get_next_core();
    sprintf(core_char_id, "%d", core_id);

    /* one to one map from instance id to queue id */
    instance_id = queue_id;

    pid = fork();
    if (pid < 0) {
        rte_exit(EXIT_FAILURE, "Can not fork new instance\n");
    } else if (pid == 0) { /* child */
        close(instances_info.instance_channel[instance_id].ins_to_mgr_pipe[0]);
        close(instances_info.instance_channel[instance_id].mgr_to_ins_pipe[1]);
        char input_pipestr[64], output_pipestr[64];
        snprintf(input_pipestr, sizeof(input_pipestr), "%d",
                instances_info.instance_channel[instance_id].mgr_to_ins_pipe[0]);
        snprintf(input_pipestr, sizeof(input_pipestr), "%d",
                instances_info.instance_channel[instance_id].ins_to_mgr_pipe[1]);
        ret = execl(FUNC_PATH, "funcbox_0",
                "-l", core_char_id, "-n", "4", "--proc-type=secondary",
                "--", "-r", rx_queue, "-t", tx_queue, "-s", instance_status,
                "-i", input_pipestr, // mgr->ins read end
                "-o", output_pipestr, // ins->mgr write end
                (char *)0);
        if (ret < 0) {
            printf("errono: %u\n", errno);
            rte_exit(EXIT_FAILURE, "Can not execl new function\n");
        }
        exit(0);
    } else { /* parent */
        close(instances_info.instance_channel[instance_id].mgr_to_ins_pipe[0]);
        close(instances_info.instance_channel[instance_id].ins_to_mgr_pipe[1]);

        char start_str[16];
        int n = 0;
        while (n == 0) {
            n = read(instances_info.instance_channel[instance_id].ins_to_mgr_pipe[0], start_str, 16);
        }
    }

    /* update instances status */
    instances_info.instance_num++;
    instances_info.instance_infos[instance_id].function_tag = function_tag;
    instances_info.instance_infos[instance_id].pid = pid;
    instances_info.instance_infos[instance_id].core_id = core_id;

    /* update cores status */
    cores_info.core_infos[core_id].instance_num++;
    core_instance_node = malloc(sizeof(struct core_instance_list_t));
    core_instance_node->instance_id = instance_id;
    list_add_tail(&(core_instance_node->list), &(cores_info.core_infos[core_id].core_instance_head.list));

}

static void
launch_instance_with_load(uint32_t function_tag, uint16_t queue_id) {
    printf("start to launch\n");
    const char *function_name, *rx_queue, *tx_queue, *instance_status;
    char core_char_id[4], func_args[128], running_sig;
    int core_id, instance_id;
    struct core_instance_list_t *core_instance_node;

    function_name = function_table.function_name[function_tag];
    instance_status = get_instance_status_name(queue_id);
    rx_queue = get_rx_queue_name(queue_id);
    tx_queue = get_tx_queue_name(queue_id);
    core_id = get_next_core();
    sprintf(core_char_id, "%d", core_id);

    /* one to one map from instance id to queue id */
    instance_id = queue_id;

    snprintf(func_args, 127, "%s %s %s %s %s", function_name, rx_queue, tx_queue, instance_status, core_char_id);

    printf("write to %d core char id: %s\n", instance_id, core_char_id);
    write(instances_info.instance_channel[instance_id].mgr_to_ins_pipe[1], func_args, sizeof(func_args));

    /* wait for running signal from instance */
    read(instances_info.instance_channel[instance_id].ins_to_mgr_pipe[0], &running_sig, sizeof(char));

    /* update instances status */
    instances_info.instance_num++;
    instances_info.instance_infos[instance_id].function_tag = function_tag;
    instances_info.instance_infos[instance_id].core_id = core_id;

    /* update cores status */
    cores_info.core_infos[core_id].instance_num++;
    core_instance_node = malloc(sizeof(struct core_instance_list_t));
    core_instance_node->instance_id = instance_id;
    list_add_tail(&(core_instance_node->list), &(cores_info.core_infos[core_id].core_instance_head.list));
}

static void
launch_instance_with_pool(uint32_t function_tag, uint16_t queue_id) {
    const char *function_name, *rx_queue, *tx_queue, *instance_status;
    char core_char_id[4], func_args[128], running_sig;
    int sig, ret, core_id, instance_id;
    pid_t pid;
    struct core_instance_list_t *core_instance_node;

    function_name = function_table.function_name[function_tag];
    instance_status = get_instance_status_name(queue_id);
    rx_queue = get_rx_queue_name(queue_id);
    tx_queue = get_tx_queue_name(queue_id);
    core_id = get_next_core();
    sprintf(core_char_id, "%d", core_id);

    /* one to one map from instance to queue */
    instance_id = queue_id;

    snprintf(func_args, 127, "%s %s %s %s %s %d %d %d %d", function_name, rx_queue, tx_queue, instance_status,
             core_char_id, pool_addr_table.shmid[function_tag], pool_addr_table.func_offset[function_tag][0],
             pool_addr_table.func_offset[function_tag][1], pool_addr_table.nflib_length[function_tag]);

    write(instances_info.instance_channel[instance_id].mgr_to_ins_pipe[1], func_args, sizeof(func_args));

    /* wait for running signal from instance */
    read(instances_info.instance_channel[instance_id].ins_to_mgr_pipe[0], &running_sig, sizeof(char));

    /* update instances status */
    instances_info.instance_num++;
    instances_info.instance_infos[instance_id].function_tag = function_tag;
    instances_info.instance_infos[instance_id].core_id = core_id;

    /* update cores status */
    cores_info.core_infos[core_id].instance_num++;
    core_instance_node = malloc(sizeof(struct core_instance_list_t));
    core_instance_node->instance_id = instance_id;
    list_add_tail(&(core_instance_node->list), &(cores_info.core_infos[core_id].core_instance_head.list));
}

static void
manager_launch_single_instance(uint32_t function_tag) {
    uint16_t i;

    for (i = 0; i < QUEUE_NUM; i++) {
        if (deliver_table.queue_tag[i] == false) {
            deliver_table.queue_tag[i] = true;
            deliver_table.used_queue[deliver_table.used_queue_num++] = i;
            deliver_table.function_to_queue[function_tag][deliver_table.function_instances[function_tag]++] = i;
            break;
        }
    }

    /* various ways to launch instance */
//    launch_instance_on_demand(function_tag, i);
//    launch_instance_with_load(function_tag, i);
    launch_instance_with_pool(function_tag, i);

    function_table.function_status[function_tag] = FUNC_EXIST;
    function_table.function_to_queue[function_tag] = i;

}

static void
manager_launch_handle(struct rte_mbuf **pkts, uint16_t nb_pkts) {
    int queue_id;
    uint16_t i;
    uint32_t function_tag, ret;

    for (i = 0; i < nb_pkts; i++) {
        function_tag = *get_function_tag(pkts[i]);
        if (function_table.function_status[function_tag] == FUNC_IDLE) {
            //launch the function
            manager_launch_single_instance(function_tag);

        }
        queue_id = function_table.function_to_queue[function_tag];
        ret = rte_ring_sp_enqueue_burst(deliver_pool.instance_queue[queue_id].rx_queue, (void *)&pkts[i], 1, NULL);
        if (ret < 1) {
            rte_pktmbuf_free(pkts[i]);
            deliver_pool.manager_queue_info.tx_drop++;
        } else {
            deliver_pool.manager_queue_info.tx++;
        }
    }
}

static void
manager_migrate_some_instances(uint16_t core_id, uint16_t instance_id) {
    cpu_set_t cpu_info;
    uint16_t target_core_id;
    pid_t pid;
    struct core_info_t *core_info, *target_core_info;
    struct core_instance_list_t *var_instance, *next_instance;

    /* TODO: migrate instances according to cpu utilization */

    /* migrate the overload instance to the next launch core */
    core_info = &cores_info.core_infos[core_id];
    target_core_id = get_next_core();
    target_core_info = &cores_info.core_infos[target_core_id];
    pid = instances_info.instance_infos[instance_id].pid;

    /* remove from core info and add to target core info */
    list_for_each_entry_safe(var_instance, next_instance, &(core_info->core_instance_head.list), list) {
        if (var_instance->instance_id == instance_id) {
            list_del(&(var_instance->list));
            core_info->instance_num--;
            list_add_tail(&(var_instance->list), &(target_core_info->core_instance_head.list));
            target_core_info->instance_num++;
        }
    }
    instances_info.instance_infos[instance_id].core_id = target_core_id;
    CPU_ZERO(&cpu_info);
    CPU_SET(target_core_id, &cpu_info);
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_info) != 0) {
        RTE_LOG(INFO, MANAGER, "Process affinity setting failed for instance id %u on core id %u\n", instance_id, core_id);
    }


}

static void
manager_adjust_handle(struct adjust_message_t **adjust_messages, uint16_t nb_messages) {
    uint16_t i, instance_id, core_id;
    uint32_t function_tag;
    printf("adjust\n");

    for (i = 0; i < nb_messages; i++) {
        /* instance id is equal to queue id */
        instance_id = adjust_messages[i]->queue_id;
        core_id = instances_info.instance_infos[instance_id].core_id;

        if (cores_info.core_infos[core_id].instance_num == 1) {
            /* scale a new instance on other cores */
            RTE_LOG(INFO, MANAGER, "Scale instance id %u on core id %u\n", instance_id, core_id);
            function_tag = instances_info.instance_infos[instance_id].function_tag;
            manager_launch_single_instance(function_tag);
        } else {
            /* migrate some instances to other cores */
            RTE_LOG(INFO, MANAGER, "Migrate instance id %u on core id %u\n", instance_id, core_id);
            manager_migrate_some_instances(core_id, instance_id);
        }
    }
}

static void
manager_launch_run(void) {
    uint16_t nb_rx;
    struct timespec pre_time = {0, 0}, cur_time = {0, 0};

    struct rte_mbuf *pkts[MAX_PKT_BURST];
    struct adjust_message_t *adjust_messages[MAX_MSG_BURST];

    RTE_LOG(INFO, MANAGER, "Core %d: Running manager launch thread\n", rte_lcore_id());

    for (; keep_running;) {
        /* handle new arrived packets */
        nb_rx = rte_ring_sc_dequeue_burst(deliver_pool.manager_queue.rx_queue, (void *)pkts, MAX_PKT_BURST, NULL);
        if (unlikely(nb_rx == 0))
            continue;
        deliver_pool.manager_queue_info.rx += nb_rx;

        manager_launch_handle(pkts, nb_rx);

        /* handle load notify message */
        nb_rx = rte_ring_sc_dequeue_burst(deliver_pool.manager_queue.adjust_queue, (void *)adjust_messages, MAX_MSG_BURST, NULL);
        if (unlikely(nb_rx == 0))
            continue;
        deliver_pool.manager_queue_info.rx += nb_rx;

//        manager_adjust_handle(adjust_messages, nb_rx);

        /* update cpus utilization */
        clock_gettime(CLOCK_MONOTONIC_RAW, &cur_time);
        if ((cur_time.tv_sec * 1e9 + cur_time.tv_nsec) - (pre_time.tv_sec * 1e9 + pre_time.tv_nsec) > CPU_UPDATE_PERIOD) {
            update_cpus_utilization(cpus_utilization, CORE_NUM);
            pre_time.tv_sec = cur_time.tv_sec;
            pre_time.tv_nsec = cur_time.tv_nsec;
        }
    }

    RTE_LOG(INFO, MANAGER, "Core %d: manager launch thread done\n", rte_lcore_id());
}

static void
manager_launch_thread(void) {
    manager_launch_init();
    manager_launch_run();
}

/* Manager to accept and launch function */
int
manager_thread(void *arg) {
    manager_socket_thread();
    manager_launch_thread();

    return 0;
}

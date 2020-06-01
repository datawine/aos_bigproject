//
// Created by hy on 19-3-17.
//
#define _GNU_SOURCE
#include <unistd.h>
#include <dlfcn.h>
#include <sched.h>

#include "memzone.h"
#include "instance.h"

#include "funcworker_1.h"

#define LIB_PATH "../target/nfs/libnfs.so"

static char *function_name;

static char *instance_status_name;
static struct instance_status_t *instance_status;

static char *packet_rx_ring_name;
static struct rte_ring *packet_rx_ring;

static char *packet_tx_ring_name;
static struct rte_ring *packet_tx_ring;

static uint32_t core_id;

void *lib_handler;

FILE *fp;

static volatile bool nf_keep_running = 1;

static void
signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
        nf_keep_running = 0;
}

static int
func_parse_args(char *request) {
    char *core_id_char;

    function_name = strtok(request, " ,.-");
    packet_rx_ring_name = strtok(NULL, " ,.-");
    packet_tx_ring_name = strtok(NULL, " ,.-");
    instance_status_name = strtok(NULL, " ,.-");
    core_id_char = strtok(NULL, " ,.-");

    core_id = strtol(core_id_char, NULL, 10);

    return 0;
}

static int
func_worker_init(char *request) {
    cpu_set_t cpu_info;
    char function_init_name[64];
    void *func_addr;
    int (*func_exec)(void);

    if (func_parse_args(request) < 0)
        rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

    CPU_ZERO(&cpu_info);
    CPU_SET(core_id, &cpu_info);
    if (sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpu_info) != 0) {
        RTE_LOG(INFO, FUNCBOX, "Process affinity setting failed in funcbox on core id %u\n", core_id);
    }

    /* Get function init name */
//    snprintf(function_init_name, 63, "%s_init", function_name);
    snprintf(function_init_name, 63, "%s_init", "func_fw_local");

    /* Get the instance status */
    instance_status = (struct instance_status_t *)memzone_lookup(instance_status_name);

    /* Get the rings of worker */
    packet_rx_ring = rte_ring_lookup(packet_rx_ring_name);
    if (packet_rx_ring == NULL)
        rte_exit(EXIT_FAILURE, "Func-Worker gets rx_ring failed\n");
    packet_tx_ring = rte_ring_lookup(packet_tx_ring_name);
    if (packet_tx_ring == NULL)
        rte_exit(EXIT_FAILURE, "Func-Worker gets tx_ring failed\n");

    /* open lib and get initialize function */
    lib_handler = dlopen(LIB_PATH, RTLD_NOW | RTLD_LOCAL);
    if (!lib_handler) {
        rte_exit(EXIT_FAILURE, "Dlopen error: %s\n", dlerror());
    }

    func_addr = dlsym(lib_handler, function_init_name);
    if (dlerror() != NULL) {
        rte_exit(EXIT_FAILURE, "Dlsym error: %s\n", dlerror());
    }

    /* Initialize the nf runtime */
    func_exec = func_addr;
    (*func_exec)();

    return 0;
}

static int
func_worker_run(struct instance_channel_t *instance_channel) {
    struct rte_mbuf *rx_pkts[MAX_PKT_BURST], **in_pkts, **out_pkts = NULL;
    uint32_t nb_pkts, tx_nb, in_nb_pkts, out_nb_pkts;
    int sig;
    sigset_t set;
    char function_handler_name[64], running_sig = 0;
    void *func_addr;
    int (*func_exec)(struct rte_mbuf **in_pkts, uint32_t in_nb_pkts, struct rte_mbuf **out_pkts);

    printf("Func worker is running...\n");

    /* Listen for ^C and docker stop so we can exit gracefully */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if ((fp = fopen("/var/tmp/worker-log.txt", "a")) == NULL)
        printf("cannot open file!\n");

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGCONT);
    pthread_sigmask(SIG_SETMASK, &set, NULL);

    /* Get function handler name */
//    snprintf(function_handler_name, 63, "%s_handler", function_name);
    snprintf(function_handler_name, 63, "%s_handler", "func_fw_local");

    /* get handler function */
    func_addr = dlsym(lib_handler, function_handler_name);
    if (dlerror() != NULL) {
        rte_exit(EXIT_FAILURE, "Dlsym error: %s\n", dlerror());
    }

    func_exec = func_addr;

    instance_status->pid = getpid();
    instance_status->status = FUNC_RUNNING;

    /* send running signal to manager */
    write(instance_channel->ins_to_mgr_pipe[1], &running_sig, sizeof(char));

    while (nf_keep_running) {
        nb_pkts = rte_ring_dequeue_burst(packet_rx_ring, (void *)rx_pkts, MAX_PKT_BURST, NULL);

        if (unlikely(nb_pkts == 0)) {
            instance_status->status = FUNC_SLEEPING;
            sigwait(&set, &sig);
            if (sig == SIGINT || sig == SIGTERM) {
                nf_keep_running = 0;
            } else {
                instance_status->status = FUNC_RUNNING;
            }
            continue;
        }

        in_pkts = rx_pkts;
        out_pkts = in_pkts;
        in_nb_pkts = nb_pkts;

        out_nb_pkts = (*func_exec)(in_pkts, in_nb_pkts, out_pkts);

        if (likely(out_nb_pkts != 0)) {
            tx_nb = rte_ring_sp_enqueue_burst(packet_tx_ring, (void *)out_pkts, out_nb_pkts, NULL);

            if (unlikely(tx_nb < out_nb_pkts)) {
                pktmbuf_free_burst(out_pkts, tx_nb, out_nb_pkts);
            }
        }

    }

    return 0;
}

int
func_worker_start_with_load(char *request, struct instance_channel_t *instance_channel) {

    func_worker_init(request);

    func_worker_run(instance_channel);

    return 0;
}


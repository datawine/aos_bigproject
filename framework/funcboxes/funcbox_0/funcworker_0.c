//
// Created by hy on 19-3-17.
//
#include <unistd.h>

#include "memzone.h"

#include "instance.h"

#include "funcworker_0.h"
#include "firewall.h"
#include "monitor.h"
#include "ids.h"
#include "vpn.h"
#include "nat.h"

static char *instance_status_name;
static struct instance_status_t *instance_status;

static char *packet_rx_ring_name;
static struct rte_ring *packet_rx_ring;

static char *packet_tx_ring_name;
static struct rte_ring *packet_tx_ring;

FILE *fp;

static volatile bool nf_keep_running = 1;

static int pipe_fd[2];

static void
signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
        nf_keep_running = 0;
}

static int
func_parse_args(int argc, char *argv[]) {
    int opt;

    while ((opt = getopt (argc, argv, "r:t:s:i:o:")) != -1)
        switch (opt) {
            case 'r':
                packet_rx_ring_name = optarg;
                break;
            case 't':
                packet_tx_ring_name = optarg;
                break;
            case 's':
                instance_status_name = optarg;
                break;
            case 'i':
                pipe_fd[0] = atoi(optarg);
                break;
            case 'o':
                pipe_fd[1] = atoi(optarg);
                break;
            default:
                return -1;
        }

    return 0;
}


int
func_worker_init(int argc, char *argv[]) {
    int retval_eal;

    if ((retval_eal = rte_eal_init(argc, argv)) < 0)
        return -1;

    argc -= retval_eal; argv += retval_eal;

    if (func_parse_args(argc, argv) < 0)
        rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

    /* Get the instance status */
    instance_status = (struct instance_status_t *)memzone_lookup(instance_status_name);

    /* Get the rings of worker */
    packet_rx_ring = rte_ring_lookup(packet_rx_ring_name);
    if (packet_rx_ring == NULL)
        rte_exit(EXIT_FAILURE, "Func-Worker gets rx_ring failed\n");
    packet_tx_ring = rte_ring_lookup(packet_tx_ring_name);
    if (packet_tx_ring == NULL)
        rte_exit(EXIT_FAILURE, "Func-Worker gets tx_ring failed\n");

    /* init nf runtime */
    func_fw_local_init();

    return 0;
}

int
func_worker_run(void) {
    struct rte_mbuf *rx_pkts[MAX_PKT_BURST], **in_pkts, **out_pkts = NULL;
    uint32_t nb_pkts, tx_nb, out_nb_pkts, in_nb_pkts;
    int sig;
    sigset_t set;

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

    instance_status->pid = getpid();
    instance_status->status = FUNC_RUNNING;

    // start to run
    int write_n = 0;
    write_n = write(pipe_fd[1], "write done\n", 11);
    if (write_n == 0) { // error writing
        return 0;
    }
//    kill(getppid(), SIGCONT);

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

        out_nb_pkts = packets_fw_local_handler(in_pkts, in_nb_pkts, out_pkts);

        if (likely(out_nb_pkts != 0)) {
            tx_nb = rte_ring_sp_enqueue_burst(packet_tx_ring, (void *)out_pkts, out_nb_pkts, NULL);

            if (unlikely(tx_nb < out_nb_pkts)) {
                pktmbuf_free_burst(out_pkts, tx_nb, out_nb_pkts);
            }
        }
    }

    return 0;
}




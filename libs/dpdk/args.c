/*
* Created by Zhilong Zheng
*/

#include <stddef.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <rte_compat.h>

static unsigned long
parse_portmask(const char *portmask)
{
    unsigned long pm;
    char *end = NULL;

    /* parse hexadecimal string */
    pm = strtoul(portmask, &end, 16);
    if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
        rte_exit(EXIT_FAILURE, "failure when parsing portmask\n");

    if (pm == 0) {
        rte_exit(EXIT_FAILURE, "failure when parsing portmask\n");
    }

    return pm;
}


unsigned long
get_portmask(int argc, char **argv)
{
    int opt;
    int option_index;
    char **argvopt;
    char *prgname = argv[0];
    unsigned long portmask = 0;
    static struct option lgopts[] = {
        {NULL, 0, 0, 0}
    };

    argvopt = argv;

    while ((opt = getopt_long(argc, argvopt, "p:",
                              lgopts, &option_index)) != EOF) {
        switch (opt) {
            /* portmask */
            case 'p':
                portmask = parse_portmask(optarg);
                if (portmask == 0) {
                    printf("invalid portmask\n");
                    rte_exit(EXIT_FAILURE, "failure when getting portmask\n");
                }
                break;
            default:
                rte_exit(EXIT_FAILURE, "failure when getting portmask\n");
        }
    }
    if (optind <= 1) {
        rte_exit(EXIT_FAILURE, "failure when getting portmask\n");
    }

    argv[optind-1] = prgname;
    optind = 0; /* reset getopt lib */
    return portmask;
}

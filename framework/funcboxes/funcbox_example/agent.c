//
// Created by junxian shen on 2019-05-10.
//

#include "agent.h"
#include <rte_mbuf.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>
#include <hiredis.h>
#include "funcbox_0.h"
#include "dpdkhelper.h"
#include "common.h"
#include "firewall.h"
#include "nat.h"
#include "ids.h"
#include "monitor.h"


uint16_t agent_init(uint32_t func_type) {
    FILE *fp;
    if ((fp = fopen("./tmp-time.txt", "a")) == NULL)
        printf("cannot open file!\n");

    int sockfd,numbytes;
    char buf[4096];
    struct sockaddr_in their_addr;
    printf("break!");
    while((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1);
    printf("We get the sockfd~\n");
    their_addr.sin_family = AF_INET;
    their_addr.sin_port = htons(8000);
    their_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    bzero(&(their_addr.sin_zero), 8);
    long time_delta_ns;

    if (connect(sockfd, (struct sockaddr*)&their_addr, sizeof(struct sockaddr)) == -1) {
        printf("didn't get it!\n");
        return 0;
    } else {
//        printf("Get the Server~Cheers!\n");

        snprintf(buf, sizeof(buf) - 1, "test1");
        numbytes = send(sockfd, buf, strlen(buf), 0);
//        printf("Entersome thing1:\n");

        uint32_t i;
        struct hash_table_t** mz;
        struct hash_table_t *hash_table;
        uint32_t msg_size;
        switch (func_type) {
            case 0:
                mz = memzone_lookup("firewall");
                msg_size = sizeof(struct fw_state_entry_t);
                break;
            case 1:
                mz = memzone_lookup("monitor");
                msg_size = sizeof(struct monitor_state_entry_t);
                break;
            case 2:
                mz = memzone_lookup("nat");
                msg_size = sizeof(struct nat_state_entry_t);
                break;
            case 3:
                mz = memzone_lookup("ids");
                msg_size = sizeof(struct ids_state_entry_t);
                break;
            default:
                break;
        }
        hash_table = *mz;
        struct fw_state_entry_t* fw_state_entry;
        struct monitor_state_entry_t* monitor_state_entry;
        struct nat_state_entry_t* nat_state_entry;
        struct ids_state_entry_t* ids_state_entry;

        uint32_t sumbytes = 0;
        for (i = 0; i < 500; i ++) {
            numbytes = recv(sockfd, buf, msg_size, 0);//接收服务器端信息
            sumbytes += numbytes;

//            printf("numbytes: %u, sumbytes: %u\n", numbytes, sumbytes);
//            fflush(fp);

            int tbl_index;
            uint32_t hash_key = i;
            tbl_index = rte_hash_add_key_with_hash(hash_table->hash, (void *)&hash_key, i);

            if (tbl_index >= 0) {
                switch (func_type) {
                    case 0:
                        fw_state_entry = (struct fw_state_entry_t *)&hash_table->data[tbl_index * hash_table->entry_size];
                        strncpy((char *)fw_state_entry, buf, sizeof(struct fw_state_entry_t));
                        break;
                    case 1:
                        monitor_state_entry = (struct monitor_state_entry_t *)&hash_table->data[tbl_index * hash_table->entry_size];
                        strncpy((char *)monitor_state_entry, buf, sizeof(struct monitor_state_entry_t));
                        break;
                    case 2:
                        nat_state_entry = (struct nat_state_entry_t *)&hash_table->data[tbl_index * hash_table->entry_size];
                        strncpy((char *)nat_state_entry, buf, sizeof(struct nat_state_entry_t));
                        break;
                    case 3:
                        ids_state_entry = (struct ids_state_entry_t *)&hash_table->data[tbl_index * hash_table->entry_size];
                        strncpy((char *)ids_state_entry, buf, sizeof(struct ids_state_entry_t));
                        break;
                    default:
                        break;
                }
            } else {
                tbl_index = 0;
            }
        }

        clock_gettime(CLOCK_REALTIME, &cur_time);
        time_delta_ns = ((1.0e9 * cur_time.tv_sec + cur_time.tv_nsec) -
                         (1.0e9 * last_time.tv_sec + last_time.tv_nsec));

        snprintf(buf, sizeof(buf) - 1, "test2");
        numbytes = send(sockfd, buf, strlen(buf), 0);
        close(sockfd);
    }

    return 1;
}

//
// Created by Zhilong Zheng on 2019/3/17.
//
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>

//#define CLIENT_SOCK_FILE "/var/tmp/sandbox_daemon_client1.sock"
//#define SERVER_SOCK_FILE "/var/tmp/sandbox_daemon_server.sock"
#define CLIENT_SOCK_FILE "/var/tmp/manager_client.sock"
#define SERVER_SOCK_FILE "/var/tmp/manager_server.sock"

int main() {
    int sock_fd;
    struct sockaddr_un addr;

    char send_buf[1024];
    char recv_buf[1024];
    int recv_size = 1024;
    int recv_len;

    int i, ret;

    if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        printf("client socket error\n");
        exit(1);
    }

    //client
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, CLIENT_SOCK_FILE);
    unlink(CLIENT_SOCK_FILE);
    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("client bind error\n");
    }

    //server
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SERVER_SOCK_FILE);
    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        printf("server connect error\n");
    }

    for (i = 0; i < 1; ++i) {
        //send
        printf("sending\n");
//        strcpy(send_buf, "run_new_func;instance_name11111;funcbox_0;Deliver_rx_1_queue;Deliver_tx_1_queue;0x40;");
        strcpy(send_buf, "get_func_pid;instance_1;");

        ret = send(sock_fd, send_buf, strlen(send_buf) + 1, 0);
        if (ret == -1) {
            printf("send error\n");

        }

        //recv, Blocked
        recv_len = recv(sock_fd, recv_buf, recv_size, 0);
        if (recv_len < 0) {
            printf("recv error\n");
        }
        printf("recved: %s\n", recv_buf);

        sleep(1);
    }

    unlink(CLIENT_SOCK_FILE);

}
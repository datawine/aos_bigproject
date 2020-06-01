#!/usr/bin/env python3
# -*- coding:utf-8 -*-

import os
import socket
import time
import subprocess

#SANDBOX_EXEC_PATH = '/home/ubuntu/projects/serverless-nfv/framework/sandbox/sandbox'
SANDBOX_EXEC_PATH = '/home/amax/projects/serverless-nfv/framework/sandbox/sandbox'
SERVER_SOCK_ADDR = "/var/tmp/sandbox_daemon_server.sock"

#NF_PATH = '/home/ubuntu/projects/serverless-nfv/funcboxes/'
NF_PATH = '/home/amax/projects/serverless-nfv/funcboxes/'

def run_new_func(instance_name, func_name, rx_ring_name, tx_ring_name, core_id, lk_name, nf_name):
    sandbox_name = instance_name
    sandbox_args = "-u"
    sandbox_funcname = NF_PATH + func_name + "/build/app/" + func_name
    core_id = core_id

    sandbox_args_func_args = "-l " + core_id + " -n " + "4 " + "--proc-type=" + "secondary " + "-- " \
                             + "-r " + rx_ring_name + ' -t ' + tx_ring_name \
                             + " -l " + core_id + ' -k ' + lk_name \
                             + ' -f ' + nf_name + " tag=" + instance_name

    os_cmd = 'sudo ' + SANDBOX_EXEC_PATH + " " + sandbox_args + " " + sandbox_name + " " + sandbox_funcname + " " + sandbox_args_func_args

    # print(os_cmd)
    # Write to shell
    with open('start_sandbox.sh', 'w+') as file:
        file.write(os_cmd)

    subprocess.Popen(['sh', 'start_sandbox.sh'], stdout=subprocess.PIPE)


def get_func_pid(tag):
    output = os.popen('ps -ef | grep tag=' + tag).read()

    split_str = output.split(' ')
    new_array = []
    for _str in split_str:
        if _str != '':
            new_array.append(_str)

    instance_pid = new_array[1]

    return instance_pid


def handle_message(message):
    print("Received message: " + message)

    split_str = message.split(';')

    # Run_new_func
    if len(split_str) == 9:
        call_type = split_str[0]
        instance_name = split_str[1]
        func_name = split_str[2]
        rx_ring_name = split_str[3]
        tx_ring_name = split_str[4]
        core_id = split_str[5]
        lk_name = split_str[6]
        nf_name = split_str[7]

        run_new_func(instance_name, func_name, rx_ring_name, tx_ring_name, core_id, lk_name, nf_name)

        time.sleep(0.8)
        pid = get_func_pid(instance_name).strip()
        ret_msg = pid

    else:
        ret_msg = "call_err;"

    return ret_msg

def create_server(server_addr):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    if sock < 0:
        print("Socket create error")

    if os.path.exists(server_addr):
        os.unlink(server_addr)
    if sock.bind(server_addr):
        print("Socket bind error")
    if sock.listen(128):
        print("Socket listen error")
    print("Socket start successfully!")

    while True:
        print("Waiting connection...")

        conn, client_addr = sock.accept()

        try:
            while True:
                msg = conn.recv(4096)
                if msg:
                    ret_msg = handle_message(msg)
                    conn.sendall(ret_msg)
                else:
                    break
        finally:
            conn.close()


def main():
    server_addr = SERVER_SOCK_ADDR
    create_server(server_addr)


if __name__ == '__main__':
    main()
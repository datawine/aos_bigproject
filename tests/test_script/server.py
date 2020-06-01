# -*- coding: UTF-8 -*-
import socket
import threading
import queue
import sys
import re
import time
import subprocess
import signal
from tools import init_server

# tcp_link structure
# 0 c
# 1 addr
# 2 input queue
# 3 tcplink object
# 4 output queue

if __name__ == '__main__':
    host_name = '202.112.237.33'
    port = 6132
    link_num = 3

    tcp_list = init_server(host_name, port, link_num) 
    # tcp_list[0] is for 39 pktgen
    # tcp_list[1] is for 37 gateway
    # tcp_list[2] is for 33 sandbox/executor

    # test_type_list = ["throughput"]
    test_type_list = ["throughput", "latency"]
    function_name_list = ["firewall", "nat", "monitor", "ids", "vpn", "firewall_setget", "nat_setget", "monitor_setget", "ids_setget", "vpn_setget"]
    # function_name_list = ["nat", "firewall", "nat_setget", "monitor_setget"]
    # max_threads_list = ["16", "64", "256"]
    max_threads_dict = {0: "256", 1: "16", 2: "16", 3: "256", 4: "16", 5: "16"}
    max_batch_list = ["1", "4", "16"]
    tmp_rate = 25
    for test_type in test_type_list:
        if test_type == "throughput":
            for funcbox_index in range(0, 6):
                for function_name in function_name_list:
                    for max_batch in max_batch_list:
                        for repeat_time in range(2):
                            funcbox_name = "funcbox_" + str(funcbox_index)
                            max_threads = max_threads_dict[funcbox_index]
                            zero_cnt = 0
                            while True:
                                client_cmd = test_type + ";" + funcbox_name + ";" + \
                                             function_name + ";" + max_threads + ";" + max_batch
                                pktgen_cmd = test_type + ";" + str(tmp_rate)
                                print(client_cmd)
                                print(pktgen_cmd)

                                tcp_list[0][2].put(pktgen_cmd)
                                tcp_list[2][2].put(client_cmd)
                                time.sleep(7)
                                tcp_list[1][2].put(client_cmd)

                                tmpret = tcp_list[1][4].get()[0]
                                tmpret = tcp_list[2][4].get()[0]
                                tmpret = tcp_list[0][4].get()[0]
                                tmpret = tmpret[:tmpret.find("myend")]

                                _client_cmd = "stop"
                                tcp_list[1][2].put(_client_cmd)
                                tcp_list[2][2].put(_client_cmd)
                                time.sleep(3)

                                # print(tmpret)
                                if int(tmpret.split(";")[0]) == 0:
                                    zero_cnt = zero_cnt + 1
                                print("tmpret: ", tmpret, "; zero cnt: ", zero_cnt)
                                if int(tmpret.split(";")[0]) != 0 or zero_cnt == 2:
                                    with open("./result.txt", "a+") as f:
                                        f.write(client_cmd + " time: " + time.asctime(time.localtime(time.time())) + "\n")
                                        f.write(tmpret + "\n")
                                    break
        else:
            cmd_arg = ""
            funcbox_name = ""
            function_name = ""
            max_threads = ""
            max_batch = ""
            client_cmd = ""
            cnt = 0
            with open("./result.txt", "r") as f:
                for line in f.readlines():
                    cnt = cnt + 1
                    if cnt % 2 == 1:
                        cmd_arg = line.split(" ")[0]
                        funcbox_name = cmd_arg.split(";")[1]
                        function_name = cmd_arg.split(";")[2]
                        max_threads = cmd_arg.split(";")[3]
                        max_batch = cmd_arg.split(";")[4]
                        client_cmd = test_type + ";" + funcbox_name + ";" + \
                                     function_name + ";" + max_threads + ";" + max_batch
                    else:
                        for tmp_percent in [0.3]:
                            for repeat_time in range(1):
                                print("repeat time: ", repeat_time)
                                tmp_rate = round(float(line.split(";")[0]) * tmp_percent / 400, 5)
                                pktgen_cmd = test_type + ";" + str(tmp_rate)
                                zero_cnt = 0
                                while True:
                                    print(client_cmd)
                                    print(pktgen_cmd)

                                    tcp_list[0][2].put(pktgen_cmd)
                                    tcp_list[2][2].put(client_cmd)
                                    time.sleep(7)
                                    tcp_list[1][2].put(client_cmd)

                                    tmpret = tcp_list[1][4].get()[0]
                                    tmpret = tcp_list[2][4].get()[0]
                                    tmpret = tcp_list[0][4].get()[0]
                                    tmpret = tmpret[:tmpret.find("myend")]

                                    _client_cmd = "stop"
                                    tcp_list[1][2].put(_client_cmd)
                                    tcp_list[2][2].put(_client_cmd)
                                    time.sleep(3)

                                    # print(tmpret)
                                    if int(tmpret.split(";")[0]) == 0:
                                        zero_cnt = zero_cnt + 1
                                    print("tmpret: ", tmpret, "; zero cnt: ", zero_cnt)
                                    if int(tmpret.split(";")[0]) != 0 or zero_cnt == 2:
                                        with open("./result.txt", "a+") as f:
                                            f.write(client_cmd + " time: " + time.asctime(time.localtime(time.time())) + "\n")
                                            f.write(tmpret + "\n")
                                        break

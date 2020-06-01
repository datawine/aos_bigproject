import socket
import time
import os
import subprocess
import threading
import re
import signal
from tools import connect_to_host, client_thread, get_ip

func_dict = {"firewall" : '0',
             "monitor": '1',
             "nat": '2',
             "ids": '3',
             "vpn": '4',
             "firewall_setget" : '5',
             "monitor_setget": '6',
             "nat_setget": '7',
             "ids_setget": '8',
             "vpn_setget": '9'}


def run_client_cmd(_cmd):
    # shell=True is a bad habbit
    # but i dont want to debug when shell=False
    # because dpdk-pktgen just report some errs

    print(_cmd)
    p = subprocess.Popen(_cmd, shell=True, stdout=subprocess.PIPE)
    # p = subprocess.Popen(_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    ret = []
    while True:
        line = p.stdout.readline()
        if not line:
            break
        print(str(line[:-1], encoding = 'utf-8'))
        ret.append(line)
    return ret

# this is for process which must be killed manually
def run_client_cmd_withuserkill(_cmd):
    # shell=True is a bad habbit
    # but i dont want to debug when shell=False
    # because dpdk-pktgen just report some errs

    print(_cmd)
    p = subprocess.Popen(_cmd, shell=True, stdout=subprocess.PIPE)
    # p = subprocess.Popen(_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    return p

def parse_socket_data(data):
    return [data], ["tmp type"]

def parse_pktgen_data(data):
    cmd_list = []
    # type list is for situations when 
    # may execution process may return difference type of result
    # so as to make the parse result easier

    # notice: if not python is not in 
    # /home/ubuntu/projects/serverless-nfv/tests/pktgen-3.5.0
    # error reports 'lua-shell: module 'Pktgen' not found:'
    type_list = []
    rate = data.split(";")[1]
    _cmd = 'sudo ./app/x86_64-native-linuxapp-gcc/pktgen '
    _cmd = _cmd + '-l 1,3,5,7,9,11,13,15,12 -n 4 --socket-mem 1024,1024 '
    _cmd = _cmd + '-- -P -m "[3/5:7/9].0" '
    tp_cmd = _cmd + '-f ./tmp_stdin.lua'
    lt_cmd = _cmd + '-f ./latency.lua'
    if data.find("throughput") != -1:
        cmd_list.append(tp_cmd)
        type_list.append("tp")
    else:
        linelist = []
        with open("./latency.lua", "r") as f:
            line_cnt = 0
            for line in f.readlines():
                line_cnt = line_cnt + 1
                if line_cnt == 2:
                    line = line[:line.rfind(',')] + ", " + rate + ")\n"
                linelist.append(line)
        with open("./latency.lua", "w") as f:
            for line in linelist:
                f.write(line)
        cmd_list.append(lt_cmd)
        type_list.append("lt")
    return cmd_list, type_list

def parse_pktgen_result(ret, test_type):
    if test_type == "tp":
        tp_result = 0
        def parse_return_stats(stats):
            pattern = re.compile("(\d+)/\d+")
            return pattern.findall(stats)
        def parse_return_stats2(stats):
            pattern = re.compile("\d+/(\d+)")
            return pattern.findall(stats)

        for line in ret:
            line = str(line, encoding = "utf-8")
            pos = line.rfind("UP-40000-FD")
            if pos != -1:
                bps_tp_result = parse_return_stats(line[pos:])[2]
                pps_tp_result = parse_return_stats2(line[pos:])[0]
        return [bps_tp_result, pps_tp_result]
    else:
        lt_result = 0
        for line in ret:
            line = str(line, encoding = "utf-8")
            line = re.sub('\x1b', ';', line)
            # print(line)
            pos = line.rfind(";[9;20H")
            if pos != -1:
                pos_head = pos - 1
                tmp_res = ""
                while line[pos_head] != ' ':
                    tmp_res = line[pos_head] + tmp_res
                    pos_head = pos_head - 1
                lt_result = int(tmp_res)          
        return [lt_result]

def parse_gateway_data(data):
    cmd_list = []
    type_list = []

    gateway_main_path = "/home/ubuntu/projects/serverless-nfv/framework/gateway_rtc/main.c"
    s =  ""
    with open(gateway_main_path, 'r') as f:
        s = f.read()
    
    if data.find("throughput") != -1:
        MAX_PKTS_BURST_TX = "32"
        type_list.append("tp")
        type_list.append("tp")
    else:
        MAX_PKTS_BURST_TX = "32"
        # MAX_PKTS_BURST_TX = "1"
        type_list.append("lt")
        type_list.append("lt")

    open(gateway_main_path, 'w').write(
        re.sub(r'#define\sMAX_PKTS_BURST_TX\s[0-9]+', 
        "#define MAX_PKTS_BURST_TX " + MAX_PKTS_BURST_TX, s))

    cmd_list.append("./make.sh gateway")
    cmd_list.append("./start.sh gateway")
    return cmd_list, type_list

def parse_gateway_result(ret, test_type):
    return ["done"]

def parse_executor_old_data(data):
    # data:
    # throughput/latency;funxbox_x;firewall/monitor/nat/ids/vpn
    funcbox_name = data.split(";")[1]
    nf_name = data.split(";")[2]
    max_threads_num = data.split(";")[3]
    max_batch_num = data.split(";")[4]
    print(max_threads_num)

    cmd_list = []
    type_list = []

    dispatcher_h_path = "/home/amax/projects/serverless-nfv/framework/executor/includes/dispatcher.h"
    s =  ""
    with open(dispatcher_h_path, 'r') as f:
        s = f.read()
    
    if data.find("throughput") != -1:
        MAX_PKTS_BURST_TX = "32"
    else:
        MAX_PKTS_BURST_TX = "1"

    open(dispatcher_h_path, 'w').write(
        re.sub(r'#define\sDISPATCHER_BUFFER_PKTS\s[0-9]+', 
        "#define DISPATCHER_BUFFER_PKTS " + MAX_PKTS_BURST_TX, s))

    dispatcher_main_path = "/home/amax/projects/serverless-nfv/framework/executor/main.c"
    linelist = []
    with open(dispatcher_main_path, "r") as f:
        for line in f.readlines():
            if line.find(funcbox_name) != -1:
                if line[0] == '/':
                    line = line[2:]
            elif line.find("funcbox_") != -1:
                if line[0] != '/':
                    line = "//" + line
            linelist.append(line)
    with open(dispatcher_main_path, "w") as f:
        for line in linelist:
            f.write(line)

    dir_name = "/home/amax/projects/serverless-nfv/funcboxes/" + funcbox_name + "/includes/"
    with open(dir_name + "funcbox.h", 'r') as f:
        s = f.read()
    open(dir_name + "funcbox.h", 'w').write(
        re.sub(r'#define\sMAX_BATCH_SIZE\s[0-9]+',
               "#define MAX_BATCH_SIZE " + max_batch_num, s))

    # dir_name = "/home/amax/projects/serverless-nfv/funcboxes/funcbox4_instance/includes/"
    # with open(dir_name + "funcbox.h", 'r') as f:
    #     s = f.read()
    # open(dir_name + "funcbox.h", 'w').write(
    #     re.sub(r'#define\sMAX_BATCH_SIZE\s[0-9]+',
    #            "#define MAX_BATCH_SIZE " + max_batch_num, s))

    dir_name = "/home/amax/projects/serverless-nfv/funcboxes/" + funcbox_name + "/includes/"
    with open(dir_name + "funcbox.h", 'r') as f:
        s = f.read()
    open(dir_name + "funcbox.h", 'w').write(
        re.sub(r'#define\sMAX_THREADS_NUM\s[0-9]+', 
        "#define MAX_THREADS_NUM " + max_threads_num, s))    
    linelist = []
    # dir_name = "/home/amax/projects/serverless-nfv/funcboxes/funcbox4_instance/includes/"
    with open(dir_name + "funcworker.h", "r") as f:
        line_cnt = 0
        for line in f.readlines():
            line_cnt = line_cnt + 1
            if line[0] != '/' and line.find("include") != -1 and \
            (line.find("firewall") != -1 or line.find("monitor") != -1 \
                or line.find("nat") != -1 or line.find("ids") != -1\
                or line.find("vpn") != -1):
                line = "//" + line
            if line.find(nf_name.split("_")[0]) != -1:
                line = line[2:]
            linelist.append(line)
    with open(dir_name + "funcworker.h", "w") as f:
        for line in linelist:
            f.write(line)

    dir_name = "/home/amax/projects/serverless-nfv/funcboxes/" + funcbox_name + "/"
    # dir_name = "/home/amax/projects/serverless-nfv/funcboxes/funcbox4_instance/"
    linelist = []
    with open(dir_name + "Makefile", "r") as f:
        line_cnt = 0
        for line in f.readlines():
            line_cnt = line_cnt + 1
            if line[0] != '#' and (line.find("SRCS-y") != -1 or \
            line.find("INC") != -1 or line.find("CFLAGS") != -1) and (line.find("firewall") != -1 or line.find("monitor") != -1 \
                or line.find("nat") != -1 or line.find("ids") != -1\
                or line.find("vpn") != -1):
                line = "# " + line
            if line.find(nf_name) != -1 and \
                (line.find("set") == -1 or nf_name.find("set") != -1) and \
                (line.find("SRCS-y") != -1 or \
                line.find("INC") != -1 or line.find("CFLAGS") != -1):
                # print(">" * 30)
                # print(line[:-1])
                # print(nf_name, line.find(nf_name))
                # print("<" * 30)
                line = line[2:]
            linelist.append(line)
    print("*" * 30)
    with open(dir_name + "Makefile", "w") as f:
        for line in linelist:
            f.write(line)
            print(line[:-1])
    print("*" * 30)

    cmd_list.append("cd /home/amax/projects/serverless-nfv/framework/ && ./make.sh executor")
    type_list.append("mk")
    cmd_list.append("cd /home/amax/projects/serverless-nfv/funcboxes/" + funcbox_name +" && make clean && make")
    type_list.append("mk")
    # cmd_list.append("cd /home/amax/projects/serverless-nfv/funcboxes/funcbox4_instance && make clean && make")
    # type_list.append("mk")
    cmd_list.append("cd /home/amax/projects/serverless-nfv/framework/ && ./start.sh sandbox")
    type_list.append("run")
    cmd_list.append("cd /home/amax/projects/serverless-nfv/framework/ && ./start.sh executor")
    type_list.append("run")
    # cmd_list.append("cd /home/amax/projects/serverless-nfv/funcboxes/funcbox_4 && ./start.sh ")
    # type_list.append("run")
    return cmd_list, type_list

def parse_executor_data(data):
    # data:
    # throughput/latency;funxbox_x;firewall/monitor/nat/ids/vpn
    funcbox_name = data.split(";")[1]
    nf_name = data.split(";")[2]
    max_threads_num = data.split(";")[3]
    max_batch_num = data.split(";")[4]
    print(max_threads_num)

    cmd_list = []
    type_list = []

    dispatcher_h_path = "/home/amax/projects/serverless-nfv/framework/executor/includes/dispatcher.h"
    s =  ""
    with open(dispatcher_h_path, 'r') as f:
        s = f.read()

    if data.find("throughput") != -1:
        MAX_PKTS_BURST_TX = "32"
    else:
        MAX_PKTS_BURST_TX = "32"
        # MAX_PKTS_BURST_TX = "1"

    open(dispatcher_h_path, 'w').write(
        re.sub(r'#define\sDISPATCHER_BUFFER_PKTS\s[0-9]+',
               "#define DISPATCHER_BUFFER_PKTS " + MAX_PKTS_BURST_TX, s))

    dispatcher_main_path = "/home/amax/projects/serverless-nfv/framework/executor/main.c"
    linelist = []
    with open(dispatcher_main_path, "r") as f:
        for line in f.readlines():
            if line.find("manager_init") != -1:
                if funcbox_name == "funcbox_4":
                    if line[0] != '/':
                        line = "//" + line
                else:
                    if line[0] == '/':
                        line = line[2:]
            if line.find(funcbox_name) != -1:
                if line[0] == '/':
                    line = line[2:]
                line = line[:line.rfind(',')] + ", " + func_dict[nf_name] + ");\n"
                print(">" * 30)
                print(line)
                print(">" * 30)
            elif line.find("funcbox_") != -1:
                if line[0] != '/':
                    line = "//" + line
            linelist.append(line)
    with open(dispatcher_main_path, "w") as f:
        for line in linelist:
            f.write(line)

    dispatcher_manager_path = "/home/amax/projects/serverless-nfv/framework/executor/manager.c"
    linelist = []
    with open(dispatcher_manager_path, "r") as f:
        line_cnt = 0
        for line in f.readlines():
            line_cnt = line_cnt + 1
            if 85 <= line_cnt and line_cnt <= 100:
                if funcbox_name == "funcbox_4":
                    if line[0] != '/':
                        line = "//" + line
                else:
                    if line[0] == '/':
                        line = line[2:]
            linelist.append(line)
    with open(dispatcher_manager_path, "w") as f:
        for line in linelist:
            f.write(line)

    dir_name = "/home/amax/projects/serverless-nfv/funcboxes/" + funcbox_name + "/includes/"
    with open(dir_name + "funcbox.h", 'r') as f:
        s = f.read()
    open(dir_name + "funcbox.h", 'w').write(
        re.sub(r'#define\sMAX_BATCH_SIZE\s[0-9]+',
               "#define MAX_BATCH_SIZE " + max_batch_num, s))

    if funcbox_name == "funcbox_4":
        dir_name = "/home/amax/projects/serverless-nfv/funcboxes/funcbox4_instance/includes/"
        with open(dir_name + "funcbox.h", 'r') as f:
            s = f.read()
        open(dir_name + "funcbox.h", 'w').write(
            re.sub(r'#define\sMAX_BATCH_SIZE\s[0-9]+',
                   "#define MAX_BATCH_SIZE " + max_batch_num, s))

    dir_name = "/home/amax/projects/serverless-nfv/funcboxes/" + funcbox_name + "/includes/"
    with open(dir_name + "funcbox.h", 'r') as f:
        s = f.read()
    open(dir_name + "funcbox.h", 'w').write(
        re.sub(r'#define\sMAX_THREADS_NUM\s[0-9]+',
               "#define MAX_THREADS_NUM " + max_threads_num, s))

    cmd_list.append("cd /home/amax/projects/serverless-nfv/framework/ && ./make.sh executor")
    type_list.append("mk")
    cmd_list.append("cd /home/amax/projects/serverless-nfv/funcboxes/" + funcbox_name +" && make clean && make")
    type_list.append("mk")
    if funcbox_name == "funcbox_4":
        cmd_list.append("cd /home/amax/projects/serverless-nfv/funcboxes/funcbox4_instance && make clean && make")
        type_list.append("mk")
    else:
        cmd_list.append("cd /home/amax/projects/serverless-nfv/framework/ && ./start.sh sandbox")
        type_list.append("run")
    cmd_list.append("cd /home/amax/projects/serverless-nfv/framework/ && ./start.sh executor")
    type_list.append("run")
    if funcbox_name == "funcbox_4":
        cmd_list.append("cd /home/amax/projects/serverless-nfv/funcboxes/funcbox_4 && "
                        "sudo ./build/app/funcbox_4 -l 17 -n 4 --proc-type=secondary -- -r Deliver_rx_0_queue -t "
                        "Deliver_tx_0_queue -k Deliver_0_lock -l 17 -f " + func_dict[nf_name])
        type_list.append("run")
    return cmd_list, type_list

def parse_executor_result(ret, test_type):
    return ["done"]

def parse_data(data, cur_ip_addr):
    if cur_ip_addr == "202.112.237.39":
        return parse_pktgen_data(data)
    elif cur_ip_addr == "202.112.237.37":
        return parse_gateway_data(data)
    elif cur_ip_addr == "202.112.237.33":
        return parse_executor_data(data)

def parse_result(ret, test_type, cur_ip_addr):
    if cur_ip_addr == "202.112.237.39":
        return parse_pktgen_result(ret, test_type)
    elif cur_ip_addr == "202.112.237.37":
        return parse_gateway_result(ret, test_type)
    elif cur_ip_addr == "202.112.237.33":
        return parse_executor_result(ret, test_type)

def executorkill_p():
    _cmd = "ps -ef | grep sandbox | grep -v grep |awk '{print $2}' | xargs sudo kill -9"
    run_client_cmd_withuserkill(_cmd)
    _cmd = "ps -ef | grep executor | grep -v grep |awk '{print $2}' | xargs sudo kill -9"
    run_client_cmd_withuserkill(_cmd)
    _cmd = "ps -ef | grep funcbox | grep -v grep |awk '{print $2}' | xargs sudo kill -9"
    run_client_cmd_withuserkill(_cmd)

def gatewaykill_p():
    _cmd = "ps -ef | grep gateway | grep -v grep |awk '{print $2}' | xargs sudo kill -9"
    run_client_cmd_withuserkill(_cmd)

if __name__ == '__main__':
    cur_ip_addr = get_ip()
    print(cur_ip_addr, type(cur_ip_addr))

    host_name = '202.112.237.33'
    port = 6132
    s = connect_to_host(host_name, port)
    # p_list = []

    while True:
        data = s.recv(1024)
        data = str(data, encoding = "utf-8")
        if data:
            print(data)

            if data.find("stop") != -1:
                if cur_ip_addr == "202.112.237.37":
                    gatewaykill_p()
                elif cur_ip_addr == "202.112.237.33":
                    executorkill_p()
                s.send(bytes("stopmyend", encoding = "utf-8"))
                continue
            
            task_list = []
            ret = []

            cmd_list, type_list = parse_data(data, cur_ip_addr)
            print(cmd_list, type_list)

            if cur_ip_addr == "202.112.237.37":
                i = 0
                for cmd in cmd_list[:-1]:
                    task = client_thread(run_client_cmd, (cmd, ))
                    task_list.append(task)
                    task.start()
                    ret = ret + parse_result(task_list[i].get_result(), type_list[i], cur_ip_addr)
                    i = i + 1

                p = run_client_cmd_withuserkill(cmd_list[-1])
                # p_list.append(p)

            elif cur_ip_addr == "202.112.237.39":
                for cmd in cmd_list:
                    task = client_thread(run_client_cmd, (cmd, ))
                    task_list.append(task)
                    task.start()

                for i in range(len(task_list)):
                    ret = ret + parse_result(task_list[i].get_result(), type_list[i], cur_ip_addr)

            elif cur_ip_addr == "202.112.237.33":
                for cmd in cmd_list[:-2]:
                    task = client_thread(run_client_cmd, (cmd, ))
                    task_list.append(task)
                    task.start()
                    print(cmd)
                    time.sleep(3)

                for i in range(len(task_list)):
                    ret = ret + parse_result(task_list[i].get_result(), type_list[i], cur_ip_addr)

                # start sandbox
                task = client_thread(run_client_cmd, (cmd_list[-2], ))
                task.start()
                time.sleep(1)
                # start executor
                task = client_thread(run_client_cmd, (cmd_list[-1], ))
                task.start()


            print(ret)
            for line in ret:
                s.send(bytes(str(line), encoding = "utf-8"))
                s.send(bytes(";", encoding = "utf-8"))
            s.send(bytes("myend", encoding = "utf-8"))
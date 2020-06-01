import numpy as np
import matplotlib.pyplot as plt

def parse_str2num(str):
    ret = str
    if ret.find(':') != -1:
        ret = ret.split(":")[1]
    if ret.find("e") != -1:
        ret = float(ret.split('e')[0]) * (10 ** float(ret.split('e')[1]))
    ret = float(ret)
    return ret

def parse_util(fn, parse_type):
    # parse type
    # 0 is for cold_start
    # 1 is for exec_time
    # 3 is for multi_thread

    print(">" * 30)
    print(fn)
    util_xlist = []
    if parse_type == 0:
        util_xlist = []
    elif parse_type == 1:
        exec_index = np.arange(1, 5, 0.1)
        util_xlist = 10 ** exec_index
    elif parse_type == 2:
        util_xlist = []
        xlabel_cnt = 0
    util_ylist = []
    with open(fn, "r") as f:
        cnt = 0
        for line in f.readlines():
            if cnt % 5 == 1:
                core_num = parse_str2num(line.split('\t')[0])
                total_runtime = parse_str2num(line.split('\t')[3])
                coldstart_time = parse_str2num(line.split('\t')[5])
                if parse_type == 0:
                    util_xlist.append(coldstart_time)
            if cnt % 5 == 4:
                util_time = parse_str2num(line.split('\t')[9][:-1])
                util_ratio = util_time / (total_runtime * core_num)
                lat_99 = parse_str2num(line.split('\t')[7])
                print(core_num, total_runtime, coldstart_time, util_time, util_ratio, lat_99)
                if parse_type == 0 or parse_type == 1:
                    util_ylist.append(util_ratio)
                elif parse_type == 2:
                    util_xlist.append(xlabel_cnt)
                    util_ylist.append(lat_99)
                    xlabel_cnt = xlabel_cnt + 1
            cnt = cnt + 1

    return util_xlist, util_ylist

def parse_single_file(folder_name, para_list, parse_type, xlabel_name, ylabel_name):
    for para in para_list:
        fn = folder_name + str(para) + ".dat"
        xlabel, ylabel = parse_util(fn, parse_type)
        print(xlabel)
        print(ylabel)
        plt.plot(xlabel, ylabel, label = "cold start ratio = " + str(para))

    plt.legend(loc='best')
    if parse_type == 0 or parse_type == 1:
        plt.xscale('log')
    plt.xlabel(xlabel_name)
    plt.ylabel(ylabel_name)
    plt.show()


def parse_all_file():
    # folder_name = "./schedsim/data/serverless_simulation/cold_start/"
    # para_list = [0.2, 0.4, 0.6, 0.8]
    # xn = 'cold start time (us)'
    # yn = 'CPU utilization (%)'
    # parse_single_file(folder_name, para_list, 0, xn, yn)
    #
    # folder_name = "./schedsim/data/serverless_simulation/exec_time/"
    # para_list = [10, 100, 1000, 10000]
    # xn = 'function exec time (us)'
    # yn = 'CPU utilization (%)'
    # parse_single_file(folder_name, para_list, 1, xn, yn)

    folder_name = "./schedsim/data/serverless_simulation/load/"
    para_list = ["0_10", "1_10",]
    xn = 'function offload'
    yn = '99th latency (us)'
    parse_single_file(folder_name, para_list, 2, xn, yn)

    folder_name = "./schedsim/data/serverless_simulation/load/"
    para_list = ["0_100", "1_100",]
    xn = 'function offload'
    yn = '99th latency (us)'
    parse_single_file(folder_name, para_list, 2, xn, yn)

if __name__ == '__main__':
    parse_all_file()
# -*- coding: UTF-8 -*-

# 此文件用来实现一些基本模块，测试成功后会将功能移植到server或其他的模块中

import time
import re

# cmd = ['sudo', './app/x86_64-native-linuxapp-gcc/pktgen', '-l 1,3,5,7,9,11,13,15,12',
#          '-n', '4', '--socket-mem', '1024,1024', '--', '-P', '-m', '"[3/5:7/9].0"', '-f', './tmp_stdin.lua']

# cmd = 'sudo ./app/x86_64-native-linuxapp-gcc/pktgen -l 1,3,5,7,9,11,13,15,12 -n 4 --socket-mem 1024,1024 -- -P -m "[3/5:7/9].0" -f ./tmp_stdin.lua'

from subprocess import *
# p = Popen(cmd, stdout=PIPE, shell=True)

# with open("./tmp_result.txt", "w+") as f:
#     while True:
#         line = p.stdout.readline()
#         if not line:
#             break
#         f.write(line)


# while True:
#     line = p.stdout.readline()
#     if not line:
#         break
#     def parse_return_stats(stats):
#         pattern = re.compile("(\d+)/\d+")
#         return pattern.findall(stats)

#     pos = line.rfind("UP-40000-FD")
#     if pos != -1:
#         print(parse_return_stats(line[pos:]))

# with open("./tmp_result.txt", "r") as f:
#     def parse_return_stats(stats):
#         pattern = re.compile("(\d+)/\d+")
#         return pattern.findall(stats)

#     res = 0
#     for line in f.readlines():
#         line = re.sub('\x1b', ';', line)
#         # print(line)
#         pos = line.rfind(";[9;20H")
#         if pos != -1:
#             pos_head = pos - 1
#             tmp_res = ""
#             while line[pos_head] != ' ':
#                 tmp_res = line[pos_head] + tmp_res
#                 pos_head = pos_head - 1
#             res = int(tmp_res)          
#             # print(parse_return_stats(line[pos:]))
#             # print(line[pos:])
#     print(res)
# time.sleep(5)
# p.stdin.write('str/n')
# time.sleep(5)
# p.stdin.write('quit/n')

gateway_main_path = "/home/ubuntu/projects/serverless-nfv/framework/gateway_rtc/main.c"
s =  ""
with open(gateway_main_path, 'r') as f:
    s = f.read()
open(gateway_main_path, 'w').write(re.sub(r'#define\sMAX_PKTS_BURST_TX\s[0-9]+', "#define MAX_PKTS_BURST_TX " + "32", s))

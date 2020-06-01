# -*- coding: UTF-8 -*-
import socket
import threading
import queue
import sys
import re
import time
import subprocess
import signal

class tcpLink_thread(threading.Thread):
    def __init__(self, sock, addr, queue, outqueue):
        threading.Thread.__init__(self)
        self.sock = sock
        self.addr = addr
        self.queue = queue
        self.outqueue = outqueue

    def run(self):
        print('client connection:', self.addr)
        self.runLoop()

    def runLoop(self):
        # 不会自己停止
        # 持续监听queue
        while True:
            # 阻塞检测queue中消息，如果有，开始向client端发送命令
            queue_msg = self.queue.get()
            # print("sending: ", queue_msg)
            self.sock.send(bytes(queue_msg, encoding = "utf-8"))

            # 持续监听client端返回的消息
            ret_msg = ""
            while True:
                tcp_msg = self.sock.recv(1024)
                if tcp_msg:
                    # print("receiving from", self.addr[0])
                    tcp_msg = str(tcp_msg, encoding = "utf-8")
                    # print(tcp_msg)
                    ret_msg = ret_msg + tcp_msg
                    if tcp_msg.find("myend") != -1:
                        break
            self.outqueue.put([ret_msg])

    def runLoop_withstop(self):
        # 检测到客户端发送的'stop'关键字, 自己停止
        is_continue = True
        # 持续监听queue
        while is_continue:
            # 阻塞检测queue中消息，如果有，开始向client端发送命令
            queue_msg = self.queue.get()
            self.sock.send(bytes(queue_msg, encoding = "utf8"))

            ret_msg = ""
            while True:
                tcp_msg = self.sock.recv(1024)
                if tcp_msg:
                    print("receiving from", self.addr[0])
                    tcp_msg = str(tcp_msg, encoding = "utf-8")
                    print(tcp_msg)
                    ret_msg = ret_msg + tcp_msg
                    if tcp_msg.find("mystop") != -1:
                        is_continue = False
                        break
                    if tcp_msg.find("myend") != -1:
                        break
            self.outqueue.put([ret_msg])

class client_thread(threading.Thread):
    def __init__(self, func, args=()):
        threading.Thread.__init__(self)
        self.func = func
        self.args = args
 
    def run(self):
        self.result = self.func(*self.args)
 
    def get_result(self):
        threading.Thread.join(self)
        try:
            return self.result
        except Exception:
            return None

# 绑定并监听端口        
def bind_and_listen(host, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((host, port))

    s.listen(8)
    return s

# 初始化服务器,创建tcp_list
def init_server(host_name, port, link_num):
    s = bind_and_listen(host_name, port)

    tcp_list = []

    # initialize tcp connection
    for i in range(link_num):
        c, addr = s.accept()
        q = queue.Queue()
        out_q = queue.Queue()
        t = tcpLink_thread(c, addr, q, out_q)
        tcp_list.append((c, addr, q, t, out_q))
        t.start()
    return tcp_list

# 阻塞处理服务器数据
def parse_data(tcp_list, link_num, parse_func):
    link_num = len(tcp_list)

    data_sec = []
    for i in range(link_num):
        data_sec.append(tcp_list[i][4].get())

    return parse_func(data_sec)

def connect_to_host(host_name, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    s.connect((host_name, port))
    return s

def get_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
    finally:
        s.close()

    return ip

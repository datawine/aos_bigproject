#!/usr/bin/python

import os
import sys
import numpy as np
from subprocess import call
from multiprocessing import Process


mu = 0.5  # service time: 1/mu
mm116_lambdas = np.arange(0.5, 8.1, 0.2) # 16 servers average service time 2
duration = 1000000
intr_trhesholds = [2, 4, 10, 20]

GENTYPES = ['m', 'd', 'b', 'b2']  # exponential, deterministic, bimodal{1-2}
PROCTYPES = ['rtc', 'ps', 'push', 'pull', 'pull_intr']

def execute_topology(topo, lambdas, g, p, path, threshold=None):
    pathname = "{}/{}_{}.dat".format(path, PROCTYPES[p], GENTYPES[g])
    cmd = ["go", "run", "main.go", "--mu={}".format(mu), "--duration={}".format(duration),
            "--topo={}".format(topo), "--genType={}".format(g)]
    if p:
        cmd.append("--procType={}".format(p))
    if threshold:
        cmd.append("--threshold={}".format(threshold))

    with open(pathname, 'w') as f:
        for l in lambdas:
            exec_cmd = cmd + ["--lambda={}".format(l)]
            print exec_cmd
            call(exec_cmd, stdout=f)



def parallel_exec(args):
    '''
        args = [(topo, lambdas, genType, procType, path, <optinal threshold>)...]
    '''
    pids = []
    for run in args:
        p = Process(target=execute_topology, args=run)
        p.start()
        pids.append(p)
    for p in pids:
        p.join()

def single_queue():
    directory = "data/single_queue"
    if not os.path.exists(directory):
            os.makedirs(directory)
    genTypes = range(4) * 2
    pTypes = [0] * 4 + [1] * 4
    topologies = [0] * 8
    lambdas = [mm116_lambdas] * 8
    names = [directory] * 8
    parallel_exec(zip(topologies, lambdas, genTypes, pTypes, names))

def multi_queue():
    directory = "data/multi_queue"
    if not os.path.exists(directory):
            os.makedirs(directory)
    genTypes = range(4) * 2
    pTypes = [0] * 4 + [1] * 4
    topologies = [1] * 8
    lambdas = [mm116_lambdas] * 8
    names = [directory] * 8
    parallel_exec(zip(topologies, lambdas, genTypes, pTypes, names))

def execute_serverless_exec_time(topos, lambdas, g, p, path, exec_times, warm_start, cold_start, cold_start_ratio, threshold=None):
    pathname = "{}/exec_time/{}.dat".format(path, cold_start)
    cmd = ["go", "run", "main.go", "--duration={}".format(duration),
           "--topo={}".format(topos), "--lambda={}".format(lambdas), "--genType={}".format(g),
           "--warm_start={}".format(warm_start), "--cold_start={}".format(cold_start),
           "--cold_start_ratio={}".format(cold_start_ratio)]
    if p:
        cmd.append("--procType={}".format(p))
    if threshold:
        cmd.append("--threshold={}".format(threshold))

    with open(pathname, 'w') as f:
        for l in exec_times:
            exec_cmd = cmd + ["--mu={}".format(float(1)/l)]
            print exec_cmd
            call(exec_cmd, stdout=f)

def parallel_exec_exec_time(args):
    '''
        args = [(topo, lambdas, genType, procType, path, <optinal threshold>)...]
    '''
    pids = []
    for run in args:
        p = Process(target=execute_serverless_exec_time, args=run)
        p.start()
        pids.append(p)
    for p in pids:
        p.join()

def serverless_exec_time():
    directory = "data/serverless_simulation"
    if not os.path.exists(directory):
        os.makedirs(directory)
    genTypes = [1] * 4 #deterministic service time
    pTypes = [2] * 4 #serverless processor type
    topologies = [1] * 4 #multi_queue topology
    lambdas = [8] * 4
    names = [directory] * 4

    warm_start = [1] * 4
    cold_start_ratio = [0.5] * 4
    start_index = np.arange(1, 5)
    cold_start = 10 ** start_index
    exec_index = np.arange(1, 5, 0.2)
    exec_times = [10 ** exec_index] * 4

    parallel_exec_exec_time(zip(topologies, lambdas, genTypes, pTypes, names, exec_times, warm_start, cold_start, cold_start_ratio))

def execute_serverless_cold_start(topos, lambdas, g, p, path, exec_time, warm_start, cold_starts, cold_start_ratio, threshold=None):
    pathname = "{}/cold_start/{}.dat".format(path, cold_start_ratio)
    cmd = ["go", "run", "main.go", "--duration={}".format(duration),
           "--topo={}".format(topos), "--lambda={}".format(lambdas), "--genType={}".format(g),
           "--warm_start={}".format(warm_start), "--mu={}".format(float(1)/exec_time),
           "--cold_start_ratio={}".format(cold_start_ratio)]
    if p:
        cmd.append("--procType={}".format(p))
    if threshold:
        cmd.append("--threshold={}".format(threshold))

    with open(pathname, 'w') as f:
        for l in cold_starts:
            exec_cmd = cmd + ["--cold_start={}".format(l)]
            print exec_cmd
            call(exec_cmd, stdout=f)

def parallel_exec_cold_start(args):
    '''
        args = [(topo, lambdas, genType, procType, path, <optinal threshold>)...]
    '''
    pids = []
    for run in args:
        p = Process(target=execute_serverless_cold_start, args=run)
        p.start()
        pids.append(p)
    for p in pids:
        p.join()

def serverless_cold_start():
    directory = "data/serverless_simulation"
    if not os.path.exists(directory):
        os.makedirs(directory)
    genTypes = [1] * 4 #deterministic service time
    pTypes = [2] * 4 #serverless processor type
    topologies = [1] * 4 #multi_queue topology
    lambdas = [8] * 4
    names = [directory] * 4

    warm_start = [1] * 4
    exec_time = [100] * 4
    cold_start_ratio = np.arange(0.2, 1, 0.2)
    index = np.arange(1, 5, 0.2)
    cold_starts = [10 ** index] * 4

    parallel_exec_cold_start(zip(topologies, lambdas, genTypes, pTypes, names, exec_time, warm_start, cold_starts, cold_start_ratio))

def execute_serverless_load(topos, lambdas, g, p, path, exec_time, warm_start, cold_start, cold_start_ratio, threshold=None):
    pathname = "{}/{}_{}.dat".format(path, topos, exec_time)
    cmd = ["go", "run", "main.go", "--duration={}".format(duration),
           "--topo={}".format(topos), "--mu={}".format(float(1)/exec_time), "--genType={}".format(g),
           "--warm_start={}".format(warm_start), "--cold_start={}".format(cold_start),
           "--cold_start_ratio={}".format(cold_start_ratio)]
    if p:
        cmd.append("--procType={}".format(p))
    if threshold:
        cmd.append("--threshold={}".format(threshold))

    with open(pathname, 'w') as f:
        for l in lambdas:
            exec_cmd = cmd + ["--lambda={}".format(l)]
            print exec_cmd
            call(exec_cmd, stdout=f)

def parallel_exec_load(args):
    '''
        args = [(topo, lambdas, genType, procType, path, <optinal threshold>)...]
    '''
    pids = []
    for run in args:
        p = Process(target=execute_serverless_load, args=run)
        p.start()
        pids.append(p)
    for p in pids:
        p.join()

def serverless_load():
    directory = "data/serverless_simulation/load"
    if not os.path.exists(directory):
        os.makedirs(directory)
    genTypes = [1] * 4 #deterministic service time
    pTypes = [2] * 4 #serverless processor type
    topologies = range(2) * 2 #single and multiple queue topology
    # lambdas = [mm116_lambdas] * 4
    m10_lambda = np.arange(0.1, 1.61, 0.1)
    m100_lambda = np.arange(0.01, 0.161, 0.01)
    lambdas = [m10_lambda] * 2 + [m100_lambda] * 2
    names = [directory] * 4

    warm_start = [1] * 4
    exec_time = [10] * 2 + [100] * 2
    cold_start_ratio = [0.1] * 4
    cold_start = [100] * 4

    parallel_exec_load(zip(topologies, lambdas, genTypes, pTypes, names, exec_time, warm_start, cold_start, cold_start_ratio))


def main():
    if len(sys.argv) != 2:
        print "Usage: python run.py <function_name>"
        return
    if sys.argv[1] == "single_queue":
        single_queue()
    elif sys.argv[1] == "multi_queue":
        multi_queue()
    elif sys.argv[1] == "serverless_overhead":
        serverless_exec_time()
        serverless_cold_start()
    elif sys.argv[1] == "serverless_load":
        serverless_load()
    else:
        print "Unknown function name"

if __name__ == "__main__":
    main()

# time_unit = 1us

import numpy as np
import matplotlib.pyplot as plt

# plot the effect of cold start time and cold start ratio
def cold_start_effect():
    exec_time = 100
    warm_start = 1
    index = np.arange(1, 5, 0.1)
    cold_start = 10 ** index
    cold_start_ratios = np.arange(0.2, 1, 0.2)

    for cold_start_ratio in cold_start_ratios:
        # throughput = 10 ** 6 / (exec_time + cold_start * cold_start_ratio + warm_start * (1 - cold_start_ratio))
        utilization = exec_time / (exec_time + cold_start * cold_start_ratio + warm_start * (1 - cold_start_ratio))
        plt.plot(cold_start, utilization, label = "cold start ratio = " + str(cold_start_ratio)[:3])

    plt.legend(loc='best')
    plt.xscale('log')
    plt.xlabel("cold start time (us)")
    plt.ylabel("CPU utilization (%)")
    plt.show()


# plot the effect of exec time and cold start time
def exec_time_effect():
    warm_start = 1
    # cold_start_ratio = 0.5
    cold_start_ratio = 1
    # exec_index = np.arange(1, 5, 0.1)
    exec_index = np.arange(1, 6, 0.1)
    exec_time = 10 ** exec_index
    # start_index = np.arange(1, 5)
    start_index = np.arange(4, 5)
    cold_starts = 10 ** start_index

    for cold_start in cold_starts:
        # throughput = 10 ** 6 / (exec_time + cold_start * cold_start_ratio + warm_start * (1 - cold_start_ratio))
        utilization = exec_time / (exec_time + cold_start * cold_start_ratio + warm_start * (1 - cold_start_ratio))
        plt.plot(exec_time, utilization, label = "cold start time = 10ms")

    plt.legend(loc='best')
    plt.xscale('log')
    plt.xlabel("function exec time (us)")
    plt.ylabel("CPU utilization (%)")
    plt.show()


# think of request arrive pattern and process pattern
def main():
    exec_time_effect()
    # cold_start_effect()


if __name__ == '__main__':
    main()
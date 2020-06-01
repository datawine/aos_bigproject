//
// Created by hy on 2019/8/24.
//

#include <stdio.h>

#include "utilization.h"

/* cpu metric from /proc/stat */
struct cpu_metric_t {
    char name[20];
    uint32_t user;
    uint32_t nice;
    uint32_t system;
    uint32_t idle;
    uint32_t iowait;
    uint32_t irq;
    uint32_t softirq;
};

/* read cpu metric from /proc/stat */
static void
get_cpu_metric(struct cpu_metric_t *cpu_metric) {
    FILE *fd;
    char buff[256];

    fd = fopen("/proc/stat", "r");
    fgets(buff, sizeof(buff), fd);

    sscanf(buff, "%s %u %u %u %u %u %u %u", cpu_metric->name, &cpu_metric->user, &cpu_metric->nice, &cpu_metric->system,
            &cpu_metric->idle, &cpu_metric->iowait, &cpu_metric->irq, &cpu_metric->softirq);

    fclose(fd);
}

/* get cpu utilization for one cpu */
static void
get_cpu_utilization(struct cpu_metric_t *pre_metric, struct cpu_metric_t *cur_metric, struct cpu_utilization_t *cpu_utilization) {
    double diff_total, diff_user, diff_nice, diff_system, diff_idle;

    diff_total = (double)((cur_metric->user + cur_metric->nice + cur_metric->system + cur_metric->idle + cur_metric->iowait + cur_metric->irq + cur_metric->softirq)
            - (pre_metric->user + pre_metric->nice + pre_metric->system + pre_metric->idle + pre_metric->iowait + pre_metric->irq + pre_metric->softirq));
    diff_user = (double)(cur_metric->user - pre_metric->user);
    diff_nice = (double)(cur_metric->nice - pre_metric->nice);
    diff_system = (double)(cur_metric->system - pre_metric->system);
    diff_idle = (double)(cur_metric->idle - pre_metric->idle);

    if (diff_total > 0) {
        cpu_utilization->user = diff_user / diff_total * 100;
        cpu_utilization->nice = diff_nice / diff_total * 100;
        cpu_utilization->system = diff_system / diff_total * 100;
        cpu_utilization->idle = diff_idle /diff_total * 100;
        cpu_utilization->usage = (1 - diff_idle / diff_total) * 100;
    }
}

/* get metric from cur to pre */
static void
pre_equal_cur_metric(struct cpu_metric_t *pre_metric, struct cpu_metric_t *cur_metric) {
    pre_metric->user = cur_metric->user;
    pre_metric->nice = cur_metric->nice;
    pre_metric->system = cur_metric->system;
    pre_metric->idle = cur_metric->idle;
    pre_metric->iowait = cur_metric->iowait;
    pre_metric->irq = cur_metric->irq;
    pre_metric->softirq = cur_metric->softirq;
}

/* update cpus utilization */
void
update_cpus_utilization(struct cpu_utilization_t *cpus_utilization, uint16_t cpus_num) {
    static struct cpu_metric_t pre_metric[MAX_CORE_NUM], cur_metric[MAX_CORE_NUM];
    uint16_t i;

    for (i = 0; i < cpus_num; i++) {
        get_cpu_metric(&cur_metric[i]);
        get_cpu_utilization(&pre_metric[i], &cur_metric[i], &cpus_utilization[i]);
        pre_equal_cur_metric(&pre_metric[i], &cur_metric[i]);
    }
}
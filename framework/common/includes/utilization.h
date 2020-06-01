//
// Created by hy on 2019/8/24.
//

#ifndef SERVERLESS_NFV_UTILIZATION_H
#define SERVERLESS_NFV_UTILIZATION_H

#include <stdint.h>

#define MAX_CORE_NUM 64

/* cpus utilization metric */
struct cpu_utilization_t {
    double user;
    double nice;
    double system;
    double idle;
    double usage;
};

/* update cpus utilization */
void
update_cpus_utilization(struct cpu_utilization_t *cpus_utilization, uint16_t cpus_num);

#endif //SERVERLESS_NFV_UTILIZATION_H

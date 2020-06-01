//
// Created by Zhilong Zheng on 2019/3/12.
//

#ifndef SERVERLESS_NFV_MEMZONE_H
#define SERVERLESS_NFV_MEMZONE_H

#include <stdint.h>
#include <memory.h>

#include <rte_lcore.h>
#include <rte_memzone.h>

#define NO_FLAGS 0

/* reserve num * length bytes memzone */
void *
memzone_reserve(const char *memzone_name, uint32_t entry_length, uint32_t entry_num);

/* lookup memzone through memzone_name */
void *
memzone_lookup(const char *memzone_name);

#endif //SERVERLESS_NFV_MEMZONE_H

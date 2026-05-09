/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
 *
 * KUPL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *        http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef KUPL_HARDWARE_H
#define KUPL_HARDWARE_H

#include <cstring>
#include <cstdint>
#include <sched.h>
#include "utils/type/kupl_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief logical process
 */
typedef struct kupl_compute_place {
    int cpu_id;
    int numa_id;
} kupl_compute_place_t;

/**
 * @brief numa node
 */
typedef struct kupl_numa_node {
    int numa_id;
} kupl_numa_node_t;

typedef struct kupl_host_info {
    int init;
    int socket_cnt;
    int numa_cnt;
    int core_cnt;
    int pu_cnt;
    int pu_conf;
    int avail_pu_cnt;
    cpu_set_t avail_set;

    uint64_t *numa_distance;
    kupl_numa_node_t *numas;
    kupl_compute_place_t *cpus;
} kupl_host_info_t;

typedef enum kupl_arch_type {
    KUPL_CPU_HISILICOM_TSV110 = 0,
    KUPL_CPU_HISILICOM_920B,
    KUPL_CPU_HISILICOM_920C,
    KUPL_CPU_HISILICOM_920F,
    KUPL_CPU_UNKNOW,
} kupl_arch_type_t;

/**
 * @brief initialize the host info, @note this function must be called before call any function of follow
 */
int kupl_host_info_init(void);

/**
 * @brief cleanup the host info, @note when this function called all functions following can't be called
 */
void kupl_host_info_fini(void);

/**
 * @brief get the hardware topology information, @note must call @ref kupl_host_info_init() firstly
 */
const kupl_host_info_t *kupl_get_host_info(void);

#define kupl_get_numa_distance(_info, _i, _j) (_info)->numa_distance[(_i) * (_info)->numa_cnt + (_j)]

kupl_compute_place_t kupl_get_compute_place(int gcid);

/**
 * @brief print the host information
 */
void kupl_host_info_print(void);

kupl_arch_type_t kupl_arch_detect(void);

#ifdef __cplusplus
}
#endif

#endif
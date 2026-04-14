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
#ifndef KUPL_EXECUTOR_BACKEND_H
#define KUPL_EXECUTOR_BACKEND_H

#include <sched.h>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

#define KUPL_EIDCID_INIT (-1)

typedef enum kupl_backend_type {
    KUPL_BACKEND_INIT = -1,
    KUPL_BACKEND_OMP,
    KUPL_BACKEND_PTHREAD,
} kupl_backend_type_t;

typedef int (*kupl_backend_base_init)(void *exec);
typedef void (*kupl_backend_base_fini)(void *exec);
typedef int (*kupl_backend_base_setaffinity)(int core_id);
typedef int (*kupl_backend_base_set_geid)(int geid);
typedef int (*kupl_backend_base_get_geid)(void);
typedef int (*kupl_backend_base_set_gcid)(int gcid);
typedef int (*kupl_backend_base_get_gcid)(void);

typedef struct kupl_executor_ops {
    kupl_backend_base_init             init;
    kupl_backend_base_fini             fini;
    kupl_backend_base_setaffinity      set_affinity; // set affinity
    kupl_backend_base_set_geid         set_geid; // set global executor id
    kupl_backend_base_get_geid         get_geid; // get global executor id
} kupl_executor_ops_t;

void kupl_backend_type_select(void);

void kupl_backend_type_set(std::string &backend_type_str);

kupl_backend_type_t kupl_backend_type_get(void);

/**
 * @brief Convert global eid to cid
 */
int kupl_global_eid2cid(int geid);

/**
 * @brief Convert global cid to eid
 */
int kupl_global_cid2eid(int gcid);

/**
 * @brief Set the global executor id int this process
 */
int kupl_set_global_executor_id(int geid);

/**
 * @brief Get the global executor id int this process
 */
int kupl_get_global_executor_id(void);

/**
 * @brief Get the global core id in this process
 */
int kupl_get_global_core_id(void);

int kupl_set_affinity(int core_id);

int kupl_backend_init(void *exec);

void kupl_backend_fini(void *exec);

int kupl_set_executor_core_mapping(void);
int kupl_get_self_affinity(void);

#ifdef __cplusplus
}
#endif
#endif
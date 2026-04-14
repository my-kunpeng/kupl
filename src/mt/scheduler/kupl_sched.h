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
#ifndef KUPL_SCHED_H
#define KUPL_SCHED_H

#include <sched.h>
#include "mt/kupl_taskbase.h"
#include "mt/kupl_ult.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief The handle of kupl sched */
typedef struct kupl_sched kupl_sched_t;
typedef struct kupl_taskbase kupl_taskbase_t;

/**
 * @brief Initialize the sched module
 *
 * @return KUPL_OK for success
 */
int kupl_sched_init(void);

/**
 * @brief Expand the sched module
 *
 * @return KUPL_OK for success
 */
int kupl_sched_expand(void);

/**
 * @brief Finalize the expanded sched module, and cleanup all resources
 */
void kupl_sched_expand_fini(void);

/**
 * @brief Finalize the sched module, and cleanup all resources
 */
void kupl_sched_fini(void);

kupl_sched_t* kupl_get_global_sched();

kupl_sched_t* kupl_get_global_sched_expand();

/**
 * @brief Create a sched instance with plugin_name
 *
 * @param plugin_name   the name of plugin
 * @return              the handler of sched
 */
kupl_sched_t* kupl_sched_create(const char *plugin_name);

/**
 * @brief Cleanup the sched, and will set the sched in finished state
 */
void kupl_sched_cleanup(kupl_sched_t *sched);

/**
 * @brief Add one tb to sched, when tb queue is full we will execute this tb immediately
 */
void kupl_sched_add_tb(kupl_sched_t *sched, kupl_taskbase_t *tb);

int kupl_sched_add_ult(kupl_sched_t *sched, kupl_ult_t *ult);

/**
 * @brief Execute one tb in sched
 *
 * @return >=0 means the number of tb execute
 *          KUPL_FINISHED means sched is finished (which call @ref kupl_sched_cleanup())
 */
int kupl_sched_execute_tb(kupl_sched_t *sched);

void kupl_sched_yield(void);

#ifdef __cplusplus
}
#endif

#endif
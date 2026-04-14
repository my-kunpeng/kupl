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
#ifndef KUPL_EXECUTOR_H
#define KUPL_EXECUTOR_H

#include "mt/kupl_taskbase.h"
#include "mt/scheduler/kupl_sched.h"
#include "utils/arch/kupl_atomic.h"
#include "utils/arch/kupl_cache.h"
#include "utils/lock/kupl_lock.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KUPL_EXECUTOR_DEFAULT          (-1)
#define KUPL_EXECUTOR_MAX_SCHED        64

static const int KUPL_EXECUTOR_MAX_QUERY_COUNT = 100;

bool kupl_is_expand_executor(void);
int kupl_get_local_executor_num(int eid);

typedef struct kupl_executor_base {
    int executor_id;
    int core_id;
    KUPL_ATOMIC_BOOL stop;
    pthread_t thread_id;

    kupl_lock_t *lock;
    kupl_taskbase_t *current_tb;
    kupl_ult_h ult;
    kupl_sched_t *sched;

    bool is_master_executor;
} kupl_executor_base_t;

typedef union KUPL_ALIGN_CACHE kupl_executor {
    char pad[KUPL_PAD_CACHE(kupl_executor_base_t)];
    kupl_executor_base_t exe;
} kupl_executor_t;

/**
 * @brief Get the executor thread.
 *
 * @return the executor thread.
 */
kupl_executor_t* kupl_executor_get_current_executor(void);

/**
 * @brief set the current tb of the executor thread.
 *
 * @param [in] tb           the tb will be set on the executor thread
 */
void kupl_executor_set_current_tb(kupl_taskbase_t *tb);

/**
 * @brief get the current tb of the executor thread.
 *
 * @return tb               the current tb is working on the executor thread
 */
kupl_taskbase_t* kupl_executor_get_current_tb(void);

int kupl_executor_get_master_core_id(void);

cpu_set_t* kupl_get_global_executor_set(void);

/**
 * @brief Initialize the executor module
 */
int kupl_executor_init(void);

/**
 * @brief Finalize the executor module
 */
void kupl_executor_fini(void);

/**
 * @brief Start the executor, this routine will create attr->executor_count threads
 */
int kupl_executor_start(void);
void kupl_executor_stop(void);
void kupl_executor_enable(int executor_id);
void kupl_executor_disable(int executor_id);
void kupl_executor_set_pf_ult(kupl_ult_h ult, int geid);
kupl_ult_h kupl_executor_get_pf_ult();

#ifdef __cplusplus
}
#endif

#endif
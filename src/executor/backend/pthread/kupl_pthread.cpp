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
#include "kupl_pthread.h"
#include <unistd.h>
#include "executor/kupl_executor.h"
#include "utils/arch/kupl_atomic.h"
#include "utils/config/kupl_config.h"
#include "utils/debug/kupl_assert.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/sys/kupl_hardware.h"
#include "utils/type/kupl_status.h"
#include "tools/profile/kupl_profile.h"
#include "utils/sys/kupl_hardware.h"
#include "utils/sys/kupl_glibc_version.h"

static const size_t KUPL_PTHREAD_EXE_DEFAULD_STACK_SIZE = (8 * 1024 * 1024);
static thread_local int g_eid = KUPL_EIDCID_INIT;

static int kupl_pt_set_global_executor_id(int geid)
{
    g_eid = geid;
    return KUPL_OK;
}

static int kupl_pt_get_global_executor_id()
{
    return g_eid;
}

static int executor_setaffinity(int core_id)
{
    if (kupl_unlikely((core_id < 0) || (core_id >= CPU_SETSIZE))) {
        return kupl_log_error_return(WARN, "KUPL pthread set affinity to core %d failed", core_id);
    }
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    if (kupl_unlikely(pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) != 0)) {
        return kupl_log_error_return(WARN, "KUPL pthread set affinity to core %d failed", core_id);
    }
    return KUPL_OK;
}

static void *executor_body(void *args)
{
    kupl_executor_base_t *exec = (kupl_executor_base_t *)args;
    executor_setaffinity(exec->core_id);
    kupl_set_global_executor_id(exec->executor_id);

    while (!KUPL_ATOMIC_LD_RLX(&exec->stop)) {
        kupl_sched_execute_tb(exec->sched);
    }
    return nullptr;
}

static int executor_backend_init(void *args)
{
    kupl_executor_base_t *exec = (kupl_executor_base_t *)args;
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setstacksize(&thread_attr, KUPL_PTHREAD_EXE_DEFAULD_STACK_SIZE);
    int ret = pthread_create(&exec->thread_id, &thread_attr, executor_body, exec);
    pthread_attr_destroy(&thread_attr);
    if (ret != 0) {
        return KUPL_ERROR;
    }
    return KUPL_OK;
}

static void executor_backend_fini(void *args)
{
    kupl_executor_base_t *exec = (kupl_executor_base_t *)args;
    pthread_join(exec->thread_id, nullptr);
}

void kupl_set_pt_executor_ops(kupl_executor_ops_t &ops)
{
    ops = {.init = executor_backend_init,
           .fini = executor_backend_fini,
           .set_affinity = executor_setaffinity,
           .set_geid = kupl_pt_set_global_executor_id,
           .get_geid = kupl_pt_get_global_executor_id};
}
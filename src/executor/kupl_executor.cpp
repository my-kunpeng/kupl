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
#include "kupl_executor.h"
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include "kupl.h"
#include "core/kupl_core.h"
#include "backend/kupl_executor_backend.h"
#include "tools/profile/kupl_profile.h"

static int g_cpu_count = 0;
static kupl_executor_t *g_executors = nullptr;
static int g_executor_count = 0;
static int g_real_executor_count = 0;
static int g_executor_master_core_id = 0;
static bool g_executor_need_wait = true;
static int g_executor_count_initial = CPU_SETSIZE;
static cpu_set_t g_executor_set;
static cpu_set_t g_executor_set_expand;
static kupl_lock_t *g_executor_lock = nullptr;
static int g_kernel_concurrency = -1;
static thread_local int g_kernel_concurrency_local = -1;

typedef struct kupl_cv_mutex {
    kupl_cv_mutex() : m_mutex(), m_cv() {}
    void wakeup()
    {
        if (!g_executor_need_wait) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_finished = true;
        }
        m_cv.notify_one();
    }
    void wait()
    {
        if (!g_executor_need_wait) {
            return;
        }

        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return this->m_finished; });
        m_finished = false;
    }

    bool m_finished = false;
    std::mutex m_mutex;
    std::condition_variable m_cv;
} kupl_cv_mutex_t;

static kupl_cv_mutex_t *g_executor_cv = nullptr;


int kupl_executor_get_master_core_id()
{
    return g_executor_master_core_id;
}

kupl_executor_t* kupl_executor_get_current_executor()
{
    int eid = kupl_get_global_executor_id();
    if (kupl_unlikely(eid == KUPL_EXECUTOR_DEFAULT || eid >= g_executor_count)) {
        return nullptr;
    }
    return &g_executors[eid];
}

void kupl_executor_set_current_tb(kupl_taskbase_t *tb)
{
    int eid = kupl_get_global_executor_id();
    if (kupl_unlikely(eid == KUPL_EXECUTOR_DEFAULT || eid >= g_executor_count)) {
        return;
    }
    g_executors[eid].exe.current_tb = tb;
}

kupl_taskbase_t* kupl_executor_get_current_tb()
{
    int eid = kupl_get_global_executor_id();
    if (kupl_unlikely(eid == KUPL_EXECUTOR_DEFAULT || eid >= g_executor_count)) {
        return nullptr;
    }
    return g_executors[eid].exe.current_tb;
}

void kupl_executor_set_pf_ult(kupl_ult_h ult, int geid)
{
    if (kupl_unlikely(geid == KUPL_EXECUTOR_DEFAULT || geid >= g_executor_count)) {
        return;
    }
    g_executors[geid].exe.ult = ult;
}

kupl_ult_h kupl_executor_get_pf_ult()
{
    int eid = kupl_get_global_executor_id();
    if (kupl_unlikely(eid == KUPL_EXECUTOR_DEFAULT || eid >= g_executor_count)) {
        return nullptr;
    }
    return g_executors[eid].exe.ult;
}


int kupl_get_num_executors()
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    return g_real_executor_count;
}

int kupl_executor_expand()
{
    if (kupl_unlikely(g_executor_count >= g_cpu_count)) {
        return KUPL_ERROR;
    }
    kupl_executor_base_t *executor = nullptr;
    executor = &g_executors[g_executor_count].exe;
    executor->lock = kupl_lock_create(TICKET_ARRAY_LOCK);
    if (executor->lock == nullptr) {
        return KUPL_ERROR;
    }
    executor->executor_id = g_executor_count;
    executor->core_id = kupl_get_self_affinity();
    executor->stop = true;
    executor->current_tb = nullptr;
    executor->is_master_executor = true;
    executor->sched = kupl_get_global_sched_expand();
    CPU_SET(executor->executor_id, &g_executor_set_expand);
    g_executor_count++;
    kupl_executor_enable(executor->executor_id);
    kupl_set_global_executor_id(executor->executor_id);
    return KUPL_OK;
}

int kupl_get_executor_num()
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    int eid = kupl_get_global_executor_id();
    if (kupl_unlikely(eid == KUPL_EXECUTOR_DEFAULT)) {
        g_executor_lock->lock(g_executor_lock);
        int ret = kupl_sched_expand();
        if (ret == KUPL_ERROR) {
            goto out;
        }
        ret = kupl_memory_expand();
        if (ret == KUPL_ERROR) {
            goto mem_err;
        }
        ret = kupl_executor_expand();
        if (ret == KUPL_ERROR) {
            goto exec_err;
        }
        goto out;
    exec_err:
        kupl_memory_expand_fini();
    mem_err:
        kupl_sched_expand_fini();
    out:
        g_executor_lock->unlock(g_executor_lock);
        eid = kupl_get_global_executor_id();
    }
    if (kupl_unlikely(eid == KUPL_EXECUTOR_DEFAULT || eid >= g_executor_count)) {
        return KUPL_EXECUTOR_DEFAULT;
    }
    return eid;
}

cpu_set_t* kupl_get_global_executor_set()
{
    if (kupl_is_expand_executor()) {
        return &g_executor_set_expand;
    } else {
        return &g_executor_set;
    }
}

bool kupl_is_expand_executor()
{
    int eid = kupl_get_executor_num();
    if (eid >= g_executor_count_initial) {
        return true;
    } else {
        return false;
    }
}

int kupl_get_local_executor_num(int eid)
{
    if (eid >= g_executor_count_initial) {
        return eid - g_executor_count_initial;
    }
    return eid;
}

int kupl_executor_init()
{
    kupl_backend_type_select();
    const kupl_host_info_t *info = kupl_get_host_info();

    g_cpu_count = info->pu_cnt;
    g_executor_count = info->avail_pu_cnt;
    g_executors = (kupl_executor_t *)kupl_calloc(static_cast<size_t>(g_cpu_count), sizeof(kupl_executor_t));
    if (kupl_unlikely(g_executors == nullptr)) {
        return KUPL_ERROR;
    }
    kupl_executor_base_t *executor = nullptr;

    /* get executor wait policy */
    std::string wait_policy = kupl_config_get_value_str(KUPL_EXECUTOR_WAIT_POLICY);
    g_executor_need_wait = (wait_policy == "passive");
    g_executor_cv = new (std::nothrow) kupl_cv_mutex[static_cast<size_t>(g_cpu_count)];
    if (kupl_unlikely(g_executor_cv == nullptr)) {
        goto err;
    }

    /* initialize the executors */
    for (int i = 0; i < g_executor_count; ++i) {
        executor = &g_executors[i].exe;
        executor->lock = kupl_lock_create(TICKET_ARRAY_LOCK);
        if (executor->lock == nullptr) {
            goto err;
        }
        executor->executor_id = i;
        executor->core_id = KUPL_EXECUTOR_DEFAULT;
        executor->stop = true;
        executor->current_tb = nullptr;
        executor->is_master_executor = false;
        executor->sched = kupl_get_global_sched();
    }
    g_executor_count_initial = g_executor_count;
    g_executor_lock = kupl_lock_create(PTHREAD_SPINLOCK);
    if (g_executor_lock == nullptr) {
        goto err;
    }

    return KUPL_OK;

err:
    kupl_executor_fini();
    return kupl_log_error_return(ERROR, "kupl executor init failed");
}

void kupl_executor_fini()
{
    if (g_executors == nullptr) {
        return;
    }
    kupl_lock_cleanup(g_executor_lock);

    /* finalize the executors */
    for (int i = 0; i < g_executor_count; ++i) {
        kupl_executor_base_t *executor = &g_executors[i].exe;
        kupl_lock_cleanup(executor->lock);
        executor->stop = true;
    }

    if (g_executor_cv != nullptr) {
        delete [] g_executor_cv;
        g_executor_cv = nullptr;
    }

    kupl_safe_free(g_executors);
    g_executor_count = 0;
}

int kupl_executor_start()
{
    int kupl_executor_count = kupl_config_get_value(KUPL_EXECUTOR_COUNT);

    int ret = kupl_set_executor_core_mapping();
    if (kupl_unlikely(ret != KUPL_OK)) {
        return kupl_log_error_return(ERROR, "failed to start executor: kupl_set_executor_core_mapping failed");
    }

    const kupl_host_info_t *info = kupl_get_host_info();

    int executor_count = 0;
    for (int i = 0; i < info->pu_conf && executor_count < kupl_executor_count; ++i) {
        if (!CPU_ISSET(i, &info->avail_set)) {
            continue;
        }

        kupl_executor_base_t *executor = &g_executors[executor_count].exe;
        executor->core_id = i;
        executor->executor_id = executor_count;
        kupl_executor_enable(executor->executor_id);
        if (executor_count == 0) {
            executor->is_master_executor = true;
            g_executor_master_core_id = executor->core_id;
            kupl_set_global_executor_id(executor->executor_id);
            kupl_set_affinity(executor->core_id);
        } else {
            if (kupl_unlikely(kupl_backend_init(executor) != 0)) {
                kupl_executor_stop();
                return KUPL_ERROR;
            }
        }
        executor_count++;
    }
    g_real_executor_count = executor_count;

    CPU_ZERO(&g_executor_set);
    for (int j = 0; j < g_real_executor_count; ++j) {
        CPU_SET(j, &g_executor_set);
    }
    CPU_ZERO(&g_executor_set_expand);

    return KUPL_OK;
}

void kupl_executor_stop()
{
    for (int i = 0; i < g_executor_count; ++i) {
        kupl_executor_base_t *executor = &g_executors[i].exe;
        if (executor->thread_id != 0) {
            kupl_executor_disable(i);
            g_executor_cv[i].wakeup();
            kupl_backend_fini(executor);
            executor->thread_id = 0;
        }
    }
}

void kupl_executor_enable(int executor_id)
{
    if (executor_id < 0 || executor_id >= g_executor_count) {
        return;
    }

    g_executors[executor_id].exe.stop = false;
}

void kupl_executor_disable(int executor_id)
{
    if (executor_id < 0 || executor_id >= g_executor_count) {
        return;
    }

    g_executors[executor_id].exe.stop = true;
}

void kupl_set_kernel_concurrency(int num)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return;
    }
    if (num < 1 || num > kupl_get_num_executors()) {
        kupl_warn("number of threads out of the 1..kupl_get_num_executors() range,"
                   " so set the number to kupl_get_num_executors().");
        g_kernel_concurrency = kupl_get_num_executors();
        return;
    }
    g_kernel_concurrency = num;
}

int kupl_get_kernel_concurrency(void)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (g_kernel_concurrency_local != -1) {
        return g_kernel_concurrency_local;
    }
    if (g_kernel_concurrency != -1) {
        return g_kernel_concurrency;
    }
    return kupl_get_num_executors();
}

void kupl_set_kernel_concurrency_local(int num)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return;
    }
    if (num < 1 || num > kupl_get_num_executors()) {
        kupl_warn("number of threads out of the 1..kupl_get_num_executors() range,"
                   " so set the number to kupl_get_num_executors().");
        g_kernel_concurrency_local = kupl_get_num_executors();
        return;
    }
    g_kernel_concurrency_local = num;
}

int kupl_get_kernel_concurrency_local(void)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (g_kernel_concurrency_local != -1) {
        return g_kernel_concurrency_local;
    }
    return kupl_get_num_executors();
}
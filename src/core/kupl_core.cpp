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
#include "kupl_core.h"
#include <cstdio>
#include <mutex>
#include "kupl.h"
#include "dm/memcpy/kupl_memcpy.h"
#include "memory/mpool/kupl_mpool.h"
#include "memory/hbw/kupl_hbw.h"
#include "memory/mem/kupl_mem.h"
#include "executor/kupl_executor.h"
#include "mt/scheduler/kupl_sched.h"
#include "mt/kupl_graph.h"
#include "mt/kupl_queue.h"
#include "mt/kupl_parallel_for.h"
#include "memory/shm/kupl_shm.h"
#include "utils/kupl_utils.h"
#include "tools/kupl_tools.h"
#include "tools/profile/kupl_profile.h"
#include "tools/profile/kupl_profile_trace.h"

volatile bool g_core_inited = false;
static std::mutex g_mtx;

static const kupl_version_t g_version = {
    .product_name = "Kunpeng HPCKit",
    .product_version = "26.1.RC1",
    .component_name = "KUPL",
    .component_abi_version = "1",
    .component_version = "26.1.RC1",
#if defined(__clang__)
    .component_appendinfo = "bisheng",
#elif defined(__GNUC__)
    .component_appendinfo = "gcc",
#endif
};

int kupl_get_version(kupl_version_t *version)
{
    if (version == nullptr) {
        return KUPL_ERROR;
    }
    *version = g_version;
    return KUPL_OK;
}

static kupl_always_inline int kupl_dm_module_init()
{
    if (kupl_unlikely(kupl_memcpy_init() != KUPL_OK)) {
        return KUPL_ERROR;
    }
    return KUPL_OK;
}

static kupl_always_inline void kupl_dm_module_fini()
{
    kupl_memcpy_fini();
}

static kupl_always_inline int kupl_memory_module_init()
{
    if (!g_mpool_inited && kupl_mpool_init() == KUPL_ERROR) {
        goto err_mpool_init;
    }

    if (kupl_unlikely(kupl_mem_init() != KUPL_OK)) {
        goto err_mem_init;
    }

    return KUPL_OK;

err_mem_init:
    kupl_mpool_fini();
err_mpool_init:
    return KUPL_ERROR;
}

static kupl_always_inline void kupl_memory_module_fini()
{
    kupl_mem_fini();
    kupl_mpool_fini();
}

static kupl_always_inline int kupl_mt_module_init()
{
    if (kupl_unlikely(kupl_sched_init() != KUPL_OK)) {
        goto err_sched_init;
    }

    if (kupl_unlikely(kupl_queue_init() != KUPL_OK)) {
        goto err_queue_init;
    }

    return KUPL_OK;

err_queue_init:
    kupl_sched_fini();
err_sched_init:
    return KUPL_ERROR;
}

static kupl_always_inline void kupl_mt_module_fini()
{
    kupl_sched_fini();
    kupl_queue_fini();
}

static kupl_always_inline int kupl_executor_module_init()
{
    if (kupl_unlikely(kupl_executor_init() != KUPL_OK)) {
        goto err_executor_init;
    }

    if (kupl_unlikely(kupl_executor_start() != KUPL_OK)) {
        goto err_executor_start;
    }
    kupl_barrier_init();

    return KUPL_OK;

err_executor_start:
    kupl_executor_fini();
err_executor_init:
    return KUPL_ERROR;
}

static kupl_always_inline void kupl_executor_module_fini()
{
    kupl_executor_stop();
    kupl_executor_fini();
}

int kupl_init()
{
    std::unique_lock<std::mutex> lk(g_mtx, std::try_to_lock);
    if (!lk.owns_lock()) {
        return KUPL_OK;
    }

    if (kupl_unlikely(!g_utils_inited && kupl_utils_init() != KUPL_OK)) {
        kupl_error("Initialize utils module failed");
        goto err_utils_init;
    }

    if (kupl_unlikely(!g_tools_inited && kupl_tools_init() != KUPL_OK)) {
        kupl_error("Initialize tools module failed");
        goto err_tools_init;
    }

    if (kupl_unlikely(kupl_sdma_module_init() != KUPL_OK)) {
        kupl_error("Initialize sdma module failed");
        goto err_sdma_init;
    }

    if (kupl_unlikely(kupl_memory_module_init() != KUPL_OK)) {
        kupl_error("Initialize memory module failed");
        goto err_memory_init;
    }

    if (kupl_unlikely(kupl_dm_module_init() != KUPL_OK)) {
        kupl_error("Initialize dm module failed");
        goto err_dm_init;
    }

    if (kupl_unlikely(kupl_mt_module_init() != KUPL_OK)) {
        kupl_error("Initialize mt module failed");
        goto err_mt_init;
    }

    if (kupl_unlikely(kupl_executor_module_init() != KUPL_OK)) {
        kupl_error("Initialize executor module failed");
        goto err_executor_init;
    }

    if (kupl_unlikely(kupl_pf_init() != KUPL_OK)) {
        kupl_error("Initialize parallel for failed");
        goto err_pf_init;
    }

    g_core_inited = true;
    return KUPL_OK;

err_pf_init:
    kupl_executor_module_fini();
err_executor_init:
    kupl_mt_module_fini();
err_mt_init:
    kupl_dm_module_fini();
err_dm_init:
    kupl_memory_module_fini();
err_memory_init:
    kupl_sdma_module_fini();
err_sdma_init:
    kupl_tools_fini();
err_tools_init:
    kupl_utils_fini();
err_utils_init:
    return KUPL_ERROR;
}

__attribute__((destructor)) void kupl_fini()
{
    if (!g_core_inited) {
        return;
    }
    kupl_global_graph_destroy();
    kupl_pf_fini();
    kupl_executor_module_fini();
    kupl_mt_module_fini();
    if (g_mpool_inited) {
        kupl_mpool_fini();
    }
    kupl_dm_module_fini();
    kupl_sdma_module_fini();
    if (g_tools_inited) {
        kupl_tools_fini();
    }
    if (g_utils_inited) {
        kupl_utils_fini();
    }
    g_core_inited = false;
}
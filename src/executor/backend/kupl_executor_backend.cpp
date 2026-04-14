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
#include "kupl_executor_backend.h"
#include <cstdio>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include "omp/kupl_omp.h"
#include "pthread/kupl_pthread.h"
#include "utils/config/kupl_config.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/type/kupl_status.h"
#include "utils/debug/kupl_log.h"
#include "utils/sys/kupl_glibc_version.h"
#include "utils/sys/kupl_hardware.h"
#include "utils/debug/kupl_log.h"

static kupl_backend_type_t g_kupl_backend_type = KUPL_BACKEND_INIT;
static kupl_executor_ops_t g_kupl_executor_ops = {};
static int g_eid2cid[CPU_SETSIZE];
static int g_cid2eid[CPU_SETSIZE];

void kupl_backend_type_select()
{
    std::string backend_type_str = kupl_config_get_value_str(KUPL_EXECUTOR_BACKEND);
    kupl_backend_type_set(backend_type_str);
}

void kupl_backend_type_set(std::string &backend_type_str)
{
    if (backend_type_str == std::string("pthread")) {
        kupl_set_pt_executor_ops(g_kupl_executor_ops);
        g_kupl_backend_type = KUPL_BACKEND_PTHREAD;
    } else {
        if (backend_type_str != std::string("omp")) {
            kupl_warn("Unsupported executor backend, use omp as default.");
        }
        kupl_set_omp_executor_ops(g_kupl_executor_ops);
        g_kupl_backend_type = KUPL_BACKEND_OMP;
    }
}

kupl_backend_type_t kupl_backend_type_get()
{
    return g_kupl_backend_type;
}

int kupl_global_eid2cid(int geid)
{
    return g_eid2cid[geid];
}

int kupl_global_cid2eid(int gcid)
{
    return g_cid2eid[gcid];
}

int kupl_set_global_executor_id(int geid)
{
    if (kupl_unlikely(g_kupl_executor_ops.set_geid == nullptr)) {
        return KUPL_ERROR;
    }
    return g_kupl_executor_ops.set_geid(geid);
}

int kupl_get_global_executor_id()
{
    if (kupl_unlikely(g_kupl_executor_ops.get_geid == nullptr)) {
        return KUPL_EIDCID_INIT;
    }
    return g_kupl_executor_ops.get_geid();
}

int kupl_get_global_core_id()
{
    thread_local int current_cpu = -1;
    if (current_cpu != -1) {
        return current_cpu;
    }
    current_cpu = sched_getcpu();
    return current_cpu;
}

int kupl_set_affinity(int core_id)
{
    if (kupl_unlikely(g_kupl_executor_ops.set_affinity == nullptr)) {
        return KUPL_ERROR;
    }
    return g_kupl_executor_ops.set_affinity(core_id);
}

int kupl_backend_init(void *exec)
{
    if (kupl_unlikely(exec == nullptr)) {
        return KUPL_ERROR;
    }
    if (g_kupl_executor_ops.init != nullptr) {
        return g_kupl_executor_ops.init(exec);
    }
    return KUPL_OK;
}

void kupl_backend_fini(void *exec)
{
    if (kupl_unlikely(exec == nullptr)) {
        return;
    }
    if (g_kupl_executor_ops.fini != nullptr) {
        g_kupl_executor_ops.fini(exec);
    }
}

int kupl_set_executor_core_mapping()
{
    static bool inited = false;

    if (inited) {
        return KUPL_OK;
    }

    const kupl_host_info_t *info = kupl_get_host_info();
    int cur_thread_id = 0;
    for (int i = 0; i < info->pu_conf; i++) {
        if (CPU_ISSET(i, &info->avail_set)) {
            g_eid2cid[cur_thread_id] = i;
            g_cid2eid[i] = cur_thread_id;
            cur_thread_id++;
        }
    }

    inited = true;
    return KUPL_OK;
}

int kupl_get_self_affinity()
{
    static thread_local int core_id = KUPL_EIDCID_INIT;

    if (core_id != KUPL_EIDCID_INIT) {
        return core_id;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    pthread_getaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    for (int i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, &cpuset)) {
            core_id = i;
            return core_id;
        }
    }
    return core_id;
}

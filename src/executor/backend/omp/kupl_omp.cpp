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
#include "kupl_omp.h"
#include <unistd.h>
#include <dlfcn.h>
#include <omp.h>
#include "utils/config/kupl_config.h"
#include "utils/debug/kupl_assert.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/type/kupl_status.h"
#include "utils/sys/kupl_glibc_version.h"

static thread_local int g_eid = KUPL_EIDCID_INIT;

static int kupl_omp_set_global_executor_id(int geid)
{
    g_eid = geid;
    return KUPL_OK;
}

static int kupl_omp_get_global_executor_id()
{
    if (kupl_unlikely(g_eid == KUPL_EIDCID_INIT)) {
        g_eid = omp_get_thread_num();
    }
    return g_eid;
}

void kupl_set_omp_executor_ops(kupl_executor_ops_t &ops)
{
    ops = {
        .init               = nullptr,
        .fini               = nullptr,
        .set_affinity       = nullptr,
        .set_geid           = kupl_omp_set_global_executor_id,
        .get_geid           = kupl_omp_get_global_executor_id,
    };
}
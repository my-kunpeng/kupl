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
#include "kupl_tools.h"
#include "kupl.h"
#include "utils/sys/kupl_compiler.h"
#include "tools/profile/kupl_profile.h"
#include "tools/profile/kupl_profile_trace.h"

bool g_tools_inited = false;

int kupl_tools_init()
{
    profile_module_init();
    if (kupl_unlikely(kupl_ptrace_init() != KUPL_OK)) {
        goto err_ptrace_init;
    }
    g_tools_inited = true;
    return KUPL_OK;

err_ptrace_init:
    profile_module_fini();
    return KUPL_ERROR;
}

void kupl_tools_fini()
{
    kupl_ptrace_fini();
    profile_module_fini();
}

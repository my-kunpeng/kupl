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
#include "kupl_ult.h"
#include "mt/kupl_graph.h"
#include "executor/kupl_executor.h"
#include "executor/backend/kupl_executor_backend.h"
#include "utils/arch/kupl_atomic.h"
#include "tools/profile/kupl_profile.h"
#include "tools/profile/kupl_profile_trace.h"

kupl_ult_h kupl_ult_create()
{
    int geid = kupl_get_executor_num();
    KUPL_PTRACE_START(KUPL_PTRACE_ULT_CREATE);
    kupl_ult_h ult = (kupl_ult_h)kupl_memory_calloc(sizeof(kupl_ult_t), geid);
    if (kupl_unlikely(ult == nullptr)) {
        kupl_warn("ult create failed.");
        return nullptr;
    }
    ult->tb.type = KUPL_TB_TYPE_ULT;
    ult->tb.ops = &ult_ops;
    ult->tb.ref = 1;
    KUPL_ATOMIC_ST(&ult->tb.state, KUPL_TB_STATE_CREATED);
    KUPL_PTRACE_END(KUPL_PTRACE_ULT_CREATE);
    kupl_debug("ult_create");
    return ult;
}

kupl_ult_h kupl_ult_init(kupl_ult_param_t *param, int geid)
{
    kupl_ult_h ult;
    KUPL_PTRACE_START(KUPL_PTRACE_ULT_INIT);
    if (param->inplace != nullptr) {
        ult = (kupl_ult_h)param->inplace;
        ult->tb.ref += 1;
    } else {
        ult = (kupl_ult_h)kupl_memory_alloc(sizeof(kupl_ult_t), geid);
        if (kupl_unlikely(ult == nullptr)) {
            return nullptr;
        }
        ult->tb.ref = 1;
    }
    kupl_tb_init(&ult->tb, &param->super, geid);
    ult->kind = param->kind;
    ult->tb.ops = &ult_ops;
    KUPL_ATOMIC_ST(&ult->tb.state, KUPL_TB_STATE_INIT);
    KUPL_PTRACE_END(KUPL_PTRACE_ULT_INIT);
    kupl_debug("ult_init");
    return ult;
}

void kupl_ult_cleanup(kupl_ult_h ult)
{
    if (kupl_unlikely(ult == nullptr)) {
        return;
    }
    KUPL_PTRACE_START(KUPL_PTRACE_ULT_CLEANUP);
    kupl_ult_deref(&ult->tb, kupl_get_executor_num());
    KUPL_PTRACE_END(KUPL_PTRACE_ULT_CLEANUP);
}

int kupl_ult_invoke(kupl_taskbase_t *tb)
{
    KUPL_PTRACE_START(KUPL_PTRACE_ULT_INVOKE);
    tb->func(tb->args);
    KUPL_PTRACE_END(KUPL_PTRACE_ULT_INVOKE);
    return 1;
}
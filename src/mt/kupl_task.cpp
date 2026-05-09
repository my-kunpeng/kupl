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
#include "kupl_task.h"
#include "mt/kupl_dag.h"
#include "mt/kupl_graph.h"
#include "mt/kupl_static_graph.h"
#include "executor/kupl_executor.h"
#include "executor/backend/kupl_executor_backend.h"
#include "memory/mpool/kupl_mpool.h"
#include "utils/arch/kupl_atomic.h"
#include "tools/struct/kupl_vla.h"
#include "tools/profile/kupl_profile.h"
#include "tools/profile/kupl_profile_trace.h"

kupl_task_h kupl_task_create_with_udata(size_t udata_size)
{
    if (kupl_unlikely(udata_size >= KUPL_MAX_MALLOC_SIZE)) {
        return nullptr;
    }
    int geid = kupl_get_executor_num();
    KUPL_PTRACE_START(KUPL_PTRACE_TASK_CREATE);
    kupl_task_h task = (kupl_task_h)kupl_memory_calloc(sizeof(kupl_task_t) + udata_size, geid);
    if (kupl_unlikely(task == nullptr)) {
        kupl_warn("task create failed");
        return nullptr;
    }
    task->tb.type = KUPL_TB_TYPE_TASK;
    task->tb.ops = &task_ops;
    task->tb.ref = 1;
    KUPL_ATOMIC_ST(&task->tb.state, KUPL_TB_STATE_CREATED);
    KUPL_PTRACE_END(KUPL_PTRACE_TASK_CREATE);
    kupl_debug("task_create");
    return task;
}

kupl_task_h kupl_task_create()
{
    return kupl_task_create_with_udata(0);
}

kupl_task_h kupl_task_init(kupl_task_param_t *param, int geid)
{
    kupl_task_h task;
    KUPL_PTRACE_START(KUPL_PTRACE_TASK_INIT);
    if (param->inplace != nullptr) {
        task = param->inplace;
        task->tb.ref += 1;
    } else {
        task = (kupl_task_h)kupl_memory_alloc(sizeof(kupl_task_t) + param->udata_size, geid);
        if (kupl_unlikely(task == nullptr)) {
            kupl_warn("task init failed");
            return nullptr;
        }
        task->tb.ref = 1;
    }
    kupl_tb_init(&task->tb, &param->super, geid);
    task->kind = param->kind;
    task->tb.ops = &task_ops;
    if (KUPL_ERROR == kupl_gnode_init(task->gnode)) {
        kupl_task_cleanup(task);
        return nullptr;
    }
    KUPL_ATOMIC_ST(&task->tb.state, KUPL_TB_STATE_INIT);
    KUPL_PTRACE_END(KUPL_PTRACE_TASK_INIT);
    return task;
}

void kupl_task_cleanup(kupl_task_h task)
{
    if (kupl_unlikely(task == nullptr)) {
        return;
    }
    KUPL_PTRACE_START(KUPL_PTRACE_TASK_CLEANUP);
    kupl_task_deref(&task->tb, kupl_get_executor_num());
    KUPL_PTRACE_END(KUPL_PTRACE_TASK_CLEANUP);
}

static kupl_always_inline bool kupl_task_execute(kupl_task_t *task)
{
    bool finished = false;

    KUPL_PTRACE_START(KUPL_PTRACE_TASK_EXECUTE);
    task->tb.func(task->tb.args);
    KUPL_ATOMIC_OR_RLS(&task->tb.state, KUPL_TB_STATE_FINISHED);
    finished = true;
    KUPL_PTRACE_END(KUPL_PTRACE_TASK_EXECUTE);

    return finished;
}

static kupl_always_inline kupl_task_h kupl_task_finish(kupl_task_h task)
{
    kupl_task_t *next_task = nullptr;

    KUPL_PTRACE_START(KUPL_PTRACE_TASK_FINISH);
    if (kupl_unlikely(task->tb.flag & KUPL_TB_FLAG_REUSE)) {
        return next_task;
    }

    // task reset
    if (task->kind & KUPL_TASK_KIND_STATIC_MASK) {
        task->tb.state = KUPL_TB_STATE_INIT;
        kupl_gnode_reset(&task->gnode);
    }

    kupl_graph_h graph;
    uint32_t n_ready_tasks = task->gnode.n_successors;
    kupl_vla<kupl_task_t *> ready_tasks(n_ready_tasks);
    if (kupl_unlikely(ready_tasks.get_data() == nullptr)) {
        return next_task;
    }

    switch (task->kind) {
        case KUPL_TASK_KIND_COMM_STATIC: {
            graph = task->tb.sgraph->graph;
            n_ready_tasks = kupl_gnode_release_ready(&task->gnode, ready_tasks.get_data());
            break;
        }
        default: {
            graph = task->tb.graph;
            n_ready_tasks = kupl_gnode_release_ready_safe(&task->gnode, ready_tasks.get_data());
        }
    }

    for (uint32_t i = 0; i < n_ready_tasks; i++) {
        kupl_sched_add_tb(graph->sched, &(ready_tasks[i]->tb));
    }

    // update taskbase count
    kupl_tb_count_sub(&task->tb);

    // task deref
    if (task->kind & KUPL_TASK_KIND_DYNAMIC_MASK) {
        kupl_task_deref(&task->tb, kupl_get_executor_num());
    }
    KUPL_PTRACE_END(KUPL_PTRACE_TASK_FINISH);

    return next_task;
}

int kupl_task_invoke(kupl_taskbase_t *tb)
{
    if (tb == nullptr) {
        return 0;
    }
    KUPL_PTRACE_START(KUPL_PTRACE_TASK_INVOKE);
    kupl_task_h task = (kupl_task_h)tb;
    kupl_taskbase_t *suspend_tb = nullptr;

    if (kupl_executor_get_current_executor() != nullptr) {
        suspend_tb = kupl_executor_get_current_tb();
        kupl_executor_set_current_tb(&task->tb);
    }
    PROFILE_CODE_START(task_execute);
    kupl_task_execute(task);
    PROFILE_CODE_END(task_execute);

    PROFILE_CODE_START(task_finish);
    kupl_task_h next_task = kupl_task_finish(task);
    (void)next_task;
    PROFILE_CODE_END(task_finish);

    if (kupl_executor_get_current_executor() != nullptr) {
        kupl_executor_set_current_tb(suspend_tb);
    }
    KUPL_PTRACE_END(KUPL_PTRACE_TASK_INVOKE);

    return 1;
}

int kupl_task_wait(kupl_task_h task)
{
    if (kupl_unlikely(task == nullptr || (task->tb.state & KUPL_TB_STATE_INIT) == 0)) {
        return KUPL_ERROR;
    }
    KUPL_PTRACE_START(KUPL_PTRACE_TASK_WAIT);
    kupl_graph_h graph = nullptr;
    if (task->kind & KUPL_TASK_KIND_DYNAMIC_MASK) {
        graph = task->tb.graph;
    } else {
        return KUPL_ERROR;
    }

    while (kupl_tb_test(&task->tb) == KUPL_AGAIN) {
        if (kupl_sched_execute_tb(graph->sched) == KUPL_FINISHED) {
            break;
        }
    }
    KUPL_PTRACE_END(KUPL_PTRACE_TASK_WAIT);
    return KUPL_OK;
}

namespace kupl {

void lambda_func(void *args)
{
    auto data = (lambda_func_data *)args;
    data->func();
}

} // namespace kupl
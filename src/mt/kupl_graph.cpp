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

#include "kupl_graph.h"
#include <unistd.h>
#include "kupl.h"
#include "core/kupl_core.h"
#include "mt/kupl_ult.h"
#include "mt/kupl_task.h"
#include "mt/kupl_static_graph.h"
#include "mt/kupl_check.h"
#include "executor/kupl_executor.h"
#include "executor/kupl_executor_group.h"
#include "executor/backend/kupl_executor_backend.h"
#include "utils/sys/kupl_math.h"

kupl_graph_h kupl_graph_create(kupl_egroup_h egroup)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return nullptr;
    }

    kupl_sched_t* sched;
    if (kupl_is_expand_executor()) {
        sched = kupl_get_global_sched_expand();
    } else {
        sched = kupl_get_global_sched();
    }
    if (kupl_unlikely(sched == nullptr)) {
        return nullptr;
    }
    kupl_graph_h graph = (kupl_graph_h)kupl_calloc(1, sizeof(kupl_graph_t));
    if (kupl_unlikely(graph == nullptr)) {
        return nullptr;
    }
    int geid = kupl_get_executor_num();
    graph->dag = kupl_dag_create(geid);
    if (kupl_unlikely(graph->dag == nullptr)) {
        kupl_graph_destroy(graph);
        return nullptr;
    }

    graph->sched = sched;
    CPU_ZERO(&graph->eid_set);
    if (egroup != nullptr) {
        if (CPU_COUNT(&egroup->cur.eid_set) != 0) {
            CPU_OR(&graph->eid_set, &graph->eid_set, &egroup->cur.eid_set);
        } else {
            kupl_warn("graph executor group invalid.");
            kupl_graph_destroy(graph);
            return nullptr;
        }
    } else {
        /* the default affinity is using all executors */
        CPU_OR(&graph->eid_set, &graph->eid_set, kupl_get_global_executor_set());
    }
    return graph;
}

void kupl_graph_destroy(kupl_graph_h graph)
{
    if (kupl_unlikely(graph == nullptr)) {
        return;
    }

    int geid = kupl_get_executor_num();
    if (graph->dag != nullptr) {
        kupl_dag_destroy(graph->dag, geid);
        graph->dag = nullptr;
    }

    kupl_free_inner(graph);
}

kupl_graph_h kupl_get_current_graph()
{
    if (kupl_executor_get_current_executor() == nullptr) {
        kupl_error("the thread is not bind to a executor");
        return nullptr;
    }
    kupl_taskbase_t *current_tb = kupl_executor_get_current_tb();
    if (current_tb == nullptr) {
        return nullptr;
    } else {
        if (kupl_static_task_check(current_tb)) {
            return current_tb->sgraph->graph;
        } else {
            return current_tb->graph;
        }
    }
}

int kupl_graph_add_task(kupl_graph_h graph, kupl_task_desc_t *task_desc)
{
    // check params
    if (kupl_unlikely(graph == nullptr || task_desc == nullptr || task_desc->func == nullptr)) {
        return kupl_log_error_return(ERROR, "graph add task params invalid");
    }
    int geid = kupl_get_executor_num();
    if (kupl_unlikely(geid == KUPL_EXECUTOR_DEFAULT)) {
        return kupl_log_error_return(ERROR, "invoke KUPL functions on threads not managed by KUPL");
    }
    kupl_tb_desc_t *user_desc = (kupl_tb_desc_t*)task_desc;
    kupl_task_param_t task_param = {
        .super = {
            .type       = KUPL_TB_TYPE_TASK,
            .user_desc  = user_desc,
            .graph      = graph,
            .count      = &graph->count,
        },
        .kind           = KUPL_TASK_KIND_COMM_DYNAMIC,
        .inplace        = nullptr,
        .udata_size     = 0,
    };
    kupl_task_t *task = kupl_task_init(&task_param, geid);
    if (kupl_unlikely(task == nullptr)) {
        return kupl_log_error_return(ERROR, "task create failed");
    }
    if (task_desc->field_mask & KUPL_TASK_DESC_FIELD_DEP) {
        if (kupl_unlikely(task_desc->ndep != 0 && task_desc->dep_list == nullptr)) {
            return kupl_log_error_return(ERROR, "dep_list is nullptr");
        }
        int ret = kupl_dag_add_task(graph->dag, task, task_desc->dep_list, (uint32_t)task_desc->ndep, geid);
        if (ret == KUPL_ERROR) {
            return KUPL_ERROR;
        } else if (ret == KUPL_DAG_TASK_NOT_READY) {
            return KUPL_OK;
        }
    }
    kupl_sched_add_tb(graph->sched, &task->tb);
    return KUPL_OK;
}

int kupl_graph_add_sgraph_task(kupl_graph_h graph, kupl_sgraph_task_desc_t *task_desc)
{
    if (kupl_unlikely((task_desc == nullptr) || (task_desc->sgraph == nullptr))) {
        return KUPL_ERROR;
    }
    int geid = kupl_get_executor_num();
    if (kupl_unlikely(geid == KUPL_EXECUTOR_DEFAULT)) {
        return kupl_log_error_return(ERROR, "invoke KUPL functions on threads not managed by KUPL");
    }
    task_desc->sgraph->graph = graph;
    kupl_tb_desc_t user_desc = {
        .field_mask = task_desc->field_mask,
        .func = kupl_sgraph_task_body,
        .args = task_desc->sgraph,
        .name = task_desc->name,
        .priority = task_desc->priority,
        .flag = task_desc->flag,
    };
    kupl_task_param_t task_param = {
        .super = {
            .type       = KUPL_TB_TYPE_TASK,
            .user_desc  = &user_desc,
            .graph      = graph,
            .count      = &graph->count,
        },
        .kind           = KUPL_TASK_KIND_COMM_DYNAMIC,
        .inplace        = nullptr,
        .udata_size     = 0,
    };
    kupl_task_t *task = kupl_task_init(&task_param, geid);
    if (kupl_unlikely(task == nullptr)) {
        return kupl_log_error_return(ERROR, "task create failed");
    }
    kupl_sched_add_tb(graph->sched, &task->tb);
    return KUPL_OK;
}

typedef struct kupl_taskloop_info {
    int dim;
    int64_t blocks[KUPL_MAX_DIM_SIZE];
    int64_t esize;
    int64_t total_tasks;
    int64_t min_tasks;
    int64_t max_tasks;
    int64_t extra_tasks;
} kupl_taskloop_info_t;

typedef struct kupl_taskloop_args {
    kupl_nd_range_t             range;
    kupl_taskloop_func_t        func;
    void                        *args;
} kupl_taskloop_args_t;

void kupl_taskloop_func(void *args)
{
    auto loop_args = (kupl_taskloop_args *)args;
    loop_args->func(&loop_args->range, loop_args->args);
}

static kupl_always_inline
int kupl_taskloop_check(kupl_graph_h graph, kupl_taskloop_desc_t *desc)
{
    if (kupl_unlikely(graph == nullptr || desc == nullptr || desc->field_mask != KUPL_TASKLOOP_DESC_FIELD_DEFAULT
                      || desc->func == nullptr || desc->egroup == nullptr || desc->range == nullptr)) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(kupl_egroup_get_cur_size(desc->egroup) == 0)) {
        return KUPL_ERROR;
    }
    return kupl_check_range(desc->range, KUPL_LOOP_POLICY_TASK);
}

static kupl_always_inline
int kupl_taskloop_linear(kupl_graph_h graph, kupl_taskloop_desc_t *desc, kupl_taskloop_info_t *info)
{
    int geid = kupl_get_executor_num();
    if (kupl_unlikely(geid == KUPL_EXECUTOR_DEFAULT)) {
        return kupl_log_error_return(ERROR, "invoke KUPL functions on threads not managed by KUPL");
    }
    kupl_range_t *nd_range = desc->range->nd_range;
    kupl_tb_desc_t user_desc = {
        .field_mask     = KUPL_TB_DESC_FIELD_EXECUTOR_ID,
        .func           = kupl_taskloop_func,
        .args           = nullptr,
        .executor_id    = 0,
    };
    kupl_task_param_t task_param = {
        .super = {
            .type       = KUPL_TB_TYPE_TASK,
            .user_desc  = &user_desc,
            .graph      = graph,
            .count      = &graph->count,
        },
        .kind           = KUPL_TASK_KIND_COMM_DYNAMIC,
        .inplace        = nullptr,
        .udata_size     = sizeof(kupl_taskloop_args_t),
    };
    // 5 tasks assigned to 3 threads
    // min_tasks: 1, max_tasks: 2, extra_tasks: 2
    //             t0 t1 t2
    //           +-0  1  3 - min_tasks
    // max_tasks-+
    //           +-X  2  4
    //                +--+
    //              extra_tasks

    for (int64_t i = 0; i < info->max_tasks; i++) {
        int64_t task_id = i;
        KUPL_FOR_EACH_EGROUP(desc->egroup, eid, eidx, {
            if (i == info->min_tasks && ((eidx + info->extra_tasks) < info->esize)) {
                task_id += info->min_tasks;
                eidx++; // continue will escape eidx++, so do it here.
                continue;
            }
            user_desc.executor_id = (int)eid;
            kupl_task_h task_inner = kupl_task_init(&task_param, geid);
            if (kupl_unlikely(task_inner == nullptr)) {
                return kupl_log_error_return(ERROR, "taskloop inner task create failed");
            }
            task_inner->tb.args = task_inner->udata;
            kupl_taskloop_args_t *taskloop_args = reinterpret_cast<kupl_taskloop_args_t *>(task_inner->udata);
            taskloop_args->func = desc->func;
            taskloop_args->args = desc->args;
            kupl_range_t *nd_range_args = taskloop_args->range.nd_range;
            taskloop_args->range.dim = info->dim;

            int64_t task_id_tmp = task_id;
            int64_t block_idx = 0;
            for (int d = 0; d < info->dim; d++) {
                block_idx = task_id_tmp % info->blocks[d];
                task_id_tmp /= info->blocks[d];
                kupl_range_t &range = nd_range_args[d];

                range.lower = nd_range[d].lower + block_idx * nd_range[d].blocksize;
                if (nd_range[d].blocksize > 0) {
                    range.upper = kupl_min(nd_range[d].upper, range.lower + nd_range[d].blocksize);
                } else {
                    range.upper = kupl_max(nd_range[d].upper, range.lower + nd_range[d].blocksize);
                }
                range.step = nd_range[d].step;
                range.blocksize = nd_range[d].blocksize;
            }
            kupl_sched_add_tb(graph->sched, &task_inner->tb);

            // update task_id
            task_id += info->min_tasks + ((eidx + info->extra_tasks) >= info->esize);
        });
    }
    return KUPL_OK;
}

typedef struct kupl_taskloop_dispatch_args {
    uint32_t                eid;
    uint32_t                eidx;
    kupl_graph_h            graph;
    kupl_taskloop_desc_t    *desc;
    kupl_taskloop_info_t    info;
} kupl_taskloop_dispatch_args_t;


static void kupl_taskloop_dispatch(void *dispatch_args)
{
    auto args = (kupl_taskloop_dispatch_args_t *)dispatch_args;
    int geid = kupl_get_executor_num();
    kupl_range_t *nd_range = args->desc->range->nd_range;
    kupl_taskloop_info_t &info = args->info;

    int64_t tasks_begin;
    int64_t tasks_end;
    if (info.extra_tasks != 0 && args->eidx >= info.extra_tasks) {
        tasks_begin = args->eidx * info.min_tasks +  info.extra_tasks;
        tasks_end = tasks_begin + info.min_tasks;
    } else {
        tasks_begin = args->eidx * info.max_tasks;
        tasks_end = tasks_begin + info.max_tasks;
    }
    kupl_tb_desc_t user_desc = {
        .field_mask     = KUPL_TB_DESC_FIELD_EXECUTOR_ID,
        .func           = kupl_taskloop_func,
        .args           = nullptr,
        .executor_id    = (int)args->eid,
    };
    kupl_task_param_t task_param = {
        .super = {
            .type       = KUPL_TB_TYPE_TASK,
            .user_desc  = &user_desc,
            .graph      = args->graph,
            .count      = &args->graph->count,
        },
        .kind           = KUPL_TASK_KIND_COMM_DYNAMIC,
        .inplace        = nullptr,
        .udata_size     = sizeof(kupl_taskloop_args_t),
    };
    for (int64_t task_id = tasks_begin; task_id < tasks_end; task_id++) {
        kupl_task_h task_inner = kupl_task_init(&task_param, geid);
        if (kupl_unlikely(task_inner == nullptr)) {
            kupl_error("taskloop dispatch inner task create failed");
            return;
        }
        task_inner->tb.args = task_inner->udata;
        kupl_taskloop_args_t *taskloop_args = reinterpret_cast<kupl_taskloop_args_t *>(task_inner->udata);
        taskloop_args->func = args->desc->func;
        taskloop_args->args = args->desc->args;
        kupl_range_t *nd_range_args = taskloop_args->range.nd_range;
        taskloop_args->range.dim = info.dim;
        int64_t task_id_tmp = task_id;
        int64_t block_idx = 0;
        for (int d = 0; d < info.dim; d++) {
            block_idx = task_id_tmp % info.blocks[d];
            task_id_tmp /= info.blocks[d];
            kupl_range_t &range = nd_range_args[d];

            range.lower = nd_range[d].lower + block_idx * nd_range[d].blocksize;
            if (nd_range[d].blocksize > 0) {
                range.upper = kupl_min(nd_range[d].upper, range.lower + nd_range[d].blocksize);
            } else {
                range.upper = kupl_max(nd_range[d].upper, range.lower + nd_range[d].blocksize);
            }
            range.step = nd_range[d].step;
            range.blocksize = nd_range[d].blocksize;
        }
        kupl_sched_add_tb(args->graph->sched, &task_inner->tb);
    }
}

/**
 * @brief The taskloop is split into num_executors dispatch tasks, which are responsible for
          concurrently dispatching tasks to the intended threads.
 */
static kupl_always_inline
int kupl_taskloop_concurrent(kupl_graph_h graph, kupl_taskloop_desc_t *desc, kupl_taskloop_info_t *info)
{
    int geid = kupl_get_executor_num();
    if (kupl_unlikely(geid == KUPL_EXECUTOR_DEFAULT)) {
        kupl_error("invoke KUPL functions on threads not managed by KUPL");
        return KUPL_ERROR;
    }
    kupl_tb_desc_t user_desc = {
        .field_mask     = KUPL_TB_DESC_FIELD_EXECUTOR_ID,
        .func           = kupl_taskloop_dispatch,
        .args           = nullptr,
        .executor_id    = 0,
    };
    kupl_task_param_t task_param = {
        .super = {
            .type       = KUPL_TB_TYPE_TASK,
            .user_desc  = &user_desc,
            .graph      = graph,
            .count      = &graph->count,
        },
        .kind           = KUPL_TASK_KIND_COMM_DYNAMIC,
        .inplace        = nullptr,
        .udata_size     = sizeof(kupl_taskloop_dispatch_args_t),
    };
    KUPL_FOR_EACH_EGROUP_REV(desc->egroup, eid, eidx, {
        user_desc.executor_id = (int)eid;
        kupl_task_h task = kupl_task_init(&task_param, geid);
        if (kupl_unlikely(task == nullptr)) {
            return kupl_log_error_return(ERROR, "taskloop distribute task create failed");
        }
        task->tb.args = task->udata;
        auto args = reinterpret_cast<kupl_taskloop_dispatch_args_t *>(task->udata);
        args->eid = eid;
        args->eidx = eidx;
        args->graph = graph;
        args->desc = desc;
        args->info = *info;
        kupl_sched_add_tb(graph->sched, &task->tb);
    });
    return KUPL_OK;
}

const static int taskloop_threshold = 5;

int kupl_graph_add_taskloop(kupl_graph_h graph, kupl_taskloop_desc_t *desc)
{
    if (kupl_unlikely(kupl_taskloop_check(graph, desc) == KUPL_ERROR)) {
        return kupl_log_error_return(ERROR, "graph add taskloop params invalid");
    }
    kupl_taskloop_info_t info;
    info.esize = kupl_egroup_get_cur_size(desc->egroup);
    info.dim = desc->range->dim;
    // calculate blocks of each dim and total tasks
    kupl_range_t *nd_range = desc->range->nd_range;
    info.total_tasks = 1;
    for (int d = 0; d < info.dim; d++) {
        kupl_range_t &range = nd_range[d];
        info.blocks[d] = ((range.upper - range.lower + range.blocksize - 1) / range.blocksize);
        info.total_tasks *= info.blocks[d];
    }
    info.min_tasks = info.total_tasks / info.esize;
    info.extra_tasks = info.total_tasks % info.esize;
    info.max_tasks =  info.min_tasks + (info.extra_tasks > 0);

    if (info.esize * taskloop_threshold > info.total_tasks) {
        return kupl_taskloop_linear(graph, desc, &info);
    } else {
        return kupl_taskloop_concurrent(graph, desc, &info);
    }
}

void kupl_graph_wait(kupl_graph_h graph)
{
    if (graph == nullptr) {
        return;
    }
    while (KUPL_ATOMIC_LD_RLX(&graph->count) != 0) {
        if (kupl_sched_execute_tb(graph->sched) == KUPL_FINISHED) {
            break;
        }
    }
}

int kupl_graph_submit(kupl_graph_h graph, kupl_task_info_t *info)
{
    if (kupl_unlikely((graph == nullptr) || (info == nullptr))) {
        return KUPL_ERROR;
    }
    switch (info->type) {
        case KUPL_TASK_TYPE_SINGLE:
            return kupl_graph_add_task(graph, (kupl_task_desc_t *)info->desc);
        case KUPL_TASK_TYPE_SGRAPH:
            return kupl_graph_add_sgraph_task(graph, (kupl_sgraph_task_desc_t *)info->desc);
        case KUPL_TASK_TYPE_TASKLOOP:
            return kupl_graph_add_taskloop(graph, (kupl_taskloop_desc_t *)info->desc);
        default:
            return kupl_log_error_return(ERROR, "invalid kupl task type.");
    }
}

static kupl_graph_t *g_graph = nullptr;
static kupl_graph_t *g_graph_expand = nullptr;
static kupl_lock_guard_data_t g_graph_guard_data;
static kupl_lock_guard_data_t g_graph_guard_data_expand;

kupl_graph_h kupl_get_global_graph(void)
{
    if (kupl_is_expand_executor()) {
        if (g_graph_expand != nullptr) {
            return g_graph_expand;
        }
        kupl_lock_guard_t guard(&g_graph_guard_data_expand.lock);
        if (g_graph_expand == nullptr) {
            g_graph_expand = kupl_graph_create(KUPL_ALL_EXECUTORS);
            if (g_graph_expand == nullptr) {
                kupl_error("global graph expand failed");
            }
        }
        return g_graph_expand;
    } else {
        if (g_graph != nullptr) {
            return g_graph;
        }
        kupl_lock_guard_t guard(&g_graph_guard_data.lock);
        if (g_graph == nullptr) {
            g_graph = kupl_graph_create(KUPL_ALL_EXECUTORS);
            if (g_graph == nullptr) {
                kupl_error("global graph create failed");
            }
        }
        return g_graph;
    }
}

void kupl_global_graph_destroy(void)
{
    if (g_graph != nullptr) {
        kupl_graph_wait(g_graph);
        kupl_graph_destroy(g_graph);
    }
    if (g_graph_expand != nullptr) {
        kupl_graph_wait(g_graph_expand);
        kupl_graph_destroy(g_graph_expand);
    }
}

namespace kupl {

    int graph_submit(kupl_graph_h graph, kupl_task_desc_t *desc,
                     const std::function<void(void)> &func)
    {
        // check params
        if (kupl_unlikely(graph == nullptr || desc == nullptr ||func == nullptr)) {
            return kupl_log_error_return(ERROR, "graph add task params invalid");
        }
        int geid = kupl_get_executor_num();
        if (kupl_unlikely(geid == KUPL_EXECUTOR_DEFAULT)) {
            return kupl_log_error_return(ERROR, "invoke KUPL functions on threads not managed by KUPL");
        }
        kupl_task_h task = (kupl_task_h)kupl_memory_alloc(sizeof(kupl_task_t) + sizeof(lambda_func_data), geid);
        if (kupl_unlikely(task == nullptr)) {
            kupl_warn("task init failed");
            return KUPL_ERROR;
        }
        task->tb.ref = 1;
        KUPL_ATOMIC_ST(&task->tb.state, KUPL_TB_STATE_INIT);

        lambda_func_data *data = reinterpret_cast<lambda_func_data *>(task->udata);
        memset((void*)data, 0, sizeof(lambda_func_data));
        data->func = func;
        desc->func = lambda_func;
        desc->args = data;
        kupl_task_param_t task_param = {
            .super = {
                .type       = KUPL_TB_TYPE_TASK,
                .user_desc  = (kupl_tb_desc_t*)desc,
                .graph      = graph,
                .count      = &graph->count,
            },
            .kind           = KUPL_TASK_KIND_COMM_DYNAMIC,
            .inplace        = task,
            .udata_size     = sizeof(lambda_func_data),
        };
        kupl_tb_init(&task->tb, &task_param.super, geid);
        task->kind = task_param.kind;
        task->tb.ops = &task_ops;
        if (KUPL_ERROR == kupl_gnode_init(task->gnode)) {
            kupl_task_cleanup(task);
            return KUPL_ERROR;
        }
        if (desc->field_mask & KUPL_TASK_DESC_FIELD_DEP) {
            if (kupl_unlikely(desc->ndep != 0 && desc->dep_list == nullptr)) {
                return kupl_log_error_return(ERROR, "dep_list is nullptr");
            }
            int ret = kupl_dag_add_task(graph->dag, task, desc->dep_list, (uint32_t)desc->ndep, geid);
            if (ret == KUPL_ERROR) {
                return KUPL_ERROR;
            } else if (ret == KUPL_DAG_TASK_NOT_READY) {
                return KUPL_OK;
            }
        }
        kupl_sched_add_tb(graph->sched, &task->tb);
        return KUPL_OK;
    }

    static int taskloop_check(kupl_graph_h graph, kupl_taskloop_desc_t *desc)
    {
        if (kupl_unlikely(graph == nullptr || desc == nullptr || desc->field_mask != KUPL_TASKLOOP_DESC_FIELD_DEFAULT
                        || desc->egroup == nullptr || desc->range == nullptr)) {
            return KUPL_ERROR;
        }
        if (kupl_unlikely(kupl_egroup_get_cur_size(desc->egroup) == 0)) {
            return KUPL_ERROR;
        }
        return kupl_check_range(desc->range, KUPL_LOOP_POLICY_TASK);
    }

    using taskloop_lambda = std::function<void(const kupl_nd_range_t *)>;

    struct kupl_taskloop_lambda_args {
        kupl_nd_range_t             range;
        taskloop_lambda             lambda_func;
    };
    using kupl_taskloop_lambda_args_t = struct kupl_taskloop_lambda_args;

    void kupl_taskloop_lambda_func(void *args)
    {
        auto loop_args = (kupl_taskloop_lambda_args *)args;
        loop_args->lambda_func(&loop_args->range);
    }

    static int taskloop_linear(kupl_graph_h graph, kupl_taskloop_desc_t *desc, kupl_taskloop_info_t *info,
                               taskloop_lambda func)
    {
        int geid = kupl_get_executor_num();
        if (kupl_unlikely(geid == KUPL_EXECUTOR_DEFAULT)) {
            return kupl_log_error_return(ERROR, "invoke KUPL functions on threads not managed by KUPL");
        }
        kupl_range_t *nd_range = desc->range->nd_range;
        kupl_tb_desc_t user_desc = {
            .field_mask     = KUPL_TB_DESC_FIELD_EXECUTOR_ID,
            .func           = kupl_taskloop_lambda_func,
            .args           = nullptr,
            .executor_id    = 0,
        };
        kupl_task_param_t task_param = {
            .super = {
                .type       = KUPL_TB_TYPE_TASK,
                .user_desc  = &user_desc,
                .graph      = graph,
                .count      = &graph->count,
            },
            .kind           = KUPL_TASK_KIND_COMM_DYNAMIC,
            .inplace        = nullptr,
            .udata_size     = sizeof(kupl_taskloop_lambda_args_t),
        };
        // 5 tasks assigned to 3 threads
        // min_tasks: 1, max_tasks: 2, extra_tasks: 2
        //             t0 t1 t2
        //           +-0  1  3 - min_tasks
        // max_tasks-+
        //           +-X  2  4
        //                +--+
        //              extra_tasks

        for (int64_t i = 0; i < info->max_tasks; i++) {
            int64_t task_id = i;
            KUPL_FOR_EACH_EGROUP(desc->egroup, eid, eidx, {
                if (i == info->min_tasks && ((eidx + info->extra_tasks) < info->esize)) {
                    task_id += info->min_tasks;
                    eidx++; // continue will escape eidx++, so do it here.
                    continue;
                }
                user_desc.executor_id = (int)eid;
                kupl_task_h task_inner = kupl_task_init(&task_param, geid);
                if (kupl_unlikely(task_inner == nullptr)) {
                    return kupl_log_error_return(ERROR, "taskloop inner task create failed");
                }
                task_inner->tb.args = task_inner->udata;
                kupl_taskloop_lambda_args_t *taskloop_args =
                reinterpret_cast<kupl_taskloop_lambda_args_t *>(task_inner->udata);
                memset((void*)taskloop_args, 0, sizeof(kupl_taskloop_lambda_args_t));
                taskloop_args->lambda_func = func;
                kupl_range_t *nd_range_args = taskloop_args->range.nd_range;
                taskloop_args->range.dim = info->dim;

                int64_t task_id_tmp = task_id;
                int64_t block_idx = 0;
                for (int d = 0; d < info->dim; d++) {
                    block_idx = task_id_tmp % info->blocks[d];
                    task_id_tmp /= info->blocks[d];
                    kupl_range_t &range = nd_range_args[d];

                    range.lower = nd_range[d].lower + block_idx * nd_range[d].blocksize;
                    if (nd_range[d].blocksize > 0) {
                        range.upper = kupl_min(nd_range[d].upper, range.lower + nd_range[d].blocksize);
                    } else {
                        range.upper = kupl_max(nd_range[d].upper, range.lower + nd_range[d].blocksize);
                    }
                    range.step = nd_range[d].step;
                    range.blocksize = nd_range[d].blocksize;
                }
                kupl_sched_add_tb(graph->sched, &task_inner->tb);

                // update task_id
                task_id += info->min_tasks + ((eidx + info->extra_tasks) >= info->esize);
            });
        }
        return KUPL_OK;
    }

    struct kupl_taskloop_dispatch_lambda_args {
        uint32_t                eid;
        uint32_t                eidx;
        kupl_graph_h            graph;
        kupl_taskloop_desc_t    *desc;
        kupl_taskloop_info_t    info;
        taskloop_lambda         lambda_func;
    };
    using kupl_taskloop_dispatch_lambda_args_t = struct kupl_taskloop_dispatch_lambda_args;

    static void taskloop_dispatch(void *dispatch_args)
    {
        auto args = (kupl_taskloop_dispatch_lambda_args_t *)dispatch_args;
        int geid = kupl_get_executor_num();
        kupl_range_t *nd_range = args->desc->range->nd_range;
        kupl_taskloop_info_t &info = args->info;

        int64_t tasks_begin;
        int64_t tasks_end;
        if (info.extra_tasks != 0 && args->eidx >= info.extra_tasks) {
            tasks_begin = args->eidx * info.min_tasks +  info.extra_tasks;
            tasks_end = tasks_begin + info.min_tasks;
        } else {
            tasks_begin = args->eidx * info.max_tasks;
            tasks_end = tasks_begin + info.max_tasks;
        }
        kupl_tb_desc_t user_desc = {
            .field_mask     = KUPL_TB_DESC_FIELD_EXECUTOR_ID,
            .func           = kupl_taskloop_lambda_func,
            .args           = nullptr,
            .executor_id    = (int)args->eid,
        };
        kupl_task_param_t task_param = {
            .super = {
                .type       = KUPL_TB_TYPE_TASK,
                .user_desc  = &user_desc,
                .graph      = args->graph,
                .count      = &args->graph->count,
            },
            .kind           = KUPL_TASK_KIND_COMM_DYNAMIC,
            .inplace        = nullptr,
            .udata_size     = sizeof(kupl_taskloop_lambda_args_t),
        };
        for (int64_t task_id = tasks_begin; task_id < tasks_end; task_id++) {
            kupl_task_h task_inner = kupl_task_init(&task_param, geid);
            if (kupl_unlikely(task_inner == nullptr)) {
                kupl_error("taskloop dispatch inner task create failed");
                return;
            }
            task_inner->tb.args = task_inner->udata;
            kupl_taskloop_lambda_args_t *taskloop_args =
            reinterpret_cast<kupl_taskloop_lambda_args_t *>(task_inner->udata);
            memset((void*)taskloop_args, 0, sizeof(kupl_taskloop_lambda_args_t));
            taskloop_args->lambda_func = args->lambda_func;
            kupl_range_t *nd_range_args = taskloop_args->range.nd_range;
            taskloop_args->range.dim = info.dim;
            int64_t task_id_tmp = task_id;
            int64_t block_idx = 0;
            for (int d = 0; d < info.dim; d++) {
                block_idx = task_id_tmp % info.blocks[d];
                task_id_tmp /= info.blocks[d];
                kupl_range_t &range = nd_range_args[d];

                range.lower = nd_range[d].lower + block_idx * nd_range[d].blocksize;
                if (nd_range[d].blocksize > 0) {
                    range.upper = kupl_min(nd_range[d].upper, range.lower + nd_range[d].blocksize);
                } else {
                    range.upper = kupl_max(nd_range[d].upper, range.lower + nd_range[d].blocksize);
                }
                range.step = nd_range[d].step;
                range.blocksize = nd_range[d].blocksize;
            }
            kupl_sched_add_tb(args->graph->sched, &task_inner->tb);
        }
    }

    static int taskloop_concurrent(kupl_graph_h graph, kupl_taskloop_desc_t *desc, kupl_taskloop_info_t *info,
                                   taskloop_lambda func)
    {
        int geid = kupl_get_executor_num();
        if (kupl_unlikely(geid == KUPL_EXECUTOR_DEFAULT)) {
            kupl_error("invoke KUPL functions on threads not managed by KUPL");
            return KUPL_ERROR;
        }
        kupl_tb_desc_t user_desc = {
            .field_mask     = KUPL_TB_DESC_FIELD_EXECUTOR_ID,
            .func           = taskloop_dispatch,
            .args           = nullptr,
            .executor_id    = 0,
        };
        kupl_task_param_t task_param = {
            .super = {
                .type       = KUPL_TB_TYPE_TASK,
                .user_desc  = &user_desc,
                .graph      = graph,
                .count      = &graph->count,
            },
            .kind           = KUPL_TASK_KIND_COMM_DYNAMIC,
            .inplace        = nullptr,
            .udata_size     = sizeof(kupl_taskloop_dispatch_lambda_args_t),
        };
        KUPL_FOR_EACH_EGROUP_REV(desc->egroup, eid, eidx, {
            user_desc.executor_id = (int)eid;
            kupl_task_h task = kupl_task_init(&task_param, geid);
            if (kupl_unlikely(task == nullptr)) {
                return kupl_log_error_return(ERROR, "taskloop distribute task create failed");
            }
            task->tb.args = task->udata;
            auto args = reinterpret_cast<kupl_taskloop_dispatch_lambda_args_t *>(task->udata);
            memset((void*)args, 0, sizeof(kupl_taskloop_dispatch_lambda_args_t));
            args->eid = eid;
            args->eidx = eidx;
            args->graph = graph;
            args->desc = desc;
            args->info = *info;
            args->lambda_func = func;
            kupl_sched_add_tb(graph->sched, &task->tb);
        });
        return KUPL_OK;
    }

    int graph_submit(kupl_graph_h graph, kupl_taskloop_desc_t *desc,
                     const std::function<void(const kupl_nd_range_t *)> &func)
    {
        if (kupl_unlikely(func == nullptr || taskloop_check(graph, desc) == KUPL_ERROR)) {
            return kupl_log_error_return(ERROR, "graph add taskloop params invalid");
        }
        kupl_taskloop_info_t info;
        info.esize = kupl_egroup_get_cur_size(desc->egroup);
        info.dim = desc->range->dim;
        // calculate blocks of each dim and total tasks
        kupl_range_t *nd_range = desc->range->nd_range;
        info.total_tasks = 1;
        for (int d = 0; d < info.dim; d++) {
            kupl_range_t &range = nd_range[d];
            info.blocks[d] = ((range.upper - range.lower + range.blocksize - 1) / range.blocksize);
            info.total_tasks *= info.blocks[d];
        }
        info.min_tasks = info.total_tasks / info.esize;
        info.extra_tasks = info.total_tasks % info.esize;
        info.max_tasks =  info.min_tasks + (info.extra_tasks > 0);

        if (info.esize * taskloop_threshold > info.total_tasks) {
            return taskloop_linear(graph, desc, &info, func);
        } else {
            return taskloop_concurrent(graph, desc, &info, func);
        }
    }
}
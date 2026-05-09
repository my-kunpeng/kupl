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
#include "kupl_static_graph.h"
#include "core/kupl_core.h"
#include "executor/backend/kupl_executor_backend.h"
#include "executor/kupl_executor_group.h"
#include "mt/kupl_task.h"
#include "mt/kupl_graph.h"
#include "utils/debug/kupl_log.h"
#include "utils/sys/kupl_compiler.h"

kupl_sgraph_h kupl_sgraph_create()
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return nullptr;
    }
    int geid = kupl_get_executor_num();
    auto sgraph = (kupl_sgraph_h)kupl_memory_alloc(sizeof(kupl_sgraph_t), geid);
    if (kupl_unlikely(sgraph == nullptr)) {
        return nullptr;
    }
    sgraph->src_nodes = new (std::nothrow) kupl_node_task_map_t();
    if (kupl_unlikely(sgraph->src_nodes == nullptr)) {
        kupl_memory_free(sgraph, geid);
        return nullptr;
    }
    sgraph->graph = nullptr;
    sgraph->nodes = nullptr;
    sgraph->task_count = 0;
    sgraph->task_id = 0;
    kupl_debug("static graph create");
    return sgraph;
}

kupl_sgraph_node_h kupl_sgraph_add_node(kupl_sgraph_h sgraph, kupl_sgraph_node_desc_t *desc)
{
    if (kupl_unlikely(sgraph == nullptr || desc == nullptr || desc->func == nullptr)) {
        kupl_error("static graph add norm node params invalid.");
        return nullptr;
    }
    int geid = kupl_get_executor_num();

    kupl_tb_desc_t tb_desc = {
        .field_mask = desc->field_mask,
        .func = desc->func,
        .args = desc->args,
        .name = desc->name,
        .priority = desc->priority,
        .flag = desc->flag,
        .egroup = desc->egroup,
    };
    if ((desc->field_mask & KUPL_SGRAPH_NODE_DESC_FIELD_EGROUP) && (desc->egroup != nullptr)) {
        tb_desc.field_mask |= KUPL_TB_DESC_FIELD_EXECUTOR_ID;
        tb_desc.executor_id = (int)kupl_egroup_master_eid(desc->egroup);
    }
    kupl_task_param_t task_param = {
        .super =
            {
                .type = KUPL_TB_TYPE_TASK,
                .user_desc = &tb_desc,
                .sgraph = sgraph,
                .count = &sgraph->task_count,
            },
        .kind = KUPL_TASK_KIND_COMM_STATIC,
        .inplace = nullptr,
        .udata_size = 0,
    };

    kupl_task_t *task = kupl_task_init(&task_param, geid);
    if (kupl_unlikely(task == nullptr)) {
        kupl_error("task create failed");
        return nullptr;
    }
    task->tb.id = sgraph->task_id++;
    kupl_slist_insert_front(&sgraph->nodes, &task->gnode, geid);
    sgraph->src_nodes->emplace(task->tb.id, task);
    return (kupl_sgraph_node_h)&task->gnode;
}

int kupl_sgraph_add_dep(kupl_sgraph_node_h precede, kupl_sgraph_node_h succeed)
{
    if (kupl_unlikely((precede == nullptr) || (succeed == nullptr))) {
        return KUPL_ERROR;
    }
    auto precede_task = kupl_gnode_get_task(precede);
    auto succeed_task = kupl_gnode_get_task(succeed);
    if (precede_task->tb.sgraph != succeed_task->tb.sgraph) {
        return KUPL_ERROR;
    }
    kupl_sgraph_t *sgraph = precede_task->tb.sgraph;
    int geid = kupl_get_executor_num();
    uint32_t n = kupl_gnode_precede((kupl_gnode_t *)precede, (kupl_gnode_t *)succeed, geid);
    (void)n;
    sgraph->src_nodes->erase(succeed_task->tb.id);
    return KUPL_OK;
}

void kupl_sgraph_destroy(kupl_sgraph_h sgraph)
{
    if (kupl_unlikely(sgraph == nullptr)) {
        return;
    }
    int geid = kupl_get_executor_num();
    if (sgraph->src_nodes != nullptr) {
        delete sgraph->src_nodes;
    }

    // clean up all tasks in static graph
    kupl_gnode_t *node = nullptr;
    kupl_task_t *task = nullptr;
    while (sgraph->nodes != nullptr) {
        node = (kupl_gnode_t *)sgraph->nodes->data;
        task = kupl_gnode_get_task(node);
        kupl_task_deref(&task->tb, geid);
        kupl_slist_destroy(&sgraph->nodes, geid);
    }
    kupl_memory_free(sgraph, geid);
    kupl_debug("cleanup static graph");
}

void kupl_sgraph_task_body(void *args)
{
    auto sgraph = (kupl_sgraph_t *)args;
    auto sched = sgraph->graph->sched;
    for (auto &pair : *sgraph->src_nodes) {
        kupl_sched_add_tb(sched, &pair.second->tb);
    }
    while (KUPL_ATOMIC_LD_RLX(&sgraph->task_count) != 0) {
        if (kupl_sched_execute_tb(sched) == KUPL_FINISHED) {
            break;
        }
    }
}

namespace kupl {

kupl_sgraph_node_h sgraph_add_node(kupl_sgraph_h sgraph, kupl_sgraph_node_desc_t *desc,
                                   const std::function<void(void)> &func)
{
    if (kupl_unlikely(sgraph == nullptr || desc == nullptr || func == nullptr)) {
        kupl_error("static graph add norm node params invalid.");
        return nullptr;
    }
    int geid = kupl_get_executor_num();

    kupl_task_h task = (kupl_task_h)kupl_memory_alloc(sizeof(kupl_task_t) + sizeof(lambda_func_data), geid);
    if (kupl_unlikely(task == nullptr)) {
        kupl_warn("task init failed");
        return nullptr;
    }
    task->tb.ref = 1;

    lambda_func_data *data = reinterpret_cast<lambda_func_data *>(task->udata);
    memset((void *)data, 0, sizeof(lambda_func_data));
    data->func = func;
    desc->func = lambda_func;
    desc->args = data;

    kupl_tb_desc_t tb_desc = {
        .field_mask = desc->field_mask,
        .func = lambda_func,
        .args = data,
        .name = desc->name,
        .priority = desc->priority,
        .flag = desc->flag,
    };
    if ((desc->field_mask & KUPL_SGRAPH_NODE_DESC_FIELD_EGROUP) && (desc->egroup != nullptr)) {
        tb_desc.field_mask |= KUPL_TB_DESC_FIELD_EXECUTOR_ID;
        tb_desc.executor_id = (int)kupl_egroup_master_eid(desc->egroup);
    }

    kupl_task_param_t task_param = {.super = {
                                        .type = KUPL_TB_TYPE_TASK,
                                        .user_desc = &tb_desc,
                                        .sgraph = sgraph,
                                        .count = &sgraph->task_count,
                                    }};
    kupl_tb_init(&task->tb, &task_param.super, geid);
    task->kind = KUPL_TASK_KIND_COMM_STATIC;
    task->tb.ops = &task_ops;
    if (KUPL_ERROR == kupl_gnode_init(task->gnode)) {
        kupl_task_cleanup(task);
        return nullptr;
    }
    task->tb.id = sgraph->task_id++;
    kupl_slist_insert_front(&sgraph->nodes, &task->gnode, geid);
    sgraph->src_nodes->emplace(task->tb.id, task);

    return (kupl_sgraph_node_h)&task->gnode;
}

} // namespace kupl

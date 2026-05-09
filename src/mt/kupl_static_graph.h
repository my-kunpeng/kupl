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
#ifndef KUPL_STATIC_GRAPH_H
#define KUPL_STATIC_GRAPH_H

#include <atomic>
#include <memory>
#include <map>
#include "kupl_task.h"

typedef std::map<uint64_t, kupl_task *> kupl_node_task_map_t;

typedef struct kupl_sgraph {
    struct kupl_graph *graph;        // specifies which dynamic graph to add
    kupl_node_task_map_t *src_nodes; // stores nodes with no dependency
    kupl_slist_t *nodes;             // stores all node resources in the static graph
    uint32_t task_id;                // stores the key of src_nodes
    KUPL_ATOMIC_UINT32 task_count;   // indicates the number of tasks that have not been executed.
} kupl_sgraph_t;

void kupl_sgraph_task_body(void *args);

#endif
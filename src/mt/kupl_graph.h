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
#ifndef KUPL_GRAPH_H
#define KUPL_GRAPH_H

#include "kupl.h"
#include "mt/kupl_dag.h"
#include "mt/scheduler/kupl_sched.h"
#include "utils/arch/kupl_atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kupl_graph {
    kupl_dag_t *dag;          // graph dependency
    kupl_sched_t *sched;      // scheduler
    KUPL_ATOMIC_UINT32 count; // task count
    cpu_set_t eid_set;        // graph affinity
} kupl_graph_t;

/**
 * @brief   get the current Graph
 *
 * @return  the grpah the current task belongs to
 */
kupl_graph_h kupl_get_current_graph(void);

int kupl_graph_add_task(kupl_graph_h graph, kupl_task_desc_t *task_desc);

int kupl_graph_add_sgraph_task(kupl_graph_h graph, kupl_sgraph_task_desc_t *task_desc);

int kupl_graph_add_taskloop(kupl_graph_h graph, kupl_taskloop_desc_t *desc);

kupl_graph_h kupl_get_global_graph(void);

void kupl_global_graph_destroy(void);

#ifdef __cplusplus
}
#endif

#endif
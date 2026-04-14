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
#include "kupl_dag.h"
#include "mt/kupl_task.h"

uint32_t kupl_gnode_precede(kupl_gnode_t *precede, kupl_gnode_t *succeed, int geid)
{
    if (kupl_unlikely(precede == nullptr || succeed == nullptr)) {
        return 0;
    }
    // The previous task may have already been executed
    kupl_task_t *precede_task = kupl_gnode_get_task(precede);
    if (kupl_tb_is_finished(&precede_task->tb)) {
        return 0;
    }

    /* add dep in reversed order */
    precede->lock->lock(precede->lock);
    if (kupl_tb_is_finished(&precede_task->tb)) {
        precede->lock->unlock(precede->lock);
        return 0;
    }
    precede->n_successors++;
    kupl_slist_insert_front(&precede->successors.head, succeed, geid);
    precede->lock->unlock(precede->lock);

    succeed->n_hard_predecessors += 1;
    KUPL_ATOMIC_ADD_RLX(&succeed->n_predecessors, 1);
    return 1;
}

/**
 * @brief release all successors of gnode, return the ready tasks.
 */
static kupl_always_inline
uint32_t gnode_release_ready(kupl_gnode_t *gnode, kupl_task_t **ready_tasks)
{
    if (!kupl_gnode_has_deps(gnode)) {
        return 0;
    }
    uint32_t n_ready_tasks = 0;
    kupl_gnode_t *curr_gnode = nullptr;
    kupl_slist_t *curr_list = gnode->successors.head;
    while (curr_list != nullptr) {
        curr_gnode = (kupl_gnode_t*)curr_list->data;
        uint32_t ready = (KUPL_ATOMIC_SUB_RLX(&curr_gnode->n_predecessors, 1u) == 1);
        if (ready) {
            *ready_tasks = kupl_gnode_get_task(curr_gnode);
            ready_tasks += ready;
            n_ready_tasks += ready;
        }
        curr_list = curr_list->next;
    }
    return n_ready_tasks;
}

uint32_t kupl_gnode_release_ready(kupl_gnode_t *gnode, kupl_task_t **ready_tasks)
{
    return gnode_release_ready(gnode, ready_tasks);
}

uint32_t kupl_gnode_release_ready_safe(kupl_gnode_t *gnode, kupl_task_t **ready_tasks)
{
    gnode->lock->lock(gnode->lock);
    gnode->lock->unlock(gnode->lock);
    return gnode_release_ready(gnode, ready_tasks);
}
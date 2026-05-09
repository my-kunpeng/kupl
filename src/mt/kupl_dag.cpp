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

static kupl_always_inline kupl_gnode_t *kupl_gnode_ref(kupl_gnode_t *node)
{
    if (kupl_likely(node != nullptr)) {
        kupl_task_ref(&(kupl_gnode_get_task(node)->tb));
    }
    return node;
}

static kupl_always_inline void kupl_gnode_deref(kupl_gnode_t *node, int geid)
{
    if (kupl_likely(node != nullptr)) {
        kupl_task_deref(&(kupl_gnode_get_task(node)->tb), geid);
    }
}

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

static kupl_always_inline uint32_t node_process_dep_out(kupl_addr_entry_t *entry, kupl_gnode_t *node, int geid)
{
    uint32_t n_predecessors = 0;
    if (entry->in) {
        kupl_slist_t *precede_list = entry->in;
        kupl_gnode_t *precede;
        while (precede_list != nullptr) {
            precede = (kupl_gnode_t *)precede_list->data;
            n_predecessors += kupl_gnode_precede(precede, node, geid);
            kupl_gnode_deref(precede, geid);
            kupl_slist_destroy(&precede_list, geid);
        }
        entry->in = nullptr;
    } else {
        n_predecessors += kupl_gnode_precede(entry->out, node, geid);
    }
    // replace entry->out with node
    kupl_gnode_deref(entry->out, geid);
    entry->out = kupl_gnode_ref(node);
    return n_predecessors;
}

static kupl_always_inline uint32_t node_process_dep_in(kupl_addr_entry_t *entry, kupl_gnode_t *node, int geid)
{
    kupl_gnode_ref(node);
    kupl_slist_insert_front(&entry->in, node, geid);
    return kupl_gnode_precede(entry->out, node, geid);
}

static kupl_always_inline int kupl_node_process_dep(kupl_gnode_t *node, kupl_dag_t *dag, kupl_task_dep_t *dep, int geid)
{
    auto hash = dag->hash;
    kupl_addr_entry_t *entry = nullptr;
    uint32_t n_predecessors = 0;

    auto iter = hash->find(dep->base_addr);
    if (iter != hash->end()) {
        entry = iter->second;
    } else {
        entry = kupl_addr_entry_create(geid);
        hash->insert({dep->base_addr, entry});
        if (dep->base_addr == KUPL_TASK_DEP_ALL_ADDR) {
            goto process_dep;
        }
        auto depall = hash->find(KUPL_TASK_DEP_ALL_ADDR);
        if (depall == hash->end()) {
            goto process_dep;
        }
        auto depall_entry = (*depall).second;
        if (depall_entry->out == nullptr) {
            goto process_dep;
        }
        // task is not dep_all, and dep_all task exist
        // then check if task is finished
        if (kupl_tb_is_finished(&kupl_gnode_get_task(depall_entry->out)->tb)) {
            kupl_gnode_deref(depall_entry->out, geid);
            depall_entry->out = nullptr;
        } else {
            entry->out = kupl_gnode_ref((*depall).second->out);
        }
    }
process_dep:
    if (kupl_unlikely(entry == nullptr)) {
        return KUPL_ERROR;
    }
    switch (dep->type) {
        case KUPL_TASK_DEP_TYPE_IN:
            n_predecessors = node_process_dep_in(entry, node, geid);
            break;
        case KUPL_TASK_DEP_TYPE_OUT:
        case KUPL_TASK_DEP_TYPE_INOUT:
            n_predecessors = node_process_dep_out(entry, node, geid);
            break;
        case KUPL_TASK_DEP_TYPE_ALL:
            for (auto &it : *hash) {
                n_predecessors += node_process_dep_out(it.second, node, geid);
            }
            break;
        default:
            kupl_error("task dep type(%d) unknow.", dep->type);
            return KUPL_ERROR;
    }
    return (int)n_predecessors;
}

/**
 * @brief release all successors of gnode, return the ready tasks.
 */
static kupl_always_inline uint32_t gnode_release_ready(kupl_gnode_t *gnode, kupl_task_t **ready_tasks)
{
    if (!kupl_gnode_has_deps(gnode)) {
        return 0;
    }
    uint32_t n_ready_tasks = 0;
    kupl_gnode_t *curr_gnode = nullptr;
    kupl_slist_t *curr_list = gnode->successors.head;
    while (curr_list != nullptr) {
        curr_gnode = (kupl_gnode_t *)curr_list->data;
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

kupl_addr_entry_t *kupl_addr_entry_create(int geid)
{
    auto entry = (kupl_addr_entry_t *)kupl_memory_alloc(sizeof(kupl_addr_entry_t), geid);
    if (kupl_unlikely(entry == nullptr)) {
        kupl_error("addr entry create failed.");
        return nullptr;
    }
    entry->out = nullptr;
    entry->in = nullptr;
    kupl_debug("addr_entry_create.");
    return entry;
}

void kupl_addr_entry_destroy(kupl_addr_entry_t *entry, int geid)
{
    if (kupl_unlikely(entry == nullptr)) {
        kupl_error("parmas invalid.");
        return;
    }

    node_process_dep_out(entry, nullptr, geid);
    kupl_debug("addr_entry_destroy.");
    kupl_memory_free(entry, geid);
}

/*=====  dag  ======*/

kupl_dag_t *kupl_dag_create(int geid)
{
    auto dag = (kupl_dag_t *)kupl_memory_alloc(sizeof(kupl_dag_t), geid);
    if (kupl_unlikely(dag == nullptr)) {
        kupl_error("dag create failed.");
        return nullptr;
    }
    dag->hash = new (std::nothrow) kupl_dag_hash_t;
    if (kupl_unlikely(dag->hash == nullptr)) {
        kupl_memory_free(dag, geid);
        kupl_error("dag->hash create failed.");
        return nullptr;
    }
    pthread_spin_init(&dag->lock, 0);

    kupl_debug("dag_create.");
    return dag;
}

void kupl_dag_destroy(kupl_dag_t *dag, int geid)
{
    if (kupl_unlikely(dag == nullptr)) {
        kupl_error("parmas invalid.");
        return;
    }
    pthread_spin_destroy(&dag->lock);
    for (auto &it : *(dag->hash)) {
        kupl_addr_entry_destroy(it.second, geid);
    }
    delete dag->hash;
    kupl_debug("dag_destroy.");
    kupl_memory_free(dag, geid);
}

static kupl_always_inline void dep_list_rm_dup(kupl_task_dep_t *task_dep_list, kupl_task_dep_t *dep_list,
                                               uint32_t &dep_cnt)
{
    /* process dep */
    uint64_t hash = 0;
    kupl_task_dep_t *task_dep;
    size_t count = 0;
    for (size_t i = 0; i < dep_cnt; i++) {
        task_dep = task_dep_list + i;
        if (kupl_unlikely((task_dep->type == KUPL_TASK_DEP_TYPE_ALL) ||
                          (task_dep->base_addr == KUPL_TASK_DEP_ALL_ADDR))) {
            dep_list[0].base_addr = KUPL_TASK_DEP_ALL_ADDR;
            dep_list[0].type = KUPL_TASK_DEP_TYPE_ALL;
            dep_cnt = 1;
            break;
        }
        if ((hash & (uint64_t)(task_dep->base_addr)) != (uint64_t)(task_dep->base_addr)) {
            hash |= (uint64_t)(task_dep->base_addr);
            dep_list[count++] = *task_dep;
        } else {
            bool found_same = false;
            for (size_t j = 0; j < count; j++) {
                if (dep_list[j].base_addr == task_dep->base_addr) {
                    if (dep_list[j].type != task_dep->type) {
                        dep_list[j].type = KUPL_TASK_DEP_TYPE_OUT;
                        found_same = true;
                    }
                    break;
                }
            }
            if (!found_same) {
                dep_list[count++] = *task_dep;
            }
        }
    }
}

int kupl_dag_add_task(kupl_dag_t *dag, kupl_task_t *task, kupl_task_dep_t *task_dep_list, uint32_t task_dep_cnt,
                      int geid)
{
    bool use_alloc = false;
    kupl_task_dep_t local_dep_list[KUPL_MAX_LOCAL_DEP_COUNT];
    kupl_task_dep_t *dep_list = &local_dep_list[0];
    uint32_t dep_cnt = task_dep_cnt;
    if (kupl_unlikely(dep_cnt > KUPL_MAX_LOCAL_DEP_COUNT)) {
        dep_list = (kupl_task_dep_t *)kupl_memory_alloc(sizeof(kupl_task_dep_t) * dep_cnt, geid);
        if (dep_list == nullptr) {
            return KUPL_ERROR;
        }
        use_alloc = true;
    }
    dep_list_rm_dup(task_dep_list, dep_list, dep_cnt);

    kupl_gnode_t *node = &task->gnode;
    int task_is_ready = KUPL_DAG_TASK_NOT_READY;
    /* process deps */
    uint32_t n_predecessors = 0;
    for (size_t i = 0; i < dep_cnt; i++) {
        int ret = kupl_node_process_dep(node, dag, (dep_list + i), geid);
        if (kupl_unlikely(ret == KUPL_ERROR)) {
            task_is_ready = KUPL_ERROR;
            goto out;
        }
        n_predecessors += (uint32_t)ret;
    }

    if (node->n_predecessors + n_predecessors == -n_predecessors) {
        task_is_ready = KUPL_DAG_TASK_READY;
    }

out:
    if (use_alloc) {
        kupl_memory_free(dep_list, geid);
    }
    return task_is_ready;
}
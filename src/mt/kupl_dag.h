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
#ifndef KUPL_DAG_H
#define KUPL_DAG_H

#include <cstdint>
#include <unordered_map>
#include <limits.h>
#include "utils/arch/kupl_atomic.h"
#include "utils/debug/kupl_log.h"
#include "utils/lock/kupl_lock.h"
#include "tools/struct/kupl_list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kupl_task kupl_task_t;

// Definition of the graph Data Structure
typedef struct kupl_gnode {
    struct {
        kupl_slist_t *head;
        kupl_slist_t *tail;
    } successors;
    uint32_t            n_successors;
    uint32_t            n_hard_predecessors;
    uint32_t            n_soft_predecessors;
    KUPL_ATOMIC_UINT32 n_predecessors;
    kupl_lock_t        *lock;
} kupl_gnode_t;

static kupl_always_inline
int kupl_gnode_init(kupl_gnode_t& gnode)
{
    gnode.successors.head = nullptr;
    gnode.successors.tail = nullptr;
    gnode.n_successors = 0;
    gnode.n_hard_predecessors = 0;
    gnode.n_soft_predecessors = 0;
    gnode.n_predecessors = 0;
    gnode.lock = kupl_lock_create(PTHREAD_SPINLOCK);
    if (gnode.lock == nullptr) {
        return kupl_log_error_return(WARN, "task gnode's lock create failed");
    }
    return KUPL_OK;
}

static kupl_always_inline
void kupl_gnode_cleanup(kupl_gnode_t& gnode, int geid)
{
    kupl_lock_cleanup(gnode.lock);
    gnode.lock = nullptr;
    kupl_slist_destroy_all(&gnode.successors.head, geid);
    gnode.successors.head = nullptr;
}

uint32_t kupl_gnode_precede(kupl_gnode_t *precede, kupl_gnode_t *succeed, int geid);

uint32_t kupl_gnode_release_ready(kupl_gnode_t *gnode, kupl_task_t **ready_tasks);

uint32_t kupl_gnode_release_ready_safe(kupl_gnode_t *gnode, kupl_task_t **ready_tasks);

#define kupl_gnode_reset(_gnode) (KUPL_ATOMIC_ST_RLX(&(_gnode)->n_predecessors, (_gnode)->n_hard_predecessors))

#define kupl_gnode_has_deps(_gnode) ((_gnode)->successors.head != nullptr)

#define kupl_gnode_get_task(_gnode) kupl_container_of((_gnode), kupl_task_t, gnode)

#define KUPL_MAX_LOCAL_DEP_COUNT 128
#define KUPL_DAG_TASK_READY 1
#define KUPL_DAG_TASK_NOT_READY 0
#define KUPL_TASK_DEP_ALL_ADDR (void*)ULONG_MAX

typedef struct kupl_addr_entry {
    kupl_gnode_t        *out;       // data write
    kupl_slist_t        *in;        // data read
} kupl_addr_entry_t;

kupl_addr_entry_t* kupl_addr_entry_create(int geid);

void kupl_addr_entry_destroy(kupl_addr_entry_t *entry, int geid);

typedef std::unordered_map<const void*, kupl_addr_entry_t*> kupl_dag_hash_t;

typedef struct kupl_dag {
    kupl_dag_hash_t     *hash; // store all addr, for trans in/out to node/dep
    pthread_spinlock_t  lock;
} kupl_dag_t;

kupl_dag_t* kupl_dag_create(int geid);

void kupl_dag_destroy(kupl_dag_t* dag, int geid);

int kupl_dag_add_task(kupl_dag_t *dag, kupl_task_t *task, kupl_task_dep_t *task_dep_list, uint32_t task_dep_cnt, int geid);

#ifdef __cplusplus
}
#endif

#endif
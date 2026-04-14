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
#ifndef KUPL_TASK_IMPL_H
#define KUPL_TASK_IMPL_H

#include "kupl.h"
#include "mt/kupl_taskbase.h"
#include "mt/kupl_dag.h"

#ifdef __cplusplus
extern "C" {
#endif

enum kupl_task_kind_t {
    KUPL_TASK_KIND_UNKNOW          = 0,

    KUPL_TASK_KIND_COMM_DYNAMIC    = kupl_bit(0),
    KUPL_TASK_KIND_DYNAMIC_MASK    = kupl_bit(10) - 1,

    KUPL_TASK_KIND_COMM_STATIC     = kupl_bit(10),
    KUPL_TASK_KIND_STATIC_MASK     = kupl_bit(20) - kupl_bit(10)
};

typedef struct kupl_task {
    kupl_taskbase_t    tb;
    kupl_task_kind_t   kind;
    kupl_gnode_t       gnode;
    char               udata[];
} kupl_task_t;

typedef struct kupl_task_param {
    kupl_tb_param_t     super;
    kupl_task_kind_t    kind;
    kupl_task_t         *inplace;
    size_t              udata_size;
} kupl_task_param_t;

kupl_task_h kupl_task_init(kupl_task_param_t *param, int geid);

static kupl_always_inline
void kupl_task_ref(kupl_taskbase_t *tb)
{
    if (kupl_unlikely(tb == nullptr)) {
        return;
    }
    KUPL_ATOMIC_ADD_RLX(&tb->ref, 1);
}

static kupl_always_inline
void kupl_task_deref(kupl_taskbase_t *tb, int geid)
{
    if (kupl_unlikely(tb == nullptr)) {
        return;
    }
    kupl_task_h task = (kupl_task_h)tb;
    if (KUPL_ATOMIC_SUB_RLX(&tb->ref, 1) == 1) {
        if (task->tb.name != nullptr) {
            kupl_free_inner(task->tb.name);
            task->tb.name = nullptr;
        }
        kupl_gnode_cleanup(task->gnode, geid);
        kupl_memory_free(task, geid);
        kupl_debug("task_cleanup");
    }
}

int kupl_task_invoke(kupl_taskbase_t *tb);

static const kupl_tb_ops_t task_ops = {
    .ref    = kupl_task_ref,
    .deref  = kupl_task_deref,
    .invoke = kupl_task_invoke
};

static kupl_always_inline
bool kupl_static_task_check(kupl_taskbase_t *tb)
{
    return ((tb->type & KUPL_TB_TYPE_TASK) && (((kupl_task_h)tb)->kind & KUPL_TASK_KIND_STATIC_MASK));
}

kupl_task_h kupl_task_create();

kupl_task_h kupl_task_create_with_udata(size_t udata_size);

void kupl_task_cleanup(kupl_task_h task);

int kupl_task_wait(kupl_task_h task);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace kupl {

    using lambda_func_type = std::function<void(void)>;
    struct lambda_func_data {
        lambda_func_type       func;
    };

    void lambda_func(void *args);

}

#endif

#endif
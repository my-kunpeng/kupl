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
#ifndef KUPL_ULT_IMPL_H
#define KUPL_ULT_IMPL_H

#include "mt/kupl_taskbase.h"
#include "memory/mpool/kupl_mpool.h"
#include "utils/debug/kupl_log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum kupl_ult_kind {
    KUPL_ULT_KIND_UNKNOW         = 0,

    KUPL_ULT_KIND_COMM_DYNAMIC   = kupl_bit(0),
    KUPL_ULT_KIND_DYNAMIC_MASK   = kupl_bit(10) - 1
} kupl_ult_kind_t;

typedef struct kupl_ult {
    kupl_taskbase_t tb;
    kupl_ult_kind_t kind;
    char            udata[];
} kupl_ult_t;

typedef struct kupl_ult* kupl_ult_h;

typedef kupl_tb_desc_t kupl_ult_desc_t;

typedef struct kupl_ult_param {
    kupl_tb_param_t      super;
    kupl_ult_kind_t      kind;
    kupl_ult_t           *inplace;
} kupl_ult_param_t;

kupl_ult_h kupl_ult_init(kupl_ult_param_t *param, int geid);

static void kupl_ult_ref(kupl_taskbase_t *tb)
{
    if (kupl_unlikely(tb == nullptr)) {
        return;
    }
    KUPL_ATOMIC_ADD_RLX(&tb->ref, 1);
}

static void kupl_ult_deref(kupl_taskbase_t *tb, int geid)
{
    if (kupl_unlikely(tb == nullptr)) {
        return;
    }
    kupl_ult_h ult = (kupl_ult_h)tb;
    if (KUPL_ATOMIC_SUB_RLX(&ult->tb.ref, 1) == 1) {
        if (ult->tb.name != nullptr) {
            kupl_free_inner(ult->tb.name);
            ult->tb.name = nullptr;
        }
        kupl_debug("ult_cleanup");
        kupl_memory_free(ult, geid);
    }
}

kupl_ult_h kupl_ult_create(void);

void kupl_ult_cleanup(kupl_ult_h ult);

int kupl_ult_invoke(kupl_taskbase_t *tb);

static const kupl_tb_ops_t ult_ops = {
    .ref    = kupl_ult_ref,
    .deref  = kupl_ult_deref,
    .invoke = kupl_ult_invoke
};

#ifdef __cplusplus
}
#endif

#endif
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
#ifndef KUPL_ATOMIC_H
#define KUPL_ATOMIC_H

#include "generic/atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KUPL_MB()                              KUPL_MB_BASE(seq_cst)
#define KUPL_MB_ACQ()                          KUPL_MB_BASE(acquire)

#define KUPL_ATOMIC_LD(p)                      KUPL_ATOMIC_LD_BASE(p, seq_cst)
#define KUPL_ATOMIC_LD_ACQ(p)                  KUPL_ATOMIC_LD_BASE(p, acquire)
#define KUPL_ATOMIC_LD_RLX(p)                  KUPL_ATOMIC_LD_BASE(p, relaxed)
#define KUPL_ATOMIC_ST(p, v)                   KUPL_ATOMIC_OP_BASE(store, p, v, seq_cst)
#define KUPL_ATOMIC_ST_RLS(p, v)               KUPL_ATOMIC_OP_BASE(store, p, v, release)
#define KUPL_ATOMIC_ST_RLX(p, v)               KUPL_ATOMIC_OP_BASE(store, p, v, relaxed)

#define KUPL_ATOMIC_AND(p, v)                  KUPL_ATOMIC_OP_BASE(fetch_and, p, v, seq_cst)
#define KUPL_ATOMIC_OR(p, v)                   KUPL_ATOMIC_OP_BASE(fetch_or, p, v, seq_cst)
#define KUPL_ATOMIC_OR_RLS(p, v)               KUPL_ATOMIC_OP_BASE(fetch_or, p, v, release)

#define KUPL_ATOMIC_EXG(p, v)                  KUPL_ARCH_ATOMIC_EXG(p, v)

#define KUPL_ATOMIC_ADD(p, v)                  KUPL_ARCH_ATOMIC_ADD(p, v)
#define KUPL_ATOMIC_ADD_RLX(p, v)              KUPL_ARCH_ATOMIC_ADD_RLX(p, v)
#define KUPL_ATOMIC_ADD_RLS(p, v)              KUPL_ARCH_ATOMIC_ADD_RLS(p, v)
#define KUPL_ATOMIC_SUB(p, v)                  KUPL_ARCH_ATOMIC_ADD(p, -(v))
#define KUPL_ATOMIC_SUB_RLX(p, v)              KUPL_ARCH_ATOMIC_ADD_RLX(p, -(v))
#define KUPL_ATOMIC_SUB_RLS(p, v)              KUPL_ARCH_ATOMIC_ADD_RLS(p, -(v))

#define KUPL_ATOMIC_CAS_STR(p, ex, v)          KUPL_ARCH_ATOMIC_CAS_STR(p, ex, v)
#define KUPL_ATOMIC_CAS_STR_RLS2RLX(p, ex, v)  KUPL_ARCH_ATOMIC_CAS_STR_RLS2RLX(p, ex, v)
#define KUPL_ATOMIC_CAS_STR_ACQ2RLX(p, ex, v)  KUPL_ARCH_ATOMIC_CAS_STR_ACQ2RLX(p, ex, v)
#define KUPL_ATOMIC_CAS_WEA(p, ex, v)          KUPL_ARCH_ATOMIC_CAS_WEA(p, ex, v)

#ifdef __cplusplus
}
#endif

#endif
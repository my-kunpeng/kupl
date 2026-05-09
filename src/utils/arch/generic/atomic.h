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
#ifndef KUPL_GENERIC_ATOMIC_H
#define KUPL_GENERIC_ATOMIC_H

#include <atomic>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#define KUPL_ATOMIC_FLAG std::atomic_flag
#define KUPL_ATOMIC_INT std::atomic<int>
#define KUPL_ATOMIC_UINT32 std::atomic<uint32_t>
#define KUPL_ATOMIC_INT64 std::atomic<int64_t>
#define KUPL_ATOMIC_UINT64 std::atomic<uint64_t>
#define KUPL_ATOMIC_BOOL std::atomic<bool>
#define KUPL_ATOMIC_SIZE_T std::atomic<size_t>
#define KUPL_ATOMIC_VOIDP std::atomic<void *>

#define KUPL_MB_BASE(order) std::atomic_thread_fence(std::memory_order_##order)
#define KUPL_ATOMIC_LD_BASE(p, order) (p)->load(std::memory_order_##order)
#define KUPL_ATOMIC_OP_BASE(op, p, v, order) (p)->op(v, std::memory_order_##order)
#define KUPL_ATOMIC_CAS_BASE(op, p, ex, v, order1, order2) \
    (p)->op(ex, v, std::memory_order_##order1, std::memory_order_##order2)

#define KUPL_ARCH_ATOMIC_EXG(p, v) KUPL_ATOMIC_OP_BASE(exchange, p, v, seq_cst)

#define KUPL_ARCH_ATOMIC_ADD(p, v) KUPL_ATOMIC_OP_BASE(fetch_add, p, v, seq_cst)
#define KUPL_ARCH_ATOMIC_ADD_RLX(p, v) KUPL_ATOMIC_OP_BASE(fetch_add, p, v, relaxed)
#define KUPL_ARCH_ATOMIC_ADD_RLS(p, v) KUPL_ATOMIC_OP_BASE(fetch_add, p, v, release)

#define KUPL_ARCH_ATOMIC_CAS_STR(p, ex, v) KUPL_ATOMIC_CAS_BASE(compare_exchange_strong, p, ex, v, seq_cst, seq_cst)
#define KUPL_ARCH_ATOMIC_CAS_STR_RLS2RLX(p, ex, v) \
    KUPL_ATOMIC_CAS_BASE(compare_exchange_strong, p, ex, v, release, relaxed)
#define KUPL_ARCH_ATOMIC_CAS_STR_ACQ2RLX(p, ex, v) \
    KUPL_ATOMIC_CAS_BASE(compare_exchange_strong, p, ex, v, acquire, relaxed)
#define KUPL_ARCH_ATOMIC_CAS_WEA(p, ex, v) KUPL_ATOMIC_CAS_BASE(compare_exchange_weak, p, ex, v, seq_cst, seq_cst)

#ifdef __cplusplus
}
#endif

#endif
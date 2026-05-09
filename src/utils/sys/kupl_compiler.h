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
#ifndef KUPL_COMPILER_H
#define KUPL_COMPILER_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief base system */
#define KUPL_BASE_DEC 10
#define KUPL_BASE_HEX 16
#define KUPL_PATH_MAX 1024
#define KUPL_EXECUTOR_ID_MAX 1024
#define KUPL_NUMA_MAX 64

/** @brief bit operation */
#define kupl_bit(i) (1ul << (i))

#define kupl_likely(_x) __builtin_expect(!!(_x), 1)

#define kupl_unlikely(_x) __builtin_expect(!!(_x), 0)

#define kupl_always_inline inline __attribute__((always_inline))

#define kupl_offsetof(_type, _member) ((uint64_t) & (((_type *)0)->_member))

#define kupl_container_of(_ptr, _type, _member) \
    (reinterpret_cast<_type *>((char *)(_ptr) - kupl_offsetof(_type, _member)))

#ifdef __cplusplus
}
#endif

#endif
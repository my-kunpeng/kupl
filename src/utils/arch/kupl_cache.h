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
#ifndef KUPL_CACHE_H
#define KUPL_CACHE_H

#if defined(__aarch64__)
#include "aarch64/cache.h"
#else
#include "generic/cache.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define KUPL_CACHE_LINE        KUPL_ARCH_CACHE_LINE

#define KUPL_ALIGN(bytes)      __attribute__((aligned(bytes)))
#define KUPL_ALIGN_CACHE       KUPL_ALIGN(KUPL_CACHE_LINE)

#define KUPL_PAD(type, sz)     (sizeof(type) + ((sz) - ((sizeof(type) - 1) % (sz)) - 1))
#define KUPL_PAD_CACHE(type)   KUPL_PAD(type, KUPL_CACHE_LINE)

#ifdef __cplusplus
}
#endif

#endif
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
#ifndef KUPL_ASSERT_H
#define KUPL_ASSERT_H

#include "kupl_log.h"
#include "utils/sys/kupl_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_KUPL_ASSERT

#define kupl_assert(_cond)                        \
    do {                                          \
        if (kupl_unlikely(!(_cond))) {            \
            kupl_fatal("assert failed! " #_cond); \
            exit(0);                              \
        }                                         \
    } while (0)

#define kupl_assertv(_cond, args...) \
    do {                             \
        if (kupl_unlikely(_cond)) {  \
            kupl_fatal(args);        \
        }                            \
    } while (0)

#else

#define kupl_assert(_cond)
#define kupl_assertv(_cond, args...)

#endif

#ifdef __cplusplus
}
#endif
#endif
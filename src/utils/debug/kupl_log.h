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
#ifndef KUPL_LOG_H
#define KUPL_LOG_H

#include <cstdio>
#include <cstdlib>
#include "utils/config/kupl_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* log function */
#define KUPL_LOG_DEBUG 0
#define KUPL_LOG_INFO 1
#define KUPL_LOG_WARN 2
#define KUPL_LOG_ERROR 3
#define KUPL_LOG_FATAL 4

#ifndef KUPL_MIN_LOG_LEVEL
#ifdef ENABLE_KUPL_DEBUG
#define KUPL_MIN_LOG_LEVEL KUPL_LOG_DEBUG
#else
#define KUPL_MIN_LOG_LEVEL KUPL_LOG_INFO
#endif
#endif

#define kupl_trace(_msg, args...)
#define kupl_log_level(_level, _msg, args...)                                                                    \
    if (KUPL_LOG_##_level >= KUPL_MIN_LOG_LEVEL && KUPL_LOG_##_level >= kupl_config_get_value(KUPL_LOG_LEVEL)) { \
        printf("[%s %s():%d] " _msg "\n", #_level, __FUNCTION__, __LINE__, ##args);                              \
    }
#define kupl_debug(args...) kupl_log_level(DEBUG, args)
#define kupl_info(args...) kupl_log_level(INFO, args)
#define kupl_warn(args...) kupl_log_level(WARN, args)
#define kupl_error(args...) kupl_log_level(ERROR, args)
#define kupl_fatal(args...)          \
    do {                             \
        kupl_log_level(FATAL, args); \
    } while (0)

#define kupl_log_error_return(level, args...) ({ kupl_log_level(level, args) KUPL_ERROR; })

#ifdef __cplusplus
}
#endif

#endif
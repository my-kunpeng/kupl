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
#ifndef KUPL_UTILS_H
#define KUPL_UTILS_H

#include "utils/config/kupl_config.h"
#include "utils/time/kupl_time.h"
#include "utils/debug/kupl_log.h"
#include "utils/lock/kupl_lock.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/sys/kupl_hardware.h"

#ifdef __cplusplus
extern "C" {
#endif

extern bool g_utils_inited;

int kupl_utils_init(void);

void kupl_utils_fini(void);

#ifdef __cplusplus
}
#endif

#endif
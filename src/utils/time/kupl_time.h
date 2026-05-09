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
#ifndef KUPL_TIME_H
#define KUPL_TIME_H

#include <cstdint>
#include <cstdlib>

#ifdef __cplusplus
extern "C" {
#endif

#define KUPL_TIME_BUF_SIZE 30

/* timer function initialize and finalize */
void kupl_time_init(void);

void kupl_time_fini(void);

uint64_t kupl_now_ns();

void kupl_timestamp(char *buffer, size_t buffersize);

#ifdef __cplusplus
}
#endif

#endif
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
#ifndef KUPL_HBW_H
#define KUPL_HBW_H

#include <numa.h>
#include <numaif.h>

#ifdef __cplusplus
extern "C" {
#endif

int kupl_hbw_get_closest_numa_id(int numa_id);

int kupl_hbw_verify_inner(void *addr, size_t size, int flags);

int kupl_hbw_init(void);

void kupl_hbw_fini(void);

#ifdef __cplusplus
}
#endif

#endif
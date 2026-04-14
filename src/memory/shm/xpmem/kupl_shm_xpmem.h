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
#ifndef KUPL_SHM_XPMEM_H
#define KUPL_SHM_XPMEM_H

#include "memory/shm/kupl_shm.h"
#include "kupl_shm_xpmem_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kupl_shm_xpmem_packed_params {
    kupl_shm_xpmem_segid_t xsegid;
    uintptr_t remote_addr;
    size_t length;
} kupl_shm_xpmem_packed_params_t;

typedef struct kupl_shm_xpmem_remote_region {
    uintptr_t start;
    uintptr_t end;
    void *attach_pg_aligned_addr;
} kupl_shm_xpmem_remote_region_t;

typedef struct kupl_shm_xpmem_remote_trans_mem {
    void *attach_addr;
    size_t length;
} kupl_shm_xpmem_remote_trans_mem_t;

void kupl_shm_xpmem_reg_ops(void);
void kupl_shm_xpmem_dereg_ops(void);

#ifdef __cplusplus
}
#endif

#endif
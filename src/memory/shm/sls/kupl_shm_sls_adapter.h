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
#ifndef KUPL_SHM_SLS_ADAPTER_H
#define KUPL_SHM_SLS_ADAPTER_H

#include <stdint.h>
#include "sls_kernel/sls_kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int fd;
} kupl_shm_sls_init_data_t;

typedef struct {
    void *addr;
} kupl_shm_sls_mmap_data_t;

typedef struct {
    uint16_t pgoff;
    uint64_t rsize;
} kupl_shm_sls_write_data_t;

typedef struct {
    int ret;
    union {
        kupl_shm_sls_init_data_t init;
        kupl_shm_sls_mmap_data_t mmap;
        kupl_shm_sls_write_data_t write;
    };
} kupl_shm_sls_slfs_t;

bool kupl_shm_sls_init_module();

int kupl_shm_sls_init_fs(kupl_shm_sls_slfs_t &re);

int kupl_shm_sls_zcopy(int fd, void *src_addr, void *dst_addr, int src_pid, int dst_pid, unsigned long size,
                       kupl_shm_sls_slfs_t &res);

int kupl_shm_sls_zcopy_all(int fd, void *src_addr, void **dst_addr, int src_pid, int dst_pid, unsigned long size,
                           void **base_addr, kupl_shm_sls_slfs_t &res);

kupl_shm_sls_slfs_t kupl_shm_sls_slfs_dump(int fd, void *addrs);

#ifdef __cplusplus
}
#endif

#endif
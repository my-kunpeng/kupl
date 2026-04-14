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
#include "sls_kernel.h"

#include <sys/ioctl.h>
#include <stdio.h>

typedef enum {
    IO_NONE       = 0,
    IO_ATTACH     = 1,
    IO_DUMP       = 3,
    IO_MAX
} IOCTL_NUMBER;

struct dax_ioctl_pswap {
    unsigned long src_addr;
    unsigned long dst_addr;
    int src_pid;
    int dst_pid;
    unsigned long size;
};
typedef struct dax_ioctl_pswap dax_ioctl_pswap_t;

struct dax_ioctl_mmap {
    unsigned long size;
    unsigned long addr;
};
typedef struct dax_ioctl_mmap dax_ioctl_mmap_t;

long attach(int fd, unsigned long src_addr, unsigned long dst_addr,
            int src_pid, int dst_pid, unsigned long size)
{
    dax_ioctl_pswap_t frame2 = {
        .src_addr = src_addr,
        .dst_addr = dst_addr,
        .src_pid = src_pid,
        .dst_pid = dst_pid,
        .size = size,
    };
    dax_ioctl_pswap_t *ptr = &frame2;
    long ret = ioctl(fd, IO_ATTACH, (uint64_t)ptr);
    return ret;
}

long dump(int fd, unsigned long src_addr)
{
    dax_ioctl_mmap_t frame1 = {
        .size = 0,
        .addr = src_addr,
    };
    dax_ioctl_mmap_t *ptr = &frame1;
    printf("trigger dump addr=%lx, action=%d\n", src_addr, IO_DUMP);
    long ret = ioctl(fd, IO_DUMP, (uint64_t)ptr);
    return ret;
}

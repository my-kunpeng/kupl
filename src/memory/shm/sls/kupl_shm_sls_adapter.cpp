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
#include "kupl_shm_sls_adapter.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "kupl.h"
#include "utils/debug/kupl_log.h"
#include "utils/sys/kupl_compiler.h"

#define SLS_ALIGN_SIZE 2097152
#define PAGE_SIZE 2097152
#define SLS_DEVICE  "/dev/zdax"
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PAGE_ALIGN(addr) (((addr) + (size_t)PAGE_SIZE - 1) & (size_t)PAGE_MASK)

int align_and_round_page(unsigned long addr, size_t size, unsigned long *aligned_addr,
                         size_t *aligned_size, unsigned long *page_offset)
{
    if (!aligned_addr || !aligned_size) {
        return -1;
    }

    *page_offset = addr & ~PAGE_MASK;

    *aligned_addr = addr - *page_offset;

    size += *page_offset;

    *aligned_size = PAGE_ALIGN(size);

    return 0;
}

bool kupl_shm_sls_init_module()
{
    int fd = open(SLS_DEVICE, O_RDWR);
    if (fd < 0) {
        printf("sls init failed\n");
        return false;
    }
    return true;
}

int kupl_shm_sls_init_fs(kupl_shm_sls_slfs_t& res)
{
    int fd = open(SLS_DEVICE, O_RDWR);
    res.ret = -1;
    if (fd < 0) {
        return KUPL_ERROR;
    }

    res.ret = 0;
    res.init.fd = fd;

    return KUPL_OK;
}

int kupl_shm_sls_zcopy(int fd, void* src_addr, void* dst_addr,
                       int src_pid, int dst_pid, unsigned long size,
                       kupl_shm_sls_slfs_t& res)
{
    res.ret = -1;
    if ((uint64_t)src_addr % SLS_ALIGN_SIZE != 0 || (uint64_t)dst_addr % SLS_ALIGN_SIZE != 0
            || size % SLS_ALIGN_SIZE != 0) {
        return KUPL_ERROR;
    }

    int ret = (int)attach(fd, (uint64_t)src_addr, (uint64_t)dst_addr, src_pid, dst_pid, size);
    if (ret != 0) {
        return KUPL_ERROR;
    }

    res.ret = 0;
    return KUPL_OK;
}

int kupl_shm_sls_zcopy_all(int fd, void* src_addr, void** dst_addr,
                           int src_pid, int dst_pid,
                           unsigned long size, void** base_addr,
                           kupl_shm_sls_slfs_t &res)
{
    unsigned long aligned_addr = 0;
    size_t aligned_size = 0;
    unsigned long page_offset = 0;
    res.ret = -1;
    int ret = KUPL_ERROR;

    /* 执行地址对齐和大小调整 */
    if (align_and_round_page((unsigned long)src_addr, size,
                             (unsigned long *)&aligned_addr, &aligned_size, &page_offset) != 0) {
        return ret;
    }

    *dst_addr = aligned_alloc(SLS_ALIGN_SIZE, aligned_size);
    if (*dst_addr == nullptr) {
        return ret;
    }

    ret = kupl_shm_sls_zcopy(fd, (void*)aligned_addr, *dst_addr, src_pid, dst_pid, aligned_size, res);
    if (ret == KUPL_ERROR) {
        free(*dst_addr);
        *dst_addr = nullptr;
        return ret;
    }

    *base_addr = *dst_addr;
    *dst_addr = (void*)((char*)*dst_addr + page_offset);

    return ret;
}

void* kupl_shm_attach(kupl_shm_addr_t addr, size_t size)
{
    void *src_addr = addr.src_addr;
    int src_pid = addr.src_pid;
    int dst_pid = addr.dst_pid;
    unsigned long aligned_addr = 0;
    size_t aligned_size = 0;
    unsigned long page_offset = 0;

    if (kupl_unlikely(src_pid <= 0 || dst_pid <= 0)) {
        kupl_error("kupl_attach failed: Invalid pid");
        return nullptr;
    }

    if (kupl_unlikely(src_addr == nullptr)) {
        kupl_error("kupl_attach failed: Invalid src ptr");
        return nullptr;
    }

    if (kupl_unlikely(size == 0)) {
        kupl_error("kupl_attach failed: Invalid size");
        return nullptr;
    }

    /* 执行地址对齐和大小调整 */
    if (align_and_round_page((unsigned long)src_addr, size,
                             (unsigned long *)&aligned_addr, &aligned_size, &page_offset) != 0) {
        return nullptr;
    }

    kupl_shm_sls_slfs_t res;
    int ret = kupl_shm_sls_init_fs(res);
    if (ret == KUPL_ERROR) {
        return nullptr;
    }

    void *dst_addr = aligned_alloc(SLS_ALIGN_SIZE, aligned_size); // aligned_size = 2097152
    if (dst_addr == nullptr) {
        return nullptr;
    }

    ret = kupl_shm_sls_zcopy(res.init.fd, (void*)aligned_addr, dst_addr, src_pid, dst_pid, aligned_size, res);
    if (ret == KUPL_ERROR) {
        free(dst_addr);
        return nullptr;
    }
    dst_addr = (void*)((char*)dst_addr + page_offset); // add page_offset

    return dst_addr;
}

void kupl_shm_detach(void* ptr)
{
    if (kupl_unlikely(ptr == nullptr)) {
        kupl_error("kupl_detach failed: Invalid ptr");
        return;
    }
    unsigned long aligned_offset = (unsigned long)ptr & ~PAGE_MASK;
    void *aligned_addr = (void*)((char*)ptr - aligned_offset);
    free(aligned_addr);
    ptr = nullptr;
}

kupl_shm_sls_slfs_t kupl_shm_sls_slfs_dump(int fd, void *addrs)
{
    kupl_shm_sls_slfs_t res;
    (void)dump(fd, (uint64_t)addrs);
    return res;
}
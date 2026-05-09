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
#include "kupl_hbw.h"
#include <unistd.h>
#include <numa.h>
#include <numaif.h>
#include "kupl.h"
#include "core/kupl_core.h"
#include "memory/mpool/kupl_mpool.h"
#include "utils/sys/kupl_hardware.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/debug/kupl_log.h"

#define DISTANCE_MAX 255
#define PAGE_MAX (1uL << 20)
#define MAX_ALLOC_SIZE (1uLL << 32)
thread_local int current_cpu = -1;

typedef struct kupl_hbw_info {
    size_t size;
} kupl_hbw_info_t;

int g_min_dist_nodes[KUPL_NUMA_MAX];
static struct bitmask *hbw_node_mask = nullptr;
static kupl_hbw_policy_t global_hbw_policy;
static int hbw_available = 0;

static inline void kupl_hbw_touch_page(void *addr)
{
    *((volatile char *)addr) = *((volatile char *)addr);
}

int kupl_hbw_init()
{
    int ret = KUPL_ERROR;
    if (kupl_arch_detect() != KUPL_CPU_HISILICOM_920F) {
        return KUPL_OK;
    }

    const kupl_host_info_t *info = kupl_get_host_info();

    if (info->numa_cnt > KUPL_NUMA_MAX) {
        return kupl_log_error_return(ERROR, "Too many NUMA Nodes in the system, error in hbw init.");
    }

    struct bitmask *numa_available_node = nullptr;
    struct bitmask *node_cpumask = numa_allocate_cpumask();
    hbw_node_mask = numa_bitmask_alloc(static_cast<unsigned int>(info->numa_cnt) + 1);
    if (node_cpumask == nullptr || hbw_node_mask == nullptr) {
        kupl_error("inner memory alloc error during hbw init.");
        goto hbw_init_err;
    }

    numa_available_node = numa_get_mems_allowed();
    if (numa_available_node == nullptr) {
        kupl_error("can't get numa node info during hbw init.");
        goto hbw_init_err;
    }

    for (int i = 0; i < info->numa_cnt; ++i) {
        numa_node_to_cpus(i, node_cpumask);
        if ((numa_bitmask_weight(node_cpumask) == 0) &&
            (numa_bitmask_isbitset(numa_available_node, static_cast<unsigned int>(i)))) {
            numa_bitmask_setbit(hbw_node_mask, static_cast<unsigned int>(i));
        }
    }

    ret = KUPL_OK;
    if (numa_bitmask_weight(hbw_node_mask) <= 0) {
        goto hbw_init_err;
    }
    hbw_available = 1;

    for (int init_node = 0; init_node < info->numa_cnt; init_node++) {
        int min_distance = DISTANCE_MAX;
        for (int dest_node = 0; dest_node < info->numa_cnt; dest_node++) {
            int dist = numa_distance(init_node, dest_node);
            if (numa_bitmask_isbitset(hbw_node_mask, static_cast<unsigned int>(dest_node)) && dist < min_distance) {
                min_distance = dist;
                g_min_dist_nodes[init_node] = dest_node;
            }
        }
    }

    global_hbw_policy = KUPL_HBW_POLICY_BIND;
    numa_bitmask_free(node_cpumask);
    return ret;

hbw_init_err:
    numa_bitmask_free(node_cpumask);
    kupl_hbw_fini();
    return ret;
}

int kupl_hbw_get_closest_numa_id(int numa_id)
{
    return g_min_dist_nodes[numa_id];
}

int kupl_hbw_check_available(void)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    return hbw_available;
}

int kupl_hbw_verify(void *addr, size_t size, int flags)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    return kupl_hbw_verify_inner(addr, size, flags);
}

int kupl_hbw_verify_inner(void *addr, size_t size, int flags)
{
    int ret = KUPL_HBW_VERIFY_ERROR;
    if ((addr == NULL) || (size == 0) || ((unsigned int)flags & ~(unsigned int)KUPL_HBW_TOUCH_PAGES)) {
        kupl_error("kupl_hbw_verify gets invalid argument.");
        return KUPL_HBW_VERIFY_ERROR;
    }

    if (size > MAX_ALLOC_SIZE) {
        kupl_error("Unable to verify memory larger than 4GB.") return KUPL_HBW_VERIFY_ERROR;
    }

    long page_size_tmp = sysconf(_SC_PAGESIZE);
    if (page_size_tmp <= 0) {
        kupl_error("kupl_hbw_verify gets invalid page_size.");
        return KUPL_HBW_VERIFY_ERROR;
    }
    const size_t page_size = (size_t)page_size_tmp;
    const size_t page_mask = ~(page_size - 1);

    uintptr_t aligned_begin = (uintptr_t)addr & page_mask;
    uintptr_t end = (uintptr_t)addr + size;

    size_t page_count = (size_t)(end - aligned_begin) / page_size;
    page_count += (size_t)((end - aligned_begin) % page_size != 0);

    if (page_count > PAGE_MAX) {
        kupl_error("Unable to verify a region contains more than 1M pages.");
        return KUPL_HBW_VERIFY_ERROR;
    }

    int *nodes = (int *)malloc(sizeof(int) * page_count);
    void **pages = (void **)malloc(sizeof(void *) * page_count);
    if ((nodes == nullptr) || (pages == nullptr)) {
        kupl_error("kupl_hbw_verify inner malloc error.");
        goto kupl_hbw_verify_ret;
    }

    for (unsigned int i = 0; i < page_count; i++) {
        char *current_begin = reinterpret_cast<char *>(aligned_begin + i * page_size);
        if ((unsigned int)flags & KUPL_HBW_TOUCH_PAGES) {
            kupl_hbw_touch_page(current_begin);
        }
        pages[i] = current_begin;
    }

    if (move_pages(0, page_count, pages, nullptr, nodes, MPOL_MF_MOVE)) {
        kupl_error("kupl_hbw_verify failed");
        goto kupl_hbw_verify_ret;
    }

    for (unsigned int j = 0; j < page_count; j++) {
        if (nodes[j] < 0) {
            kupl_error("kupl_hbw_verify addr not pointing to valid virtual mapping");
            goto kupl_hbw_verify_ret;
        }

        if (!numa_bitmask_isbitset(hbw_node_mask, static_cast<unsigned int>(nodes[j]))) {
            ret = KUPL_IS_NOT_HBW_MEMORY;
            goto kupl_hbw_verify_ret;
        }
    }
    ret = KUPL_IS_HBW_MEMORY;

kupl_hbw_verify_ret:
    free(nodes);
    free(pages);
    return ret;
}

kupl_hbw_policy_t kupl_hbw_get_policy(void)
{
    return global_hbw_policy;
}

int kupl_hbw_set_policy(kupl_hbw_policy_t policy)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (policy != KUPL_HBW_POLICY_BIND) {
        return KUPL_ERROR;
    }
    global_hbw_policy = policy;
    return KUPL_OK;
}

void *kupl_hbw_malloc_bind(size_t size)
{
    int geid = kupl_get_executor_num();
    if (kupl_unlikely(geid < 0 || geid >= g_kupl_memory_pool_size)) {
        kupl_error("kupl_hbw_malloc invalid geid: %d", geid);
        return nullptr;
    }
    void *ptr = kupl_memory_hbw_alloc_inner(size, geid);
    if (ptr == nullptr) {
        kupl_error("kupl_hbw_malloc bind policy: Unable to malloc memory.") return nullptr;
    }
    return ptr;
}

void *kupl_hbw_malloc(size_t size)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return nullptr;
    }
    if (size == 0) {
        kupl_error("Unable to alloc a 0 size memory in HBW");
        return nullptr;
    }

    if (size > MAX_ALLOC_SIZE) {
        kupl_error("Unable to alloc memory larger than 4GB.");
        return nullptr;
    }

    if (kupl_hbw_check_available() == 0) {
        kupl_error("High Band-width memory undetected, failed to alloc hbw memory");
        return nullptr;
    }

    void *ptr;
    switch (global_hbw_policy) {
        case KUPL_HBW_POLICY_BIND:
            ptr = kupl_hbw_malloc_bind(size);
            break;
        default:
            ptr = kupl_hbw_malloc_bind(size);
    }

    return ptr;
}

void kupl_hbw_free(void *ptr)
{
    if (ptr == nullptr) {
        return;
    }
    int geid = kupl_get_executor_num();
    kupl_memory_hbw_free_inner(ptr, geid);
    return;
}

void kupl_hbw_fini()
{
    if (hbw_node_mask) {
        numa_bitmask_free(hbw_node_mask);
        hbw_node_mask = nullptr;
    }
    return;
}
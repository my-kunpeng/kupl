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
#include "kupl_mpool.h"

#ifdef KUPL_USE_TCMALLOC
#include <gperftools/tcmalloc.h>
#elif defined KUPL_USE_JEMALLOC
#include <jemalloc/jemalloc.h>
#else
#include <cstdlib>
#endif

#include <string>
#include <cstring>
#include <fstream>
#include <dirent.h>
#include <numa.h>
#include <numaif.h>
#include <linux/mman.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "kupl.h"
#include "core/kupl_core.h"
#include "memory/hbw/kupl_hbw.h"
#include "utils/thirdpart/sdma/sdma_module.h"
#include "utils/arch/kupl_atomic.h"
#include "utils/arch/kupl_cache.h"
#include "utils/config/kupl_config.h"
#include "utils/debug/kupl_assert.h"
#include "utils/debug/kupl_log.h"
#include "utils/lock/kupl_lock.h"
#include "tools/struct/kupl_list.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/sys/kupl_math.h"
#include "utils/type/kupl_status.h"
#include "utils/sys/kupl_hardware.h"
#include "tools/profile/kupl_profile.h"

void* kupl_malloc_inner(size_t size)
{
#ifdef KUPL_USE_TCMALLOC
    return tc_malloc(size);
#elif defined KUPL_USE_JEMALLOC
    return malloc(size);
#elif defined KUPL_USE_KQMALLOC
    return malloc(size);
#else
    return malloc(size);
#endif
}

void* kupl_calloc(size_t num, size_t size)
{
#ifdef KUPL_USE_TCMALLOC
    return tc_calloc(num, size);
#elif defined KUPL_USE_JEMALLOC
    return calloc(num, size);
#elif defined KUPL_USE_KQMALLOC
    return calloc(num, size);
#else
    return calloc(num, size);
#endif
}

void* kupl_aligned_alloc(size_t alignment, size_t size)
{
#ifdef KUPL_USE_TCMALLOC
    return tc_memalign(alignment, size);
#elif defined KUPL_USE_JEMALLOC
    return aligned_alloc(alignment, size);
#elif defined KUPL_USE_KQMALLOC
    return aligned_alloc(alignment, size);
#else
    return aligned_alloc(alignment, size);
#endif
}

void kupl_free_inner(void* ptr)
{
#ifdef KUPL_USE_TCMALLOC
    tc_free(ptr);
#elif defined KUPL_USE_JEMALLOC
    free(ptr);
#elif defined KUPL_USE_KQMALLOC
    free(ptr);
#else
    free(ptr);
#endif
}

#define HUGE_PAGE_SIZE (2 * 1024 * 1024)

#define ALIGN_TO_PAGE_SIZE(x) \
    (((x) + HUGE_PAGE_SIZE - 1) / HUGE_PAGE_SIZE * HUGE_PAGE_SIZE)

#define KUPL_MEMORY_WARMUP_COUNT           32
#define KUPL_MEMORY_WARMUP_SIZE0           64
#define KUPL_MEMORY_WARMUP_SIZE1           128
#define KUPL_MEMORY_WARMUP_SIZE2           256
#define KUPL_MEMORY_WARMUP_SIZE3           512
#define KUPL_MEMORY_BIN_INDEX_INIT         (-1)
#define KUPL_FAST_MEMORY_OTHRE_QUEUE_LIMIT 16
#define KUPL_NORMAL_MEMORY_FAST_GROW_SIZE  65536
#define KUPL_MEM_BINS_THRESHOLD            4
#define KUPL_HBW_BIAS                      16

#define MEM_FAST_UNIT   KUPL_CACHE_LINE
static const size_t g_fast_bin_size[] = {
    1 * MEM_FAST_UNIT, /* when user alloc 0 size buffer, we still return some space */
    1 * MEM_FAST_UNIT, 2 * MEM_FAST_UNIT, 3 * MEM_FAST_UNIT,
    4 * MEM_FAST_UNIT, 5 * MEM_FAST_UNIT, 6 * MEM_FAST_UNIT,
    7 * MEM_FAST_UNIT, 8 * MEM_FAST_UNIT, 9 * MEM_FAST_UNIT,
    10 * MEM_FAST_UNIT, 11 * MEM_FAST_UNIT, 12 * MEM_FAST_UNIT,
    13 * MEM_FAST_UNIT, 14 * MEM_FAST_UNIT, 15 * MEM_FAST_UNIT,
    16 * MEM_FAST_UNIT, 17 * MEM_FAST_UNIT, 18 * MEM_FAST_UNIT
};

static const size_t g_memory_bin_size[] = {
    0, 512, 1024, 2 * 1024, 4 * 1024, 8 * 1024, 16 * 1024, 32 * 1024, 64 * 1024, 128 * 1024,
    256 * 1024, 512 * 1024, 1024 * 1024, 2 * 1024 * 1024, 4 * 1024 * 1024};

static const size_t g_memory_warmup_size[] = {
    KUPL_MEMORY_WARMUP_SIZE0, KUPL_MEMORY_WARMUP_SIZE1,
    KUPL_MEMORY_WARMUP_SIZE2, KUPL_MEMORY_WARMUP_SIZE3
};

#define FAST_BINS_COUNT             (sizeof(g_fast_bin_size) / sizeof(size_t))
#define MEM_BINS_COUNT              (sizeof(g_memory_bin_size) / sizeof(size_t))
#define MEM_WARMUP_SIZE_COUNT       (sizeof(g_memory_warmup_size) / sizeof(size_t))
#define MEM_CHUNK_GROW_SIZE         (g_memory_bin_size[MEM_BINS_COUNT - 1])
#define MEM_MAX_SIZE                (MEM_CHUNK_GROW_SIZE - 2 * sizeof(kupl_buffer_desc_t))
#define MEM_IS_WHOLE_CHUNK(_desc)       \
    ((_desc)->total_size == (ssize_t)(MEM_CHUNK_GROW_SIZE - sizeof(kupl_buffer_desc_t)))

#ifdef USE_MEM_STATIS
#define MEM_STATIS(_code) _code
#else
#define MEM_STATIS(_code)
#endif

PROFILE_ID_REG(fast_alloc, fast_free);
typedef struct kupl_memory_fast_cache {
    void *self;          /* only self executor thread will get/put memory from this list */
    void *sync;          /* other multi-executor_threads will put memory to this list */
    void *other;         /* only self executor thread will put other executor thread's memory in there */
    void *other_tail;    /* the tail node in other list */
    size_t other_count;
} kupl_mfc_t;

typedef struct kupl_fast_desc {
    uint16_t eid;
    int fast_bin_index;        /* >= 0 means use fast bins */
    void *alloc_ptr;           /* alloc ptr from kupl_normal_alloc() */
} kupl_fast_desc_t;

typedef struct kupl_buffer_desc {
    uint16_t eid;               /* which executor executor thread alloc this buffer */
    uint16_t ref;               /* ref count only use for fast memory */
    int mem_bin_index;          /* >=0 mean use mem bins */

    int total_size;             /* buffer total size include the buffer desc, <=0 mean is in using */
    int prev_free;              /* =0 means prev buffer is in using, >0 means prev buffer is freed */

    kupl_list_t list;

    kupl_fast_desc_t padding;    /* please don't access this member */
} kupl_buffer_desc_t;

/**
 * @brief Per-executor_thread memory meta data
 */
typedef struct kupl_memory {
    /* executor thread info */
    uint16_t eid;

    /* fast memory bins */
    kupl_mfc_t fast_bins[FAST_BINS_COUNT];
    uint64_t stats_fast_self = 0;
    uint64_t stats_fast_sync = 0;
    uint64_t stats_fast_other = 0;

    /* normal memory bins */
    uint32_t alloc_chunks;
    kupl_list_t mem_bins[MEM_BINS_COUNT];
    uint32_t mem_bins_count[MEM_BINS_COUNT];
    void *mem_sync_list;
    uint64_t stats_mem_alloc_count = 0;
    uint64_t stats_mem_free_count = 0;
    uint64_t stats_mem_link_travel = 0;
    uint64_t stats_mem_bin_travel = 0;
} kupl_memory_t;

bool g_mpool_inited = false;
bool g_mpool_default_hbw = false;
static kupl_memory_t** g_kupl_memory_pool = nullptr;
static kupl_memory_t** g_kupl_hbw_memory_pool = nullptr;
int g_kupl_memory_pool_size = 0;
int g_kupl_max_memory_pool_size = 0;
static int g_kupl_user_align_config = 8;
static bool g_is_hugepage_exist[KUPL_NUMA_MAX] = {false};
static bool g_enable_hugepages = false;
static bool sdma_mpool_func_init = false;

#define KUPL_MEM(_eid)       (g_kupl_memory_pool[_eid])
#define KUPL_HBW_MEM(_eid)   (g_kupl_hbw_memory_pool[_eid])
#define KUPL_FAST_DESC(_ptr) (reinterpret_cast<kupl_fast_desc_t *>((char *)(_ptr) - sizeof(kupl_fast_desc_t)))
#define KUPL_BUFF_DESC(_buf) (reinterpret_cast<kupl_buffer_desc_t *>((char *)(_buf) - sizeof(kupl_buffer_desc_t)))

static void kupl_normal_free(kupl_memory_t *mem, void *ptr, bool is_hbw_free);

static kupl_always_inline
int kupl_get_bin_index(size_t size, const size_t *bins, size_t bin_count)
{
    int low = 0;
    int high = (int)bin_count - 1;

    while ((high - low) > 1) {
        int mid = (high + low) >> 1;
        if (size < bins[mid]) {
            high = mid - 1;
        } else {
            low = mid;
        }
    }

    kupl_assert(low >= 0 && low < (int)bin_count);
    if (high > low && size >= bins[high]) {
        return high;
    }

    return low;
}

static kupl_always_inline
void kupl_normal_insert_into_freelist(kupl_memory_t *mem, kupl_buffer_desc_t *desc)
{
    int index = kupl_get_bin_index(static_cast<size_t>(desc->total_size), g_memory_bin_size, MEM_BINS_COUNT);
    kupl_assert((int)g_memory_bin_size[index] <= desc->total_size);
    desc->mem_bin_index = index;

    kupl_list_insert_after(&mem->mem_bins[index], &desc->list);
    mem->mem_bins_count[index] += 1;
}

static kupl_always_inline
void* kupl_normal_travel_list(kupl_memory_t *mem, kupl_list_t *head, int actual, size_t index)
{
    static const int min_buffer_size = sizeof(kupl_buffer_desc_t) + 16;
    kupl_list_t *next = head->next;
    while (next != head) {
        MEM_STATIS(mem->stats_mem_link_travel++);
        kupl_buffer_desc_t* desc = kupl_container_of(next, kupl_buffer_desc_t, list);
        if (desc->total_size >= (ssize_t)actual) {
            if (desc->total_size - actual >= min_buffer_size) {
                /* this buffer can spilt into a smaller buffer */
                auto user_desc = reinterpret_cast<kupl_buffer_desc_t *>((char *)(desc) + desc->total_size - actual);
                auto next_desc = reinterpret_cast<kupl_buffer_desc_t *>(
                    reinterpret_cast<uintptr_t>(user_desc) + static_cast<size_t>(actual)
                );

                desc->total_size -= actual;         /* decrease the remain buffer size */

                user_desc->eid = desc->eid;
                user_desc->mem_bin_index = desc->mem_bin_index;
                user_desc->total_size = -actual;    /* Set negative size to mark this buffer is in used */
                user_desc->prev_free = desc->total_size;
                kupl_list_init(&user_desc->list);

                next_desc->prev_free = 0;           /* Set zero to tell the next buffer that prev buffer is in using */

                kupl_list_del(&desc->list);
                mem->mem_bins_count[index] -= 1;
                kupl_normal_insert_into_freelist(mem, desc);

                user_desc->ref = 1;
                return user_desc + 1;
            } else {
                kupl_list_del(&desc->list);
                mem->mem_bins_count[index] -= 1;

                auto next_desc = reinterpret_cast<kupl_buffer_desc_t *>((char *)desc + desc->total_size);
                next_desc->prev_free = 0;               /* Set zero to tell next buffer that prev buffer is in using */

                desc->total_size = -desc->total_size;   /* Set negative size to mark this buffer is in used */
                desc->prev_free = 0;                    /* Because this desc buffer is the head in this chunk,
                                                         * so it doesn't have prev buffer,
                                                         * so we set zero mark that prev buffer is always in using */

                desc->ref = 1;
                return desc + 1;
            }
        }

        next = next->next;  /* to find next */
    }

    /* we don't find any buffer */
    return nullptr;
}

static kupl_always_inline
void kupl_normal_sync_list_enqueue(uint16_t eid, void *ptr, bool is_hbw_enqueue = false)
{
    std::atomic<void*> *sync = (is_hbw_enqueue) ? (std::atomic<void*> *)(&(KUPL_HBW_MEM(eid)->mem_sync_list))
                                                : (std::atomic<void*> *)(&(KUPL_MEM(eid)->mem_sync_list));
    auto old = KUPL_ATOMIC_LD_RLX(sync);
    *(void **)ptr = old;

    while (!KUPL_ATOMIC_CAS_WEA(sync, old, ptr)) {
        old = KUPL_ATOMIC_LD_RLX(sync);
        *(void **)ptr = old;
    }
}

static kupl_always_inline
void kupl_normal_sync_list_dequeue(kupl_memory_t *mem, bool is_hbw_dequeue = false)
{
    auto sync = (std::atomic<void*> *)(&mem->mem_sync_list);
    auto old = KUPL_ATOMIC_LD_RLX(sync);
    while (!KUPL_ATOMIC_CAS_WEA(sync, old, nullptr)) {
        old = KUPL_ATOMIC_LD_RLX(sync);
    }

    while (old != nullptr) {
        void *buf = old;
        old = *(void **)old;        /* move to next */

        kupl_normal_free(mem, buf, is_hbw_dequeue);  /* free this buffer */
    }
}

int kupl_bind_memory_to_numa(void *ptr, int node, size_t size)
{
    nodemask_t nodemask;
    struct bitmask bitmask = {KUPL_NUMA_MAX, nodemask.n};
    long err;
    numa_bitmask_clearall(&bitmask);
    numa_bitmask_setbit(&bitmask, (unsigned int)node);
    err = mbind(ptr, size, MPOL_BIND, nodemask.n, KUPL_NUMA_MAX, 0);
    if (err != 0) {
        return KUPL_ERROR;
    }
    return KUPL_OK;
}

static kupl_always_inline
int kupl_mpool_get_numa_id()
{
    int core_id = sched_getcpu();
    if (core_id < 0) {
        kupl_error("sched_getcpu failed.");
        return KUPL_ERROR;
    }
    return numa_node_of_cpu(core_id);
}

static kupl_always_inline
int kupl_hugepages_mmap(void **ptr, int node, size_t size)
{
    int ret = KUPL_ERROR;
    struct bitmask *old_mask = numa_get_membind();
    if (old_mask == nullptr) {
        kupl_error("Hugepage mmap couldn't get mem bind info.");
        return KUPL_ERROR;
    }

    struct bitmask *new_mask = numa_allocate_nodemask();
    if (new_mask == nullptr) {
        numa_free_nodemask(old_mask);
        kupl_error("Hugepage mmap couldn't set mem bind info.");
        return KUPL_ERROR;
    }

    numa_bitmask_clearall(new_mask);
    numa_bitmask_setbit(new_mask, (unsigned int)node);
    numa_set_membind(new_mask);

    *ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);

    if (kupl_likely(*ptr != MAP_FAILED)) {
        ret = KUPL_OK;
    }

    numa_set_membind(old_mask);
    numa_free_nodemask(new_mask);
    numa_free_nodemask(old_mask);
    return ret;
}

static kupl_always_inline
void* kupl_memory_mmap(size_t size, bool is_hbw_mmap = false, bool is_win_alloc_hugepage = false)
{
    void* ptr = nullptr;
    thread_local int current_numa_id = kupl_mpool_get_numa_id();
    int is_hugepage_success = KUPL_ERROR;
    if (kupl_unlikely(current_numa_id == KUPL_ERROR)) {
        return nullptr;
    }
    int dest_numa_id = is_hbw_mmap ? kupl_hbw_get_closest_numa_id(current_numa_id) : current_numa_id;
    if ((g_enable_hugepages || is_win_alloc_hugepage) && g_is_hugepage_exist[dest_numa_id]) {
        is_hugepage_success = kupl_hugepages_mmap(&ptr, dest_numa_id, size);
        if (is_hugepage_success == KUPL_ERROR) {
            kupl_warn("kupl_memory_mmap use hugepage failed, fallback to malloc normal page.");
            ptr = (kupl_buffer_desc_t *)mmap(nullptr, size, PROT_READ | PROT_WRITE,
                                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }
    } else {
        ptr = (kupl_buffer_desc_t *)mmap(nullptr, size, PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }

    if (kupl_unlikely(ptr == MAP_FAILED)) {
        return nullptr;
    }

    if (is_hugepage_success != KUPL_OK) {
        int ret = kupl_bind_memory_to_numa(ptr, dest_numa_id, size);
        if (ret == KUPL_ERROR) {
            kupl_warn("kupl_memory_mmap bind failed, fallback to default mmap.");
        }
    }
    return ptr;
}

void* kupl_normal_alloc(kupl_memory_t *mem, size_t size, bool is_hbw_alloc = false, bool is_win_alloc_hugepage = false)
{
    kupl_normal_sync_list_dequeue(mem, is_hbw_alloc);

    static size_t align_length = (size_t)g_kupl_user_align_config;
    if (is_win_alloc_hugepage) {
        align_length = HUGE_PAGE_SIZE;
    }
    void *ptr = nullptr;
    auto actual = size + sizeof(kupl_buffer_desc_t);
    actual = (actual + align_length - 1) & (~(align_length - 1));   /* size change to 8 bytes aligned */

    if (actual > MEM_MAX_SIZE) {
        auto desc = (kupl_buffer_desc_t *)kupl_memory_mmap(actual, is_hbw_alloc, is_win_alloc_hugepage);
        if (kupl_unlikely(desc == nullptr)) {
            return nullptr;
        }

        if (sdma_mpool_func_init) {
            kupl_mlock(desc, actual);
        }

        desc->eid = mem->eid;
        desc->mem_bin_index = KUPL_MEMORY_BIN_INDEX_INIT;  /* <0 means this buffer is direct alloc */
        desc->total_size = (int)actual;
        kupl_list_init(&desc->list);
        desc->ref = 1;
        return desc + 1;
    }

search_bins:
    MEM_STATIS(mem->stats_mem_alloc_count++);
    for (size_t index = (size_t)kupl_get_bin_index(actual, g_memory_bin_size, MEM_BINS_COUNT);
        index < MEM_BINS_COUNT; ++index) {
        auto head = &mem->mem_bins[index];
        MEM_STATIS(mem->stats_mem_bin_travel++);
        ptr = kupl_normal_travel_list(mem, head, (int)actual, index);
        if (kupl_likely(ptr != nullptr)) {
            return ptr;
        }
    }

    kupl_buffer_desc_t* desc = nullptr;
    desc = (kupl_buffer_desc_t *)kupl_memory_mmap(MEM_CHUNK_GROW_SIZE, is_hbw_alloc, is_win_alloc_hugepage);
    if (kupl_unlikely(desc == nullptr)) {
        return nullptr;
    }

    mem->alloc_chunks += 1;

    desc->eid = mem->eid;
    desc->ref = 1;
    /* remian a dummy desc in tail buffer */
    desc->total_size = (int)(MEM_CHUNK_GROW_SIZE - sizeof(kupl_buffer_desc_t));
    desc->prev_free = 0;
    kupl_list_init(&desc->list);

    /* next_desc is a dummy buffer */
    auto next_desc = reinterpret_cast<kupl_buffer_desc_t *>((char *)desc + desc->total_size);
    next_desc->prev_free = desc->total_size;
    next_desc->eid = mem->eid;
    next_desc->total_size = -1;     /* dummy buffer's size must be less then 0, means it is always in using */

    if (sdma_mpool_func_init) {
        kupl_mlock(desc, MEM_CHUNK_GROW_SIZE);
    }

    kupl_normal_insert_into_freelist(mem, desc);
    goto search_bins;
}

static kupl_always_inline
void kupl_normal_free(kupl_memory_t *mem, void *ptr, bool is_hbw_free = false)
{
    auto desc = KUPL_BUFF_DESC(ptr);
    kupl_assert(desc->ref > 0);
    if (--desc->ref > 0) { /* other still use this buffer */
        return;
    }

    /* This is direct malloc, so just free it */
    if (desc->mem_bin_index < 0) {
        if (sdma_mpool_func_init) {
            kupl_munlock(desc, (size_t)desc->total_size);
        }
        munmap(desc, (size_t)desc->total_size);
        return;
    }
    MEM_STATIS(mem->stats_mem_free_count++);

    /* This buffer is other executor thread */
    if (desc->eid != mem->eid) {
        kupl_normal_sync_list_enqueue(desc->eid, ptr, is_hbw_free);
        return;
    }

    kupl_assert(desc->total_size < 0);       /* negative size means this buffer is in using */
    desc->total_size = -desc->total_size;   /* set the real size */

    int index = desc->mem_bin_index;

    if (mem->mem_bins_count[index] < KUPL_MEM_BINS_THRESHOLD) {
        kupl_normal_insert_into_freelist(mem, desc);
        return;
    }

    if (desc->prev_free != 0) {             /* prev buffer is freed */
        auto prev_desc = reinterpret_cast<kupl_buffer_desc_t *>((char *)desc - desc->prev_free);
        prev_desc->total_size += desc->total_size;
        kupl_assert(prev_desc->total_size > desc->total_size);
        kupl_list_del(&prev_desc->list);
        mem->mem_bins_count[index] -= 1;

        desc = prev_desc;
    }

    auto next_desc = reinterpret_cast<kupl_buffer_desc_t *>((char*)desc + desc->total_size);
    if (next_desc->total_size > 0) {        /* next buffer is freed */
        desc->total_size += next_desc->total_size;
        kupl_list_del(&next_desc->list);
        mem->mem_bins_count[index] -= 1;

        /* get the new next buffer, it must in used */
        next_desc = reinterpret_cast<kupl_buffer_desc_t *>((char *)desc + desc->total_size);
    }
    next_desc->prev_free = desc->total_size;

    /* if this is a whole block, we free it */
#ifndef MEM_NEED_FREE
    if (MEM_IS_WHOLE_CHUNK(desc) && mem->alloc_chunks >= 2) {
        if (sdma_mpool_func_init) {
            kupl_munlock(desc, (size_t)desc->total_size);
        }
        munmap(desc, (size_t)desc->total_size);
        return;
    }
#endif

    kupl_normal_insert_into_freelist(mem, desc);
    return;
}

static kupl_always_inline
void kupl_normal_init(kupl_memory_t *mem)
{
    mem->alloc_chunks = 0;
    mem->mem_sync_list = nullptr;
    for (unsigned i = 0; i < MEM_BINS_COUNT; ++i) {
        kupl_list_init(&mem->mem_bins[i]);
        mem->mem_bins_count[i] = 0;
    }
}

static kupl_always_inline
void kupl_normal_cleanup(kupl_memory_t *mem, bool is_hbw_cleanup = false)
{
    kupl_normal_sync_list_dequeue(mem, is_hbw_cleanup);

    for (unsigned i = 0; i < MEM_BINS_COUNT; ++i) {
        auto head = &mem->mem_bins[i];
        auto buffer = head->next;

        while (buffer != head) {
            auto desc = kupl_container_of(buffer, kupl_buffer_desc_t, list);
            if (!MEM_IS_WHOLE_CHUNK(desc)) {
                buffer = buffer->next;  /* move to next */
            } else {
                auto old = buffer;
                buffer = old->next;   /* move to next */

                kupl_list_del(old);  /* free this buffer */
                mem->mem_bins_count[i] -= 1;
                if (sdma_mpool_func_init) {
                    kupl_munlock(desc, (size_t)desc->total_size);
                }
                munmap(desc, (size_t)desc->total_size);
            }
        }
    }
}

static kupl_always_inline
void* kupl_get_memory_from_normal_memory(kupl_memory_t *mem, size_t actual, int index,
                                         bool is_hbw_alloc = false, bool is_win_alloc_hugepage = false)
{
    void *ptr;
#ifdef KUPL_ONLY_ALLOC_ONE
    auto alloc_ptr = kupl_normal_alloc(mem, actual + KUPL_CACHE_LINE, is_hbw_alloc, is_win_alloc_hugepage);
    if (kupl_unlikely(alloc_ptr == nullptr)) {
        return nullptr;
    }

    /* align ptr to CACHELINE */
    ptr = (void *)((((uint64_t)alloc_ptr) + KUPL_CACHE_LINE - 1) & (~(KUPL_CACHE_LINE - 1)));
    auto fast_desc = KUPL_FAST_DESC(ptr);
    fast_desc->eid = mem->eid;
    fast_desc->fast_bin_index = index;
    fast_desc->alloc_ptr = alloc_ptr;
#else
    size_t fast_grow_size = KUPL_NORMAL_MEMORY_FAST_GROW_SIZE;
    auto alloc_ptr = kupl_normal_alloc(mem, fast_grow_size, is_hbw_alloc, is_win_alloc_hugepage);
    if (kupl_unlikely(alloc_ptr == nullptr)) {
        return nullptr;
    }
    /* for fast_desc */
    size_t delta = (sizeof(kupl_fast_desc_t) + (size_t)KUPL_CACHE_LINE - 1) & (~((size_t)KUPL_CACHE_LINE - 1));
    /* because in kupl_buffer_desc has kupl_fast_desc */
    uint16_t count = (uint16_t)(fast_grow_size / (actual + delta));
    auto mem_desc = KUPL_BUFF_DESC(alloc_ptr);
    mem_desc->ref = count;

    /* align ptr to CACHELINE */
    ptr = (void *)((((uint64_t)alloc_ptr) + (size_t)KUPL_CACHE_LINE - 1) & (~((size_t)KUPL_CACHE_LINE - 1)));
    for (uint16_t i = 0; i < count; ++i) {
        auto buf = (void *)(((uint64_t)ptr) + (actual + delta) * i);
        auto desc = KUPL_FAST_DESC(buf);
        desc->eid = mem->eid;
        desc->fast_bin_index = index;
        desc->alloc_ptr = alloc_ptr;
        if (kupl_likely(i > 0)) {    /* the first one we will return to user, so don't put in self list */
            kupl_sv_list_put(mem->fast_bins[index].self, buf);
        }
    }
#endif
    return ptr;
}

static kupl_always_inline
void* kupl_fast_alloc(kupl_memory_t *mem, size_t size, bool is_hbw_alloc = false, bool is_win_alloc_hugepage = false)
{
    void *ptr = nullptr;
    size_t actual = size;
    /* 0. it is too big, so direct alloc from normal memory */
    if (actual > g_fast_bin_size[FAST_BINS_COUNT - 1]) {
        ptr = kupl_normal_alloc(mem, actual, is_hbw_alloc, is_win_alloc_hugepage);
        if (kupl_unlikely(ptr == nullptr)) {
            return nullptr;
        }

        auto fast_desc = KUPL_FAST_DESC(ptr);
        fast_desc->eid = mem->eid;
        fast_desc->fast_bin_index = KUPL_MEMORY_BIN_INDEX_INIT;
        fast_desc->alloc_ptr = ptr;

        return ptr;
    }

    int index = (int)((actual + MEM_FAST_UNIT - 1) / MEM_FAST_UNIT);
    actual = g_fast_bin_size[index];

    /* 1. try get memory from self list */
    auto free_list = &mem->fast_bins[index];
    ptr = kupl_sv_list_get(free_list->self);
    if (ptr != nullptr) {
        MEM_STATIS(mem->stats_fast_self++);
        return ptr;
    }

    /* 2. try get memory from sync list */
    auto sync = (std::atomic<void *> *)(&free_list->sync);
    ptr = KUPL_ATOMIC_LD_RLX(sync);
    if (ptr != nullptr) {
        while (!KUPL_ATOMIC_CAS_WEA(sync, ptr, nullptr)) {
            ptr = KUPL_ATOMIC_LD_RLX(sync);
        }

        free_list->self = *(void **)ptr;
        MEM_STATIS(mem->stats_fast_sync++);
        return ptr;
    }

    /* 3. final get memory from normal memory */
    ptr = kupl_get_memory_from_normal_memory(mem, actual, index, is_hbw_alloc, is_win_alloc_hugepage);

    return ptr;
}

static kupl_always_inline
void kupl_fast_free(kupl_memory_t *mem, void *ptr, bool is_hbw_free = false)
{
    auto fast_desc = KUPL_FAST_DESC(ptr);
    auto eid = fast_desc->eid;
    auto index = fast_desc->fast_bin_index;
    if (index < 0) {
        kupl_normal_free(mem, fast_desc->alloc_ptr, is_hbw_free);
        return;
    }

    auto free_list = &mem->fast_bins[index];
    /* 1. free to self list */
    if (eid == mem->eid) {
        kupl_sv_list_put(free_list->self, ptr);
        return;
    }

    /* 2. free to other list */
    void *head = free_list->other;
    if (head == nullptr) {
        kupl_sv_list_put(free_list->other, ptr);
        free_list->other_tail = ptr;
        kupl_assert(*(void **)ptr == nullptr);
        return;
    }

    fast_desc = KUPL_FAST_DESC(head);
    // eid is same as the other queue's eid and the other queue's count is less than the limit
    if (eid == fast_desc->eid && free_list->other_count < KUPL_FAST_MEMORY_OTHRE_QUEUE_LIMIT) {
        kupl_sv_list_put(free_list->other, ptr);
        free_list->other_count++;
        return;
    }

    /* 3. free other to other's sync */
    eid = fast_desc->eid;
    void *tail = free_list->other_tail;

    /* 3.1 push ptr to other list */
    *(void **)ptr = nullptr;
    free_list->other = free_list->other_tail = ptr;
    free_list->other_count = 1;

    /* 3.2 push this executor thread's other list to memory owner's sync list */
    std::atomic<void *> *sync = (is_hbw_free) ? (std::atomic<void *> *)(&(KUPL_HBW_MEM(eid)->fast_bins[index].sync))
                                              : (std::atomic<void *> *)(&(KUPL_MEM(eid)->fast_bins[index].sync));
    auto old_ptr = KUPL_ATOMIC_LD_RLX(sync);
    *(void **)tail = old_ptr;
    while (!KUPL_ATOMIC_CAS_WEA(sync, old_ptr, head)) {
        old_ptr = KUPL_ATOMIC_LD_RLX(sync);
        *(void **)tail = old_ptr;
    }
    MEM_STATIS(mem->stats_fast_other++);

    return;
}

static kupl_always_inline
void kupl_fast_init(kupl_memory_t *mem)
{
    for (unsigned i = 0; i < FAST_BINS_COUNT; ++i) {
        mem->fast_bins[i].self = nullptr;
        mem->fast_bins[i].sync = nullptr;
        mem->fast_bins[i].other = nullptr;
        mem->fast_bins[i].other_tail = nullptr;
        mem->fast_bins[i].other_count = 0;
    }
}

static kupl_always_inline
void kupl_fast_cleanup(kupl_memory_t *mem, bool is_hbw_free = false)
{
    for (unsigned i = 0; i < FAST_BINS_COUNT; ++i) {
        auto bin = &mem->fast_bins[i];
        auto ptr = bin->self;
        while (ptr != nullptr) {
            auto desc = KUPL_FAST_DESC(ptr);
            ptr = *(void **)ptr;  /* move to next */
            kupl_normal_free(mem, desc->alloc_ptr, is_hbw_free);
        }

        ptr = bin->sync;
        while (ptr != nullptr) {
            auto desc = KUPL_FAST_DESC(ptr);
            ptr = *(void **)ptr;  /* move to next */
            kupl_normal_free(mem, desc->alloc_ptr, is_hbw_free);
        }

        ptr = bin->other;
        MEM_STATIS(uint64_t other_count = 0);
        while (ptr != nullptr) {
            auto desc = KUPL_FAST_DESC(ptr);
            ptr = *(void **)ptr;  /* move to next */
            auto free_mem = (is_hbw_free) ? KUPL_HBW_MEM(desc->eid) : KUPL_MEM(desc->eid);
            kupl_normal_free(free_mem, desc->alloc_ptr, is_hbw_free);
            MEM_STATIS(other_count++);
        }

        MEM_STATIS(printf("Eid :%u other:%lu\n", i, other_count));
    }
}

static kupl_always_inline
void kupl_memory_warmup()
{
#ifdef ENABLE_KUPL_GLIBC_MALLOC
    return;
#endif

    if (g_kupl_memory_pool == nullptr) {
        return;
    }

    void* default_ptr = mmap(nullptr, MEM_CHUNK_GROW_SIZE, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (default_ptr == MAP_FAILED) {
        return;
    }
    int verify_result = kupl_hbw_verify_inner(default_ptr, MEM_CHUNK_GROW_SIZE, KUPL_HBW_TOUCH_PAGES);
    if (verify_result == KUPL_IS_HBW_MEMORY) {
        g_mpool_default_hbw = true;
    }
    munmap(default_ptr, MEM_CHUNK_GROW_SIZE);

    const int max_warmup_count = kupl_min(KUPL_MEMORY_WARMUP_COUNT, g_kupl_memory_pool_size);
    for (int i = 0; i < max_warmup_count; ++i) {
        auto mem = KUPL_MEM(i);
        if (mem != nullptr) {
            for (size_t j = 0; j < MEM_WARMUP_SIZE_COUNT; j++) {
                auto ptr = kupl_fast_alloc(mem, g_memory_warmup_size[j]);
                if (ptr != nullptr) {
                    kupl_fast_free(mem, ptr);
                }
            }
        }
    }
}

static kupl_always_inline
void kupl_memory_pool_free()
{
    for (int i = 0; i < g_kupl_memory_pool_size; ++i) {
        if (KUPL_MEM(i) != nullptr) {
            free(KUPL_MEM(i));
            KUPL_MEM(i) = nullptr;
        }
    }
    free(g_kupl_memory_pool);
    g_kupl_memory_pool = nullptr;

    if (g_kupl_hbw_memory_pool == nullptr) {
        return;
    }

    for (int i = 0; i < g_kupl_memory_pool_size; ++i) {
        if (KUPL_HBW_MEM(i) != nullptr) {
            free(KUPL_HBW_MEM(i));
            KUPL_HBW_MEM(i) = nullptr;
        }
    }
    free(g_kupl_hbw_memory_pool);
    g_kupl_hbw_memory_pool = nullptr;
}

bool check_numa_has_hugepages(int nodeId)
{
    std::string base_path = "/sys/devices/system/node/node" + std::to_string(nodeId) + "/hugepages";

    DIR *dir = opendir(base_path.c_str());
    if (!dir) {
        kupl_error("Failed to open directory");
        return false;
    }

    bool found_hugepages = false;
    struct dirent *entry;

    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR && strncmp(entry->d_name, "hugepages-", 10) == 0) {
            std::string size_dir = base_path + "/" + entry->d_name;
            std::string nr_file = size_dir + "/nr_hugepages";

            std::ifstream file(nr_file);
            if (file.is_open()) {
                int nr_hugepages;
                file >> nr_hugepages;
                if (nr_hugepages > 0) {
                    found_hugepages = true;
                    break;
                }
            }
        }
    }
    closedir(dir);
    return found_hugepages;
}

static kupl_always_inline
void kupl_hugepage_init()
{
    if (!kupl_config_get_value(KUPL_ENABLE_HUGEPAGES)) {
        return;
    }
    const kupl_host_info_t *info = kupl_get_host_info();
    if (info->numa_cnt > KUPL_NUMA_MAX) {
        kupl_warn("Unable to get numa info, hugepage malloc fallback to regular.");
        return;
    }
    for (int i = 0; i < info->numa_cnt; i++) {
        g_is_hugepage_exist[i] = check_numa_has_hugepages(i);
    }
    g_enable_hugepages = true;
    return;
}

int kupl_memory_expand()
{
    if (kupl_unlikely(g_kupl_memory_pool_size >= g_kupl_max_memory_pool_size)) {
        return KUPL_ERROR;
    }
    auto mem = (kupl_memory_t *)calloc(1, sizeof(kupl_memory_t));
    if (kupl_unlikely(mem == nullptr)) {
        kupl_error("There is no memory for kupl memory pool expand");
        return KUPL_ERROR;
    }

    mem->eid = (uint16_t)g_kupl_memory_pool_size;
    kupl_fast_init(mem);
    kupl_normal_init(mem);
    KUPL_MEM(g_kupl_memory_pool_size) = mem;

    auto hbw_mem = (kupl_memory_t *)calloc(1, sizeof(kupl_memory_t));
    if (kupl_unlikely(hbw_mem == nullptr)) {
        kupl_error("There is no memory for kupl memory pool expand");
        free(KUPL_MEM(g_kupl_memory_pool_size));
        return KUPL_ERROR;
    }

    hbw_mem->eid = (uint16_t)g_kupl_memory_pool_size;
    kupl_fast_init(hbw_mem);
    kupl_normal_init(hbw_mem);
    KUPL_HBW_MEM(g_kupl_memory_pool_size) = hbw_mem;

    g_kupl_memory_pool_size++;
    return KUPL_OK;
}

void kupl_memory_expand_fini()
{
    auto mem = KUPL_MEM(g_kupl_memory_pool_size - 1);
    if (mem != nullptr) {
        kupl_fast_cleanup(mem);
        MEM_STATIS(
            printf("[Eid %d] fast self:%lu sync:%lu other:%lu\n",
                    g_kupl_memory_pool_size - 1, mem->stats_fast_self, mem->stats_fast_sync, mem->stats_fast_other);
        );
        kupl_normal_cleanup(mem);
        MEM_STATIS(
            printf("[Eid:%d] normal list-travel:%lu bin-travel:%lu alloc:%lu free:%lu\n",
                    g_kupl_memory_pool_size - 1, mem->stats_mem_link_travel, mem->stats_mem_bin_travel,
                    mem->stats_mem_alloc_count, mem->stats_mem_free_count);
        );
        free(mem);
    }

    auto hbw_mem = KUPL_HBW_MEM(g_kupl_memory_pool_size - 1);
    if (hbw_mem != nullptr) {
        kupl_fast_cleanup(hbw_mem, true);
        MEM_STATIS(
            printf("[Eid %d] fast self:%lu sync:%lu other:%lu\n", g_kupl_memory_pool_size - 1,
                    hbw_mem->stats_fast_self, hbw_mem->stats_fast_sync, hbw_mem->stats_fast_other);
        );
        kupl_normal_cleanup(hbw_mem, true);
        MEM_STATIS(
        printf("[Eid:%d] normal list-travel:%lu bin-travel:%lu alloc:%lu free:%lu\n",
                g_kupl_memory_pool_size - 1, hbw_mem->stats_mem_link_travel, hbw_mem->stats_mem_bin_travel,
                hbw_mem->stats_mem_alloc_count, hbw_mem->stats_mem_free_count);
        );
        free(hbw_mem);
    }

    g_kupl_memory_pool_size--;
}

static kupl_always_inline
int kupl_memory_init()
{
    const kupl_host_info_t *info = kupl_get_host_info();
    g_kupl_max_memory_pool_size = info->pu_cnt;
    g_kupl_memory_pool_size = info->avail_pu_cnt;
    g_kupl_memory_pool = (kupl_memory_t **)calloc(1, sizeof(kupl_memory_t *) * (size_t)g_kupl_max_memory_pool_size);
    if (kupl_unlikely(g_kupl_memory_pool == nullptr)) {
        return kupl_log_error_return(ERROR, "There is no memory for kupl memory pool meta");
    }

    for (int i = 0; i < g_kupl_memory_pool_size; ++i) {
        auto mem = (kupl_memory_t *)calloc(1, sizeof(kupl_memory_t));
        if (kupl_unlikely(mem == nullptr)) {
            kupl_error("There is no memory for kupl memory pool");
            kupl_memory_pool_free();
            return KUPL_ERROR;
        }

        mem->eid = (uint16_t)i;
        kupl_fast_init(mem);
        kupl_normal_init(mem);
        KUPL_MEM(i) = mem;
    }

    g_kupl_hbw_memory_pool = (kupl_memory_t **)calloc(1, sizeof(kupl_memory_t *) * (size_t)g_kupl_max_memory_pool_size);

    if (kupl_unlikely(g_kupl_hbw_memory_pool == nullptr)) {
        kupl_memory_pool_free();
        return kupl_log_error_return(ERROR, "There is no memory for kupl memory pool meta");
    }

    for (int i = 0; i < g_kupl_memory_pool_size; ++i) {
        auto mem = (kupl_memory_t *)calloc(1, sizeof(kupl_memory_t));
        if (kupl_unlikely(mem == nullptr)) {
            kupl_error("There is no memory for kupl memory pool");
            kupl_memory_pool_free();
            return KUPL_ERROR;
        }

        mem->eid = (uint16_t)i;
        kupl_fast_init(mem);
        kupl_normal_init(mem);
        KUPL_HBW_MEM(i) = mem;
    }

    g_kupl_user_align_config = kupl_config_get_value(KUPL_MPOOL_ALIGN_SIZE);

    kupl_trace("grow :%lu max:%lu chunk:%lu\n",
                MEM_CHUNK_GROW_SIZE, MEM_MAX_SIZE, MEM_CHUNK_GROW_SIZE - sizeof(kupl_buffer_desc_t));
    kupl_trace("kupl_buffer_desc :%lu\n", sizeof(kupl_buffer_desc_t));
    kupl_hugepage_init();
    return KUPL_OK;
}

static kupl_always_inline
void kupl_memory_fini()
{
    /* free fast memory to normal pool */
    for (int i = 0; i < g_kupl_memory_pool_size; ++i) {
        auto mem = KUPL_MEM(i);
        if (mem != nullptr) {
            kupl_fast_cleanup(mem);
            MEM_STATIS(
                printf("[Eid %d] fast self:%lu sync:%lu other:%lu\n",
                        i, mem->stats_fast_self, mem->stats_fast_sync, mem->stats_fast_other);
            );
        }
    }

    /* free normal memory */
    for (int i = 0; i < g_kupl_memory_pool_size; ++i) {
        auto mem = KUPL_MEM(i);
        if (mem != nullptr) {
            kupl_normal_cleanup(mem);
            MEM_STATIS(
                printf("[Eid:%d] normal list-travel:%lu bin-travel:%lu alloc:%lu free:%lu\n",
                        i, mem->stats_mem_link_travel, mem->stats_mem_bin_travel,
                        mem->stats_mem_alloc_count, mem->stats_mem_free_count);
            );
        }
    }

    /* free fast memory to normal pool */
    for (int i = 0; i < g_kupl_memory_pool_size; ++i) {
        auto mem = KUPL_HBW_MEM(i);
        if (mem != nullptr) {
            kupl_fast_cleanup(mem, true);
            MEM_STATIS(
                printf("[Eid %d] fast self:%lu sync:%lu other:%lu\n",
                       i, mem->stats_fast_self, mem->stats_fast_sync, mem->stats_fast_other);
            );
        }
    }

    /* free normal memory */
    for (int i = 0; i < g_kupl_memory_pool_size; ++i) {
        auto mem = KUPL_HBW_MEM(i);
        if (mem != nullptr) {
            kupl_normal_cleanup(mem, true);
            MEM_STATIS(
            printf("[Eid:%d] normal list-travel:%lu bin-travel:%lu alloc:%lu free:%lu\n",
                    i, mem->stats_mem_link_travel, mem->stats_mem_bin_travel,
                    mem->stats_mem_alloc_count, mem->stats_mem_free_count);
            );
        }
    }

    kupl_memory_pool_free();

    return;
}

void* kupl_memory_alloc_inner(size_t size, int geid)
{
    if (kupl_unlikely(size == 0 || size > KUPL_MAX_MALLOC_INNER_SIZE
        || geid < 0 || geid >= g_kupl_memory_pool_size)) {
        return nullptr;
    }

#ifdef ENABLE_KUPL_GLIBC_MALLOC
    void *ptr = malloc(size);
#else
    auto mem = KUPL_MEM(geid);
    auto ptr = kupl_fast_alloc(mem, size);
#endif
    return ptr;
}

void* kupl_memory_hbw_alloc_inner(size_t size, int geid)
{
#ifdef ENABLE_KUPL_GLIBC_MALLOC
    void *ptr = malloc(size);
#else
    auto mem = KUPL_HBW_MEM(geid);
    auto ptr = kupl_fast_alloc(mem, size, true);
#endif
    return ptr;
}

void* kupl_memory_calloc_inner(size_t size, int geid)
{
    if (kupl_unlikely(size == 0 || size > KUPL_MAX_MALLOC_INNER_SIZE
        || geid < 0 || geid >= g_kupl_memory_pool_size)) {
        return nullptr;
    }

    auto ptr = kupl_memory_alloc_inner(size, geid);
    if (kupl_likely(ptr != nullptr)) {
        memset(ptr, 0, size);
        return ptr;
    }

    return nullptr;
}

void kupl_memory_free_inner(void *ptr, int geid)
{
    if (kupl_unlikely(ptr == nullptr
        || geid < 0 || geid >= g_kupl_memory_pool_size)) {
        return;
    }

#ifdef ENABLE_KUPL_GLIBC_MALLOC
    free(ptr);
    return;
#else
    auto mem = KUPL_MEM(geid);
    kupl_fast_free(mem, ptr);
#endif
}

void kupl_memory_hbw_free_inner(void *ptr, int geid)
{
    if (kupl_unlikely(ptr == nullptr
        || geid < 0 || geid >= g_kupl_memory_pool_size)) {
        return;
    }

#ifdef ENABLE_KUPL_GLIBC_MALLOC
    free(ptr);
    return;
#else
    auto mem = KUPL_HBW_MEM(geid);
    kupl_fast_free(mem, ptr, true);
#endif
}

bool kupl_memory_is_inited()
{
    return g_mpool_inited;
}

void *kupl_malloc_hugepages_inner(size_t size, size_t *align_size, int use_hbw)
{
    void* ptr;
    if (size == 0) {
        ptr = malloc(0);
        *align_size = 0;
        return ptr;
    }
	*align_size = ALIGN_TO_PAGE_SIZE(size);
    ptr = mmap(NULL, *align_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (kupl_unlikely(ptr == MAP_FAILED)) {
        kupl_error("mmap ptr failed");
        return nullptr;
    }
    // mbind to HBW
    if (use_hbw) {
        int cpu = sched_getcpu();
        int rnode = numa_node_of_cpu(cpu) + KUPL_HBW_BIAS;
        unsigned long mask = 1UL << rnode;
        long int success = mbind(ptr, (unsigned long)(*align_size), MPOL_BIND, &mask, KUPL_NUMA_MAX, MPOL_MF_STRICT);
        if (kupl_unlikely(success != 0)) {
            kupl_error("mbind to hbw node #%d failed!", rnode);
        }
    }

    return ptr;
}

void kupl_free_hugepages_inner(void *ptr, size_t align_size)
{
    if (align_size == 0) {
        free(ptr);
        return;
    }
    if (ptr != nullptr) {
        munmap(ptr, align_size);
    }
}

void *kupl_mpool_malloc_hugepages_inner(size_t size, size_t *align_size, int use_hbw)
{
    void* ptr;
    if (size == 0) {
        ptr = malloc(0);
        *align_size = 0;
        return ptr;
    }
    *align_size = ALIGN_TO_PAGE_SIZE(size);
    int geid = kupl_get_executor_num();
    if (geid < 0 || geid >= g_kupl_memory_pool_size) {
        return nullptr;
    }
    if (use_hbw) {
        ptr = kupl_fast_alloc(KUPL_HBW_MEM(geid), *align_size, true, true);
    } else {
        ptr = kupl_fast_alloc(KUPL_MEM(geid), *align_size, false, true);
    }
    return ptr;
}

void kupl_mpool_free_hugepages_inner(void *ptr, size_t align_size, int use_hbw)
{
    if (align_size == 0) {
        free(ptr);
        return;
    }
    int geid = kupl_get_executor_num();
    if (geid < 0 || geid >= g_kupl_memory_pool_size) {
        return;
    }
    if (use_hbw) {
        kupl_memory_hbw_free_inner(ptr, geid);
    } else {
        kupl_memory_free_inner(ptr, geid);
    }
}

#include <map>
#include "executor/backend/kupl_executor_backend.h"
#include "dm/memcpy/kupl_memcpy.h"

static kupl_lock_t *g_pin_map_lock = nullptr;
typedef struct kupl_memory_info {
    size_t size;
    uint64_t cookie;
    int sdma_fd;
} kupl_memory_info_t;
static std::map<void*, kupl_memory_info_t*> *g_pin_map;

static pin_umem_sdma kupl_sdma_pin_umem = nullptr;
static unpin_umem_sdma kupl_sdma_unpin_umem = nullptr;

bool kupl_sdma_mpool_func_init()
{
    if (!g_sdma_func_init) {
        return false;
    }
    sdma_func_list_t func_l = get_sdma_dl_func_l();
    kupl_sdma_pin_umem = func_l.kupl_sdma_pin_umem;
    kupl_sdma_unpin_umem = func_l.kupl_sdma_unpin_umem;
    if (kupl_sdma_pin_umem && kupl_sdma_unpin_umem) {
        return true;
    } else {
        return false;
    }
}

void kupl_sdma_mpool_func_fini()
{
    kupl_sdma_pin_umem = nullptr;
    kupl_sdma_unpin_umem = nullptr;
}

int kupl_mpool_init()
{
    if (kupl_hbw_init() != KUPL_OK) {
        goto err_hbw_init;
    }

    sdma_mpool_func_init = kupl_sdma_mpool_func_init();
    if (kupl_unlikely(kupl_memory_init() == KUPL_ERROR)) {
        kupl_error("Initialize memory pool failed");
        goto err_memory_init;
    }
    if (sdma_mpool_func_init) {
        g_pin_map = new (std::nothrow) std::map<void*, kupl_memory_info_t*>;
        if (g_pin_map == nullptr) {
            goto err_map_init;
        }
        g_pin_map_lock = kupl_lock_create(PTHREAD_SPINLOCK);
        if (g_pin_map_lock == nullptr) {
            goto err_map_lock_init;
        }
    }
    (void)kupl_memory_warmup;
    g_mpool_inited = true;

    return KUPL_OK;

err_map_lock_init:
    delete g_pin_map;
    g_pin_map = nullptr;
err_map_init:
    kupl_memory_fini();
err_memory_init:
    kupl_sdma_mpool_func_fini();
err_hbw_init:
    return KUPL_ERROR;
}

void kupl_mpool_fini()
{
    if (!g_mpool_inited) {
        return;
    }
    if (sdma_mpool_func_init) {
        kupl_lock_cleanup(g_pin_map_lock);
        delete g_pin_map;
        g_pin_map = nullptr;
        sdma_mpool_func_init = false;
    }
    kupl_memory_fini();
    kupl_sdma_mpool_func_fini();
    kupl_hbw_fini();
    g_mpool_inited = false;
}

bool kupl_get_sdma_mpool_func_init()
{
    return sdma_mpool_func_init;
}

int kupl_memory_is_pinned(void* addr, size_t count)
{
    if (sdma_mpool_func_init) {
        g_pin_map_lock->lock(g_pin_map_lock);
        bool res = false;
        auto tmp = g_pin_map->upper_bound(addr);
        if (tmp != g_pin_map->begin()) {
            --tmp;
            if ((char *)tmp->first + tmp->second->size >= (char *)addr + count) {
                res = true;
            }
        }
        g_pin_map_lock->unlock(g_pin_map_lock);
        if (!res) {
            return KUPL_MEMORY_UNPINNED;
        }
    }
    return KUPL_MEMORY_PINNED;
}

void* kupl_malloc(kupl_mem_kind_t kind, size_t size)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return nullptr;
    }
    if (kupl_unlikely(size > KUPL_MAX_MALLOC_SIZE)) {
        kupl_error("Unable to malloc memory larger than 128GB.");
        return nullptr;
    }

    void* ptr = nullptr;
    static thread_local int geid = kupl_get_executor_num();
    switch (kind) {
        case KUPL_MEM_LARGE_CAP:
            ptr = kupl_memory_alloc_inner(size, geid);
            break;
        case KUPL_MEM_HIGH_BW:
            ptr = kupl_memory_hbw_alloc_inner(size, geid);
            break;
        case KUPL_MEM_DEFAULT:
            if (g_mpool_default_hbw) {
                ptr = kupl_memory_hbw_alloc_inner(size, geid);
            } else {
                ptr = kupl_memory_alloc_inner(size, geid);
            }
            break;
        default:
            kupl_error("Kupl Malloc:Invalid memory kind.");
    }

    return ptr;
}

void kupl_free(kupl_mem_kind_t kind, void *ptr)
{
    static thread_local int geid = kupl_get_executor_num();
    switch (kind) {
        case KUPL_MEM_LARGE_CAP:
            kupl_memory_free_inner(ptr, geid);
            break;
        case KUPL_MEM_HIGH_BW:
            kupl_memory_hbw_free_inner(ptr, geid);
            break;
        case KUPL_MEM_DEFAULT:
            if (g_mpool_default_hbw) {
                kupl_memory_hbw_free_inner(ptr, geid);
            } else {
                kupl_memory_free_inner(ptr, geid);
            }
            break;
        default:
            kupl_error("Kupl Free:Invalid memory kind.");
    }
}

int kupl_mlock(void *buffer, size_t count)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(!sdma_mpool_func_init || count == 0 || buffer == nullptr)) {
        return KUPL_ERROR;
    }

    int fd_index = kupl_get_self_affinity() / cores_per_sdma % KUPL_MAX_SDMA_DEVICE_SIZE;
    int fd = g_sdma_fd[fd_index];
    if (kupl_unlikely(fd == 0)) {
        return KUPL_ERROR;
    }

    g_pin_map_lock->lock(g_pin_map_lock);
    auto iter = g_pin_map->find(buffer);
    if (iter == g_pin_map->end()) {
        kupl_memory_info_t *mem_info = (kupl_memory_info_t*)malloc(sizeof(kupl_memory_info_t));
        if (mem_info == nullptr) {
            g_pin_map_lock->unlock(g_pin_map_lock);
            return KUPL_ERROR;
        }
        mem_info->size = count;
        mem_info->sdma_fd = fd;
        g_pin_map->insert(std::make_pair(buffer, mem_info));
        kupl_sdma_pin_umem(fd, buffer, (uint32_t)count, &mem_info->cookie);
    } else if (iter->second->size < count) {
        kupl_sdma_unpin_umem(iter->second->sdma_fd, iter->second->cookie);
        iter->second->sdma_fd = fd;
        iter->second->size = count;
        kupl_sdma_pin_umem(fd, buffer, (uint32_t)count, &iter->second->cookie);
    }
    g_pin_map_lock->unlock(g_pin_map_lock);
    return KUPL_OK;
}

int kupl_munlock(void *buffer, size_t count)
{
    if (kupl_unlikely(!sdma_mpool_func_init || count == 0 || buffer == nullptr)) {
        return KUPL_ERROR;
    }

    int res = KUPL_ERROR;
    g_pin_map_lock->lock(g_pin_map_lock);
    auto iter = g_pin_map->find(buffer);
    if (iter != g_pin_map->end() && iter->second->size == count) {
        kupl_sdma_unpin_umem(iter->second->sdma_fd, iter->second->cookie);
        free(iter->second);   // free kupl_memory_info_t
        g_pin_map->erase(iter);
        res = KUPL_OK;
    }
    g_pin_map_lock->unlock(g_pin_map_lock);
    return res;
}
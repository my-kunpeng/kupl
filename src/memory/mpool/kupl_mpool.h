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
#ifndef KUPL_MPOOL_H
#define KUPL_MPOOL_H

#include <pthread.h>
#include <sched.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KUPL_MEMORY_UNPINNED 0
#define KUPL_MEMORY_PINNED 1
#define KUPL_MALLOC_PAGESIZE 4096
#define KUPL_MAX_MALLOC_INNER_SIZE (1uLL << 37)
#define KUPL_MAX_MALLOC_SIZE (1uLL << 37)
extern int g_kupl_memory_pool_size;
extern bool g_mpool_inited;

int kupl_memory_is_pinned(void* addr, size_t count);
int kupl_mpool_init(void);
void kupl_mpool_fini(void);
int kupl_memory_expand(void);
void kupl_memory_expand_fini(void);

/*
    Notice that allocating memory for another thread is forbidden.
    Code using memory_alloc api should be checked that
    geid matches its own global executor id.
*/
void* kupl_memory_alloc_inner(size_t size, int geid);
void* kupl_memory_calloc_inner(size_t size, int geid);
void kupl_memory_free_inner(void *ptr, int geid);
void* kupl_memory_hbw_alloc_inner(size_t size, int geid);
void kupl_memory_hbw_free_inner(void *ptr, int geid);

#define KUPL_CHECK_MEM 0
#if KUPL_CHECK_MEM
#define kupl_memory_alloc(_s, _geid)           \
({                                              \
    PROFILE_DATA_STATS(0xa110c, __FUNCTION__);  \
    kupl_memory_alloc_inner(_s, _geid);        \
})
#define kupl_memory_calloc(_s, _geid)          \
({                                              \
    PROFILE_DATA_STATS(0xca110c, __FUNCTION__); \
    kupl_memory_calloc_inner(_s, _geid);       \
})
#define kupl_memory_free(_p, _geid)            \
({                                              \
    PROFILE_DATA_STATS(0xf1ee, __FUNCTION__);   \
    kupl_memory_free_inner(_p, _geid);         \
})
#else
#define kupl_memory_alloc(_s, _geid)     kupl_memory_alloc_inner(_s, _geid)
#define kupl_memory_calloc(_s, _geid)    kupl_memory_calloc_inner(_s, _geid)
#define kupl_memory_free(_p, _geid)      kupl_memory_free_inner(_p, _geid)
#endif

/**
 * @brief Check the memory module whether initialed
 * @return true for initialized.
 */
bool kupl_memory_is_inited(void);

/**
 * @brief Allocates size bytes of memory and returns a void * pointer to the start of that memory
 *
 * @param [in] size         the size of the allocated memory
 *
 * @return the prt of the memory
 */
void* kupl_malloc_inner(size_t size);

/**
 * @brief Allocates memory for an array of objects, zero-initializes all bytes in allocated storage,
 * and if allocation succeeds, returns a pointer to the first byte in the allocated memory block
 *
 * @param [in] num          the number of the allocated memory object
 * @param [in] size         the size of each object
 *
 * @return the prt of the memory
 */
void* kupl_calloc(size_t num, size_t size);

/**
 * @brief Allocates size bytes of memory with alignment of size alignment and
 * returns a void * pointer to the start of that memory
 *
 * @param [in] alignment    the size of the alignment
 * @param [in] size         the size of new allocated memory
 *
 * @return the prt of the memory
 */
void* kupl_aligned_alloc(size_t alignment, size_t size);

/**
 * @brief Deallocates memory
 *
 * @param [in] ptr          the memory ptr
 */
void kupl_free_inner(void *ptr);

/**
 * @brief Deallocates memory
 *
 * @param [in] ptr          the memory ptr
 */
#define kupl_safe_free(_p)     \
do {                            \
    if ((_p) != nullptr) {      \
        kupl_free_inner(_p);         \
        (_p) = nullptr;         \
    }                           \
} while (0)

/**
 * @brief Allocates huge page memory
 *
 * @param [in] size             the memory size
 * @param [out] align_size      the memory aligned size
 * @param [in] use_hbw          whether it is hbw alloacation
 */
void *kupl_malloc_hugepages_inner(size_t size, size_t* align_size, int use_hbw);

/**
 * @brief Deallocates huge page memory
 *
 * @param [in] ptr             the memory ptr
 * @param [in] align_size      the memory aligned size
 */
void kupl_free_hugepages_inner(void *ptr, size_t align_size);

/**
 * @brief Allocates huge page memory
 *
 * @param [in] size             the memory size
 * @param [out] align_size      the memory aligned size
 * @param [in] use_hbw          whether it is hbw alloacation
 */
void *kupl_mpool_malloc_hugepages_inner(size_t size, size_t *align_size, int use_hbw);

/**
 * @brief Deallocates huge page memory
 *
 * @param [in] ptr              the memory ptr
 * @param [in] use_hbw          whether it is hbw free
 */
void kupl_mpool_free_hugepages_inner(void *ptr, size_t align_size, int use_hbw);

/**
 * @brief Check whether sdma mpool func is inited
 *
 * @return sdma_mpool_func_init
 */
bool kupl_get_sdma_mpool_func_init(void);

#ifdef __cplusplus
}
#endif

#endif
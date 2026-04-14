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
#ifndef KUPL_FENCE_H
#define KUPL_FENCE_H

#include <atomic>
#include <climits>
#include "kupl.h"
#include "utils/arch/kupl_cache.h"
#include "memory/shm/kupl_shm.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__aarch64__)
#define kupl_aarch64_dmb(_op)          asm volatile ("dmb " #_op ::: "memory")
#define kupl_aarch64_isb(_op)          asm volatile ("isb " #_op ::: "memory")
#define kupl_aarch64_dsb(_op)          asm volatile ("dsb " #_op ::: "memory")

#define kupl_memory_cpu_fence()        kupl_aarch64_dmb(ish)
#define kupl_memory_cpu_store_fence()  kupl_aarch64_dmb(ishst)
#define kupl_memory_cpu_load_fence()   kupl_aarch64_dmb(ishld)
#endif

#define KUPL_SHM_FENCE_FLAG_WIN_SIZE    (3)

/** @brief fence algo to select the fence algorithm */
enum kupl_fence_algo_t {
    /**
     * when use the default type, user can set KUPL_FENCE_ALGORITHM to choose the real algorithm
     */
    KUPL_FENCE_ALGO_DEFAULT,
    /**
     * choose the P2P algorithm, only process can chose
     */
    KUPL_FENCE_ALGO_P2P,
    /**
     * choose the linear algorithm
     */
    KUPL_FENCE_ALGO_LINEAR,
    /**
     * choose the rd algorithm
     */
    KUPL_FENCE_ALGO_RD,
    KUPL_FENCE_ALGO_MAX,
};

/** @brief the handle of kupl fence */
typedef struct kupl_fence* kupl_fence_h;

typedef struct kupl_fence_ops {
    int  (*create)(kupl_fence_h fence);
    void (*destroy)(kupl_fence_h fence);
    void (*wait)(kupl_fence_h fence, int local_tid, int local_tnum);
} kupl_fence_ops_t;

typedef struct kupl_fence_rd_args {
    int loop_times;
    int near_poweroftwo;
} kupl_fence_rd_args_t;

typedef struct kupl_fence {
    kupl_fence_algo_t       algo;
    kupl_shm_win_h          local_win;
    kupl_fence_ops_t        *ops;
    int                     size;
    int                     flag_idx;
    int                     *flag_peer_idx;
    void                    *bar;
    kupl_shm_win_h          win;
    union {
        kupl_fence_rd_args_t rd;
    } args;
} kupl_fence_t;

void kupl_fence_ops_set(kupl_fence_algo_t algo, kupl_fence_ops_t *ops);

void kupl_set_fence_linear_ops(void);

void kupl_set_fence_peer_ops(void);

void kupl_set_fence_rd_ops(void);

/**
 * @brief init the fence ops for algo
 */
void kupl_fence_init(void);

/**
 * @brief create barier
 *
 * @param [in] type     the type of fence
 * @param [in] algo     the algo of fence
 * @param [in] mode     the mode of fence
 * @param [in] local_win the local_win of fence
 * @return kupl_fence_h  the handler of fence
 */
kupl_fence_h kupl_fence_create(kupl_fence_algo_t algo, kupl_shm_win_h local_win);

kupl_fence_algo_t kupl_fence_algo_select();

/**
 * @brief destroy fence
 *
 * @param [in] fence  the handler of fence
 */
void kupl_fence_destroy(kupl_fence_h fence);

/**
 * @brief fence synchronization
 *
 * @param [in] fence          the fence
 * @param [in] local_tid        local thread id
 * @param [in] local_tnum       local thread num
 * @param [in] reduce           reduce function
 * @param [in] reduce_data      reduce data
 */
void kupl_fence_wait(kupl_fence_h fence, int local_tid, int local_tnum);

// only use in process fence
static kupl_always_inline
void kupl_fence_set_flag(kupl_fence_h fence, int id, int local_num, int flag_val, int rank, int flag_idx)
{
    (void)id;
    (void)local_num;
    kupl_memory_cpu_store_fence();
    void* peer_baseptr;
    int *flag;
    bool is_comm_fence = (fence->algo != KUPL_FENCE_ALGO_P2P);
    if (kupl_shm_win_query(fence->win, id, &peer_baseptr) == -1) {
        kupl_error("kupl shm win qurey failed in set_flag");
        return;
    }

    // offset for comm_fence
    if (is_comm_fence) {
        peer_baseptr = reinterpret_cast<char *>(peer_baseptr)
                        + KUPL_CACHE_LINE * KUPL_SHM_FENCE_FLAG_WIN_SIZE * fence->size;
    }
    int factor = is_comm_fence ? 1 : fence->size;
    flag = reinterpret_cast<int *>(reinterpret_cast<char *>(peer_baseptr)
            + flag_idx * KUPL_CACHE_LINE * factor
            + rank * KUPL_CACHE_LINE);
    *flag = flag_val;
}

// only use in process fence
static kupl_always_inline
int kupl_fence_get_flag(kupl_fence_h fence, int id, int local_num, int rank, int flag_idx)
{
    (void)local_num;
    kupl_memory_cpu_load_fence();
    void* peer_baseptr;
    int *flag;
    bool is_comm_fence = (fence->algo != KUPL_FENCE_ALGO_P2P);
    if (kupl_shm_win_query(fence->win, id, &peer_baseptr) == -1) {
        return KUPL_ERROR;
    }

    // offset for comm_fence
    if (is_comm_fence) {
        peer_baseptr = reinterpret_cast<char *>(peer_baseptr)
                        + KUPL_CACHE_LINE * KUPL_SHM_FENCE_FLAG_WIN_SIZE * fence->size;
    }
    int factor = is_comm_fence ? 1 : fence->size;
    flag = reinterpret_cast<int *>(reinterpret_cast<char *>(peer_baseptr)
            + flag_idx * KUPL_CACHE_LINE * factor
            + rank * KUPL_CACHE_LINE);
    return *flag;
}

void kupl_fence_set_flag_idx(kupl_fence_h fence, int local_id, int flag_idx);

#ifdef __cplusplus
}
#endif

#endif
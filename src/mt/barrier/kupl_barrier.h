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
#ifndef KUPL_BARRIER_H
#define KUPL_BARRIER_H

#include <atomic>
#include <climits>
#include "kupl.h"
#include "executor/kupl_executor.h"
#include "executor/backend/kupl_executor_backend.h"
#include "mt/kupl_graph.h"
#include "utils/arch/kupl_cache.h"
#include "memory/shm/kupl_shm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KUPL_BAR_CACHE_LINE      128
#define KUPL_BAR_ALIGN           KUPL_ALIGN(KUPL_BAR_CACHE_LINE)

#define KUPL_BARRIER_STATE_BUMP      4
#define KUPL_BAR_NOT_READY           1
#define KUPL_BAR_READY_TO_GO         0
#define KUPL_BAR_IS_MASTER(_id)      ((_id) == 0)

#if defined(__aarch64__)
#define kupl_aarch64_dmb(_op)          asm volatile ("dmb " #_op ::: "memory")
#define kupl_aarch64_isb(_op)          asm volatile ("isb " #_op ::: "memory")
#define kupl_aarch64_dsb(_op)          asm volatile ("dsb " #_op ::: "memory")

#define kupl_memory_cpu_fence()        kupl_aarch64_dmb(ish)
#define kupl_memory_cpu_store_fence()  kupl_aarch64_dmb(ishst)
#define kupl_memory_cpu_load_fence()   kupl_aarch64_dmb(ishld)
#endif

#define KUPL_SHM_BARRIER_FLAG_WIN_SIZE    (3)

/** @brief barrier algo to select the barrier algorithm */
enum kupl_barrier_algo_t {
    /**
     * when use the default type, user can set KUPL_BARRIER_ALGORITHM to choose the real algorithm
     */
    KUPL_BARRIER_ALGO_DEFAULT,
    /**
     * choose the dist algorithm
     */
    KUPL_BARRIER_ALGO_DIST,
    KUPL_BARRIER_ALGO_MAX,
};

/** @brief the handle of kupl barrier */
typedef struct kupl_barrier* kupl_barrier_h;

typedef void (*kupl_barrier_reduce_t)(void *, void *);

typedef struct kupl_barrier_ops {
    int  (*create)(kupl_barrier_h barrier);
    void (*destroy)(kupl_barrier_h barrier);
    int  (*prepare)(kupl_barrier_h barrier);
    void (*wait)(kupl_barrier_h barrier, int local_tid, int local_tnum);
    void (*fork)(kupl_barrier_h barrier, int local_tid, int local_tnum);
    void (*join)(kupl_barrier_h barrier, int local_tid, int local_tnum);
} kupl_barrier_ops_t;

typedef struct barrier_atomic {
    KUPL_ATOMIC_INT  b;
} KUPL_BAR_ALIGN barrier_atomic_t;

typedef struct barrier_volatile {
    int volatile    b;
} KUPL_BAR_ALIGN barrier_volatile_t;

typedef struct kupl_barrier_rd_args {
    int loop_times;
    int near_poweroftwo;
} kupl_barrier_rd_args_t;

typedef struct kupl_barrier {
    kupl_barrier_algo_t     algo;
    kupl_barrier_ops_t      *ops;
    kupl_graph_h            graph;
    int                     max_size;
    int                     size;
    int                     flag_idx;
    void                    *bar;
} kupl_barrier_t;

void kupl_barrier_ops_set(kupl_barrier_algo_t algo, kupl_barrier_ops_t *ops);

void kupl_set_barrier_dist_ops(void);

/**
 * @brief init the barrier ops for algo
 */
void kupl_barrier_init(void);

/**
 * @brief create barier
 *
 * @param [in] algo     the algo of barrier
 * @return kupl_barrier_h  the handler of barrier
 */
kupl_barrier_h kupl_barrier_create(kupl_barrier_algo_t algo);

void kupl_barrier_prepare(kupl_barrier_h barrier, kupl_graph_h graph, int num_threads);

int kupl_barrier_prepare_dummy(kupl_barrier_h barrier);

/**
 * @brief destroy barrier
 *
 * @param [in] barrier  the handler of barrier
 */
void kupl_barrier_destroy(kupl_barrier_h barrier);

/**
 * @brief barrier wait
 *
 * @param [in] barrier          the barrier
 * @param [in] local_tid        local thread id
 * @param [in] local_tnum       local thread num
 */
void kupl_barrier_wait(kupl_barrier_h barrier, int local_tid, int local_tnum);

/**
 * @brief barrier fork
 *
 * @param [in] barrier          the barrier
 * @param [in] local_tid        local thread id
 * @param [in] local_tnum       local thread num
 */
void kupl_barrier_fork(kupl_barrier_h barrier, int local_tid, int local_tnum);

/**
 * @brief barrier join
 *
 * @param [in] barrier          the barrier
 * @param [in] local_tid        local thread id
 * @param [in] local_tnum       local thread num
 */
void kupl_barrier_join(kupl_barrier_h barrier, int local_tid, int local_tnum);

int kupl_init_bar(kupl_barrier_h barrier);

void kupl_destroy_bar(kupl_barrier_h barrier);

int kupl_barrier_change_size(kupl_barrier_h barrier, int size);

void kupl_barrier_set_flag_idx(kupl_barrier_h barrier, int local_id, int flag_idx);

static kupl_always_inline
void kupl_barrier_busy_wait(kupl_barrier_h barrier, int tid)
{
    (void)barrier;
    (void)tid;
    kupl_sched_t *sched = kupl_get_global_sched();
    kupl_sched_execute_tb(sched);
}

#ifdef __cplusplus
}
#endif

namespace kupl {
    struct FlagBarrier {
    public:
        FlagBarrier(const FlagBarrier&) = delete;
        FlagBarrier& operator=(const FlagBarrier&) = delete;

        static FlagBarrier& getInstance()
        {
            static FlagBarrier instance;
            return instance;
        }

        void notify(kupl_egroup_h egroup, int num_threads);
        bool arrive(int geid);
        void leave(kupl_egroup_h egroup, int num_threads, int master_eid);
        void wait(kupl_egroup_h egroup, int num_threads);

    private:
        struct flag {
            volatile size_t     vf;
        } KUPL_ALIGN(128);
        FlagBarrier() = default;
        ~FlagBarrier() = default;
        flag flags_[1024];
        static constexpr int FLAG_READY = 1;
        static constexpr int FLAG_ARRIVED = 2;
        static constexpr int FLAG_LEAVE = 0;
    };
}

#endif
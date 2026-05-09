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
#include "kupl_barrier.h"
#include "executor/kupl_executor.h"
#include "utils/config/kupl_config.h"

typedef struct kupl_barrier_dist {
    barrier_volatile_t *enter;
    barrier_volatile_t *leave;
} kupl_barrier_dist_t;

static int kupl_barrier_dist_create(kupl_barrier_h barrier)
{
    // alloc data
    auto bar = (kupl_barrier_dist_t *)kupl_malloc_inner(sizeof(kupl_barrier_dist_t));
    size_t flag_size;
    if (bar == nullptr) {
        goto err_alloc_bar;
    }
    flag_size = (size_t)barrier->max_size * sizeof(barrier_volatile_t);
    bar->enter = (barrier_volatile_t *)kupl_aligned_alloc(KUPL_BAR_CACHE_LINE, flag_size);
    if (kupl_unlikely(bar->enter == nullptr)) {
        goto err_alloc_enter;
    }
    bar->leave = (barrier_volatile_t *)kupl_aligned_alloc(KUPL_BAR_CACHE_LINE, flag_size);
    if (kupl_unlikely(bar->leave == nullptr)) {
        goto err_alloc_leave;
    }
    // init data
    for (int i = 0; i < barrier->max_size; i++) {
        bar->enter[i].b = KUPL_BAR_NOT_READY;
        bar->leave[i].b = KUPL_BAR_NOT_READY;
    }
    barrier->bar = bar;
    return KUPL_OK;

err_alloc_leave:
    kupl_safe_free(bar->enter);
err_alloc_enter:
    kupl_safe_free(bar);
err_alloc_bar:
    return KUPL_ERROR;
}

static void kupl_barrier_dist_destroy(kupl_barrier_h barrier)
{
    if (kupl_unlikely(barrier->bar == nullptr)) {
        return;
    }
    auto bar = (kupl_barrier_dist_t *)barrier->bar;
    kupl_safe_free(bar->leave);
    kupl_safe_free(bar->enter);
    kupl_safe_free(barrier->bar);
}

static void kupl_barrier_dist_wait(kupl_barrier_h barrier, int local_tid, int local_tnum)
{
    auto bar = (kupl_barrier_dist_t *)barrier->bar;

    int new_state = bar->enter[local_tid].b + KUPL_BARRIER_STATE_BUMP;
    KUPL_MB();
    if (KUPL_BAR_IS_MASTER(local_tid)) {
        for (int i = 1; i < local_tnum; i++) {
            while (bar->enter[i].b != new_state) {
                kupl_barrier_busy_wait(barrier, local_tid);
            }
        }
        KUPL_MB();
        bar->enter[local_tid].b = new_state;
    } else {
        bar->enter[local_tid].b += KUPL_BARRIER_STATE_BUMP;
    }

    KUPL_MB();

    /* other threads busy wait master thread */
    if (KUPL_BAR_IS_MASTER(local_tid)) {
        for (int i = 1; i < local_tnum; i++) {
            bar->leave[i].b = KUPL_BAR_READY_TO_GO;
        }
    } else {
        while (bar->leave[local_tid].b != KUPL_BAR_READY_TO_GO) {
            kupl_barrier_busy_wait(barrier, local_tid);
        }
        KUPL_MB();
        bar->leave[local_tid].b = KUPL_BAR_NOT_READY;
    }
}

static void kupl_barrier_dist_fork(kupl_barrier_h barrier, int local_tid, int local_tnum)
{
    auto bar = (kupl_barrier_dist_t *)barrier->bar;

    /* other threads busy wait master thread */
    if (KUPL_BAR_IS_MASTER(local_tid)) {
        for (int i = 1; i < local_tnum; i++) {
            bar->leave[i].b = KUPL_BAR_READY_TO_GO;
        }
    } else {
        while (bar->leave[local_tid].b != KUPL_BAR_READY_TO_GO) {
            kupl_barrier_busy_wait(barrier, local_tid);
        }
        KUPL_MB();
        bar->leave[local_tid].b = KUPL_BAR_NOT_READY;
    }
}

static void kupl_barrier_dist_join(kupl_barrier_h barrier, int local_tid, int local_tnum)
{
    auto bar = (kupl_barrier_dist_t *)barrier->bar;

    /* mather thread busy wait other threads */
    int new_state = bar->enter[local_tid].b + KUPL_BARRIER_STATE_BUMP;
    KUPL_MB();
    if (KUPL_BAR_IS_MASTER(local_tid)) {
        for (int i = 1; i < local_tnum; i++) {
            while (bar->enter[i].b != new_state) {
                kupl_barrier_busy_wait(barrier, local_tid);
            }
        }
        KUPL_MB();
        bar->enter[local_tid].b = new_state;
    } else {
        bar->enter[local_tid].b += KUPL_BARRIER_STATE_BUMP;
    }
}

static kupl_barrier_ops_t barrier_dist_ops = {
    .create = kupl_barrier_dist_create,
    .destroy = kupl_barrier_dist_destroy,
    .prepare = kupl_barrier_prepare_dummy,
    .wait = kupl_barrier_dist_wait,
    .fork = kupl_barrier_dist_fork,
    .join = kupl_barrier_dist_join,
};

void kupl_set_barrier_dist_ops()
{
    kupl_barrier_ops_set(KUPL_BARRIER_ALGO_DIST, &barrier_dist_ops);
}

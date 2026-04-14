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
#include <string>
#include "kupl.h"
#include "utils/config/kupl_config.h"
#include "executor/kupl_executor_group.h"

static kupl_barrier_ops_t g_barrier_ops[KUPL_BARRIER_ALGO_MAX];

void kupl_barrier_ops_set(kupl_barrier_algo_t algo, kupl_barrier_ops_t *ops)
{
    g_barrier_ops[algo] = *ops;
}

static kupl_barrier_algo_t kupl_barrier_algo_select()
{
    return KUPL_BARRIER_ALGO_DIST;
}

void kupl_barrier_init()
{
    kupl_set_barrier_dist_ops();
}

kupl_barrier_h kupl_barrier_create(kupl_barrier_algo_t algo)
{
    auto barrier = (kupl_barrier_t *)kupl_malloc_inner(sizeof(kupl_barrier_t));
    if (kupl_unlikely(barrier == nullptr)) {
        return nullptr;
    }

    barrier->graph = nullptr;
    barrier->size = 0;
    barrier->algo = kupl_barrier_algo_select();
    if (algo != KUPL_BARRIER_ALGO_DEFAULT) {
        barrier->algo = algo;
    }

    const kupl_host_info_t *info = kupl_get_host_info();
    barrier->max_size = info->pu_cnt;
    barrier->ops = &g_barrier_ops[barrier->algo];
    barrier->flag_idx = 0;
    int ret = barrier->ops->create(barrier);
    if (ret == KUPL_ERROR) {
        kupl_barrier_destroy(barrier);
        return nullptr;
    }
    return barrier;
}

void kupl_barrier_destroy(kupl_barrier_h barrier)
{
    if (kupl_unlikely(barrier == nullptr)) {
        return;
    }
    barrier->ops->destroy(barrier);
    kupl_safe_free(barrier);
}

void kupl_barrier_prepare(kupl_barrier_h barrier, kupl_graph_h graph, int num_threads)
{
    if (kupl_unlikely(barrier == nullptr)) {
        return;
    }

    if (kupl_unlikely(num_threads > barrier->max_size)) {
        kupl_error("num_threads %d is great than max_threads %d",
                   num_threads, barrier->max_size);
        return;
    }

    barrier->graph = graph;
    barrier->size = num_threads;
    barrier->ops->prepare(barrier);
}

int kupl_barrier_prepare_dummy(kupl_barrier_h barrier)
{
    (void)barrier;
    return KUPL_OK;
}

void kupl_barrier_fork(kupl_barrier_h barrier, int local_tid, int local_tnum)
{
    if (kupl_unlikely(barrier == nullptr)) {
        return;
    }

    KUPL_MB();
    barrier->ops->fork(barrier, local_tid, local_tnum);
}

void kupl_barrier_join(kupl_barrier_h barrier, int local_tid, int local_tnum)
{
    if (kupl_unlikely(barrier == nullptr)) {
        return;
    }

    KUPL_MB();
    barrier->ops->join(barrier, local_tid, local_tnum);
}

void kupl_barrier_wait(kupl_barrier_h barrier, int local_tid, int local_tnum)
{
    if (kupl_unlikely(barrier == nullptr)) {
        return;
    }

    KUPL_MB();
    barrier->ops->wait(barrier, local_tid, local_tnum);
}

// only use in process barrier
int kupl_init_bar(kupl_barrier_h barrier)
{
    barrier->bar = (void *)kupl_calloc(KUPL_SHM_BARRIER_FLAG_WIN_SIZE * (size_t)barrier->max_size, sizeof(int));
    if (barrier->bar == nullptr) {
        return KUPL_ERROR;
    }
    return KUPL_OK;
}

void kupl_destroy_bar(kupl_barrier_h barrier)
{
    kupl_safe_free(barrier->bar);
}

int kupl_barrier_change_size(kupl_barrier_h barrier, int size)
{
    if (kupl_unlikely(barrier == nullptr)) {
        return KUPL_ERROR;
    }
    barrier->size = size;
    return KUPL_OK;
}

void kupl_barrier_set_flag_idx(kupl_barrier_h barrier, int local_id, int flag_idx)
{
    (void)local_id;
    barrier->flag_idx = flag_idx;
}

namespace kupl {

    void FlagBarrier::notify(kupl_egroup_h egroup, int num_threads)
    {
        KUPL_MB();
        KUPL_FOR_EACH_LIMIT_EGROUP(egroup, num_threads, eid, eidx, {
            flags_[eid].vf = FlagBarrier::FLAG_READY;
        });
    }

    bool FlagBarrier::arrive(int geid)
    {
        bool ret = flags_[geid].vf == FlagBarrier::FLAG_READY;
        if (ret) {
            flags_[geid].vf = FlagBarrier::FLAG_ARRIVED;
        }
        return ret;
    }

    void FlagBarrier::leave(kupl_egroup_h egroup, int num_threads, int master_eid)
    {
        int geid = kupl_get_executor_num();
        if (master_eid == geid) {
            // master wait slave
            KUPL_FOR_EACH_LIMIT_EGROUP(egroup, num_threads, eid, eidx, {
                if ((int)eid != geid) {
                    while (FlagBarrier::FLAG_LEAVE != flags_[eid].vf);
                }
            });
        }
        KUPL_MB();
        flags_[geid].vf = FlagBarrier::FLAG_LEAVE;
    }

    void FlagBarrier::wait(kupl_egroup_h egroup, int num_threads)
    {
        uint32_t geid = static_cast<uint32_t>(kupl_get_executor_num());
        KUPL_FOR_EACH_LIMIT_EGROUP(egroup, num_threads, eid, eidx, {
            if (eid == geid) {
                auto sched = kupl_get_global_sched();
                kupl_sched_execute_tb(sched);
            }
            while (FlagBarrier::FLAG_LEAVE != flags_[eid].vf);
        });
        KUPL_MB();
    }
}
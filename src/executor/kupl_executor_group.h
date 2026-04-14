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
#ifndef KUPL_EXECUTOR_GROUP_H
#define KUPL_EXECUTOR_GROUP_H

#include <atomic>
#include <cstdint>
#include <sched.h>
#include <pthread.h>
#include "kupl.h"
#include "kupl_executor.h"
#include "utils/lock/kupl_lock.h"
#include "mt/barrier/kupl_barrier.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KUPL_EGROUP_ID_NULL UINT32_MAX

typedef struct kupl_egroup_info {
    uint32_t                size;                               // egroup size
    uint32_t                min_eid;                            // minimum executor id in egroup
    uint32_t                max_eid;                            // maximum executor id in egroup
    cpu_set_t               eid_set;                            // executor id set
    uint32_t                eid2lid[KUPL_EXECUTOR_ID_MAX];      // executor id to local id mapping
} kupl_egroup_info_t;

typedef struct kupl_egroup {
    uint32_t                next;           // next executor id index
    kupl_egroup_info_t      def;            // default egroup info
    kupl_egroup_info_t      cur;            // current egroup info
    kupl_lock_t             *lock;          // the lock
    kupl_barrier_h          barrier;        // the barrier
    int                     alloc_id;
} kupl_egroup_t;

uint32_t kupl_egroup_get_next(kupl_egroup_h group);

uint32_t kupl_egroup_get_cur_size(kupl_egroup_h group);

#define KUPL_FOR_EACH_EGROUP(group, eid, eidx, action...)   do {        \
        uint32_t min_eid = (group)->cur.min_eid;                        \
        uint32_t max_eid = (group)->cur.max_eid;                        \
        for (uint32_t eidx = 0, eid = min_eid; eid <= max_eid; eid++) { \
            if (CPU_ISSET(eid, &(group)->cur.eid_set)) {                \
                action                                                  \
                eidx++;                                                 \
            }                                                           \
        }                                                               \
    } while (0)

#define KUPL_FOR_EACH_LIMIT_EGROUP(group, limit, eid, eidx, action...)  do {        \
        uint32_t min_eid = (group)->cur.min_eid;                                    \
        int eidx = 0;                                                               \
        for (uint32_t eid = min_eid; eidx < limit; eid++) {                         \
            if (CPU_ISSET(eid, &(group)->cur.eid_set)) {                            \
                action                                                              \
                eidx++;                                                             \
            }                                                                       \
        }                                                                           \
    } while (0)

#define KUPL_FOR_EACH_EGROUP_REV(group, eid, eidx, action...)                       \
    uint32_t min_eid = (group)->cur.min_eid;                                        \
    uint32_t max_eid = (group)->cur.max_eid;                                        \
    uint32_t eidx = (group)->cur.size - 1;                                          \
    for (uint32_t eid = max_eid; (eid >= min_eid) && (eid != UINT32_MAX); eid--) {  \
        if (CPU_ISSET(eid, &(group)->cur.eid_set)) {                                \
            action                                                                  \
            eidx--;                                                                 \
        }                                                                           \
    }

static kupl_always_inline
cpu_set_t* kupl_egroup_get_cur_cpuset(kupl_egroup_h group)
{
    if (group == nullptr) {
        return nullptr;
    }
    return &group->cur.eid_set;
}

kupl_egroup_h kupl_get_current_egroup();

static kupl_always_inline
int kupl_egroup_master_eid(kupl_egroup_h egroup)
{
    return static_cast<int>(egroup->cur.min_eid);
}

#ifdef __cplusplus
}
#endif

#endif
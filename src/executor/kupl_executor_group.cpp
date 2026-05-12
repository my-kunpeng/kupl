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

#include "kupl_executor_group.h"
#include <cstring>
#include "core/kupl_core.h"
#include "executor/kupl_executor.h"
#include "executor/backend/kupl_executor_backend.h"
#include "utils/arch/kupl_atomic.h"
#include "mt/barrier/kupl_barrier.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/sys/kupl_math.h"

kupl_egroup_h kupl_egroup_create(int *executors, int executors_num)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return nullptr;
    }
    auto host_info = kupl_get_host_info();
    int num_executors = host_info->avail_pu_cnt;
    if (kupl_unlikely((executors_num > KUPL_EXECUTOR_ID_MAX) || (executors_num > num_executors) ||
                      (executors_num < 0))) {
        return nullptr;
    }
    if (kupl_unlikely((executors == nullptr) && (executors_num != 0))) {
        return nullptr;
    }
    int geid = kupl_get_executor_num();
    kupl_egroup_t *group = (kupl_egroup_t *)kupl_memory_calloc(sizeof(kupl_egroup_t), geid);
    if (kupl_unlikely(group == nullptr)) {
        return nullptr;
    }
    group->alloc_id = geid;
    kupl_egroup_info_t &def_info = group->def;
    def_info.size = 0;
    def_info.min_eid = KUPL_EXECUTOR_ID_MAX;
    def_info.max_eid = 0;
    CPU_ZERO(&def_info.eid_set);
    // set all lid to KUPL_EGROUP_LID_INVALID
    memset(&def_info.eid2lid, UINT8_MAX, sizeof(def_info.eid2lid));
    for (int i = 0; i < executors_num; i++) {
        if (kupl_unlikely(executors[i] >= num_executors || executors[i] < 0)) {
            goto err;
        }
        uint32_t eid = static_cast<uint32_t>(executors[i]);
        CPU_SET(eid, &def_info.eid_set);
        if (kupl_likely(def_info.eid2lid[eid] == KUPL_EGROUP_ID_NULL)) {
            def_info.eid2lid[eid] = static_cast<uint32_t>(i);
        } else {
            goto err;
        }
        if (eid < def_info.min_eid) {
            def_info.min_eid = eid;
        }
        if (eid > def_info.max_eid) {
            def_info.max_eid = eid;
        }
        def_info.size++;
    }
    memcpy(&group->cur, &group->def, sizeof(group->cur));
    group->next = 0;
    group->barrier = kupl_barrier_create(KUPL_BARRIER_ALGO_DIST);
    if (kupl_unlikely(group->barrier == nullptr)) {
        goto err;
    }
    kupl_barrier_prepare(group->barrier, nullptr, static_cast<int>(group->def.size));
    group->lock = kupl_lock_create(PTHREAD_SPINLOCK);
    if (kupl_unlikely(group->lock == nullptr)) {
        goto err;
    }
    return group;
err:
    kupl_egroup_destroy(group);
    return nullptr;
}

void kupl_egroup_destroy(kupl_egroup_h egroup)
{
    if (kupl_unlikely(egroup == nullptr)) {
        return;
    }
    kupl_lock_cleanup(egroup->lock);
    egroup->lock = nullptr;
    kupl_barrier_destroy(egroup->barrier);
    egroup->barrier = nullptr;
    kupl_memory_free(egroup, egroup->alloc_id);
}

uint32_t kupl_egroup_get_next(kupl_egroup_h group)
{
    if (kupl_unlikely((group == nullptr) || (group->cur.size == 0))) {
        return KUPL_EGROUP_ID_NULL;
    }
    auto lock = group->lock;
    lock->lock(lock);
    uint32_t next = group->next;
    group->next = (group->next + 1) % group->cur.size;
    lock->unlock(lock);
    uint32_t eid = group->cur.min_eid;
    for (; eid <= group->cur.max_eid; eid++) {
        if (CPU_ISSET(eid, &group->cur.eid_set)) {
            if (next-- == 0) {
                break;
            }
        }
    }
    return eid;
}

void kupl_egroup_reset(kupl_egroup_t *egroup)
{
    if (kupl_unlikely(egroup == nullptr)) {
        kupl_warn("invalid parameter");
        return;
    }
    memcpy(&egroup->cur, &egroup->def, sizeof(egroup->cur));
}

static kupl_always_inline int kupl_egroup_add(kupl_egroup_t *dest, kupl_egroup_t *src)
{
    kupl_egroup_info_t &dest_info = dest->cur;
    kupl_egroup_info_t &src_info = src->cur;
    if (src_info.size == 0) {
        return static_cast<int>(dest_info.size);
    }
    for (uint32_t eid = src_info.min_eid; eid <= src_info.max_eid; eid++) {
        if (src_info.eid2lid[eid] != KUPL_EGROUP_ID_NULL) {
            dest_info.eid2lid[eid] = src_info.eid2lid[eid] + dest_info.size;
            src_info.eid2lid[eid] = KUPL_EGROUP_ID_NULL;
        }
    }
    CPU_OR(&dest_info.eid_set, &dest_info.eid_set, &src_info.eid_set);
    dest_info.size += src_info.size;
    if (dest_info.min_eid > src_info.min_eid) {
        dest_info.min_eid = src_info.min_eid;
    }
    if (dest_info.max_eid < src_info.max_eid) {
        dest_info.max_eid = src_info.max_eid;
    }
    src_info.size = 0;
    src_info.min_eid = KUPL_EXECUTOR_ID_MAX;
    src_info.max_eid = 0;
    CPU_ZERO(&src_info.eid_set);
    return static_cast<int>(dest->cur.size);
}

uint32_t kupl_egroup_get_cur_size(kupl_egroup_h group)
{
    if (group->cur.size > KUPL_EXECUTOR_ID_MAX) {
        return 0;
    }
    return group->cur.size;
}

int kupl_egroup_borrow(kupl_egroup_h dest, kupl_egroup_h src)
{
    if (kupl_unlikely(dest == nullptr)) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(src == nullptr)) {
        return static_cast<int>(dest->cur.size);
    }
    auto lock = src->lock;
    lock->lock(lock);
    int ret = kupl_egroup_add(dest, src);
    kupl_barrier_change_size(dest->barrier, static_cast<int>(dest->cur.size));
    lock->unlock(lock);
    return ret;
}

int kupl_egroup_return(kupl_egroup_h dest, kupl_egroup_h src)
{
    if (kupl_unlikely(dest == nullptr)) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(src == nullptr)) {
        return static_cast<int>(dest->cur.size);
    }
    auto lock = dest->lock;
    lock->lock(lock);
    int ret = kupl_egroup_add(dest, src);
    kupl_barrier_change_size(dest->barrier, static_cast<int>(dest->cur.size));
    lock->unlock(lock);
    return ret;
}

static kupl_always_inline uint32_t kupl_egroup_get_local_id(kupl_egroup_h group)
{
    int eid = kupl_get_executor_num();
    if (kupl_unlikely(eid == KUPL_EXECUTOR_DEFAULT)) {
        return KUPL_EGROUP_ID_NULL;
    }
    return group->cur.eid2lid[eid];
}

kupl_egroup_h kupl_get_current_egroup()
{
    kupl_taskbase_t *current_tb = kupl_executor_get_current_tb();
    if (kupl_likely(current_tb != nullptr)) {
        return current_tb->egroup;
    }
    auto ult = kupl_executor_get_pf_ult();
    if (kupl_likely(ult != nullptr)) {
        return ult->tb.egroup;
    }
    return nullptr;
}

void kupl_egroup_barrier(kupl_egroup_h egroup)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return;
    }
    if (egroup == nullptr) {
        egroup = kupl_get_current_egroup();
    }
    if (kupl_unlikely(egroup == nullptr)) {
        kupl_error("fail to get current egroup");
        return;
    }
    if (egroup->cur.size <= 1) {
        return;
    }
    auto local_tid = kupl_egroup_get_local_id(egroup);
    auto local_tnum = egroup->cur.size;
    if (kupl_in_parallel()) {
        local_tnum = kupl_min(local_tnum, (uint32_t)kupl_get_kernel_concurrency());
    } else if (kupl_backend_type_get() == KUPL_BACKEND_PTHREAD) {
        return;
    }
    if (local_tid < local_tnum) {
        kupl_barrier_wait(egroup->barrier, static_cast<int>(local_tid), static_cast<int>(local_tnum));
    }
}

void kupl_egroup_fork_barrier(kupl_egroup_h egroup)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return;
    }
    if (egroup == nullptr) {
        egroup = kupl_get_current_egroup();
    }
    if (kupl_unlikely(egroup == nullptr)) {
        kupl_error("fail to get current egroup");
        return;
    }
    if (egroup->cur.size <= 1) {
        return;
    }
    auto local_tid = kupl_egroup_get_local_id(egroup);
    auto local_tnum = egroup->cur.size;
    if (local_tid < local_tnum) {
        kupl_barrier_fork(egroup->barrier, static_cast<int>(local_tid), static_cast<int>(local_tnum));
    }
}

void kupl_egroup_join_barrier(kupl_egroup_h egroup)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return;
    }
    if (egroup == nullptr) {
        egroup = kupl_get_current_egroup();
    }
    if (kupl_unlikely(egroup == nullptr)) {
        kupl_error("fail to get current egroup");
        return;
    }
    if (egroup->cur.size <= 1) {
        return;
    }
    auto local_tid = kupl_egroup_get_local_id(egroup);
    auto local_tnum = egroup->cur.size;
    if (local_tid < local_tnum) {
        kupl_barrier_join(egroup->barrier, static_cast<int>(local_tid), static_cast<int>(local_tnum));
    }
}
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
#include <string>
#include "kupl_fence.h"
#include "kupl.h"
#include "memory/shm/kupl_shmc.h"
#include "memory/shm/kupl_shm.h"
#include "utils/config/kupl_config.h"

constexpr int DOUBLING_STEP = 2;
constexpr int REUSE_STEP = 2;

int kupl_fence_rd_create(kupl_fence_h fence)
{
    fence->args.rd.loop_times = 0;
    int near_poweroftwo = 1;
    int tmp = fence->size;
    while (tmp > 1) {
        tmp /= DOUBLING_STEP;
        near_poweroftwo = near_poweroftwo << 1;
        fence->args.rd.loop_times++;
    }
    fence->args.rd.near_poweroftwo = near_poweroftwo;

    return KUPL_OK;
}

void kupl_fence_rd_destroy(kupl_fence_h fence)
{
    (void)fence;
}

void kupl_shm_peer_fence_internal(kupl_fence_h fence, int local_id, int remote_id)
{
    // change of flag of this process
    int base_id = fence->local_win->rank;
    int cur_flag_idx;

    cur_flag_idx = fence->flag_peer_idx[remote_id];
    kupl_memory_cpu_load_fence();
    if (base_id == local_id) {
        int this_val = 1 - kupl_fence_get_flag(fence, local_id, 0, remote_id, cur_flag_idx);
        kupl_fence_set_flag(fence, local_id, 0, this_val, remote_id, cur_flag_idx);
        // spy peer process
        if (kupl_fence_get_flag(fence, remote_id, 0, local_id, cur_flag_idx) == KUPL_ERROR) {
            return;
        }
        while (kupl_fence_get_flag(fence, remote_id, 0, local_id, cur_flag_idx) != this_val) {}
        // this sync is finished, update idx to use next flag
        kupl_memory_cpu_store_fence();
        fence->flag_peer_idx[remote_id] = (cur_flag_idx + 1) % KUPL_SHM_FENCE_FLAG_WIN_SIZE;
    }
    return;
}

void kupl_fence_rd_wait(kupl_fence_h fence, int local_id, int local_num)
{
    // local_num is flag's final status, once arrived, sync success
    int leap = 1;
    int this_flag_idx = fence->flag_idx;
    int near_poweroftwo = fence->args.rd.near_poweroftwo;

    // pre phase
    if (local_id >= local_num - ((local_num - near_poweroftwo) << 1)) {
        if (local_id < near_poweroftwo) {
            kupl_shm_peer_fence_internal(fence->local_win->peer_fence, local_id,
                                         local_id + (local_num - near_poweroftwo));
        } else {
            kupl_shm_peer_fence_internal(fence->local_win->peer_fence, local_id,
                                         local_id - (local_num - near_poweroftwo));
        }
    }

    // rd phase
    for (int i = 0; i < fence->args.rd.loop_times; i++) {
        int peer_idx = local_id / leap;
        if (peer_idx % DOUBLING_STEP == 0) {
            peer_idx = local_id + leap;
        } else {
            peer_idx = local_id - leap;
        }
        // update flag
        int this_flag_val = 1 + kupl_fence_get_flag(fence, local_id, local_num, 0, this_flag_idx);
        kupl_fence_set_flag(fence, local_id, local_num, this_flag_val, 0, this_flag_idx);
        // sync with peer
        if (peer_idx < fence->size) {
            while (kupl_fence_get_flag(fence, peer_idx, local_num, 0, this_flag_idx) < this_flag_val) {}
        }
        leap *= DOUBLING_STEP;
    }
    kupl_fence_set_flag(fence, local_id, local_num, 0, 0, (REUSE_STEP + this_flag_idx) % KUPL_SHM_FENCE_FLAG_WIN_SIZE);
    kupl_fence_set_flag_idx(fence, local_id, (1 + this_flag_idx) % KUPL_SHM_FENCE_FLAG_WIN_SIZE);

    // post phase
    if (local_id >= local_num - ((local_num - near_poweroftwo) << 1)) {
        if (local_id < near_poweroftwo) {
            kupl_shm_peer_fence_internal(fence->local_win->peer_fence, local_id,
                                         local_id + (local_num - near_poweroftwo));
        } else {
            kupl_shm_peer_fence_internal(fence->local_win->peer_fence, local_id,
                                         local_id - (local_num - near_poweroftwo));
        }
    }
}

static kupl_fence_ops_t fence_rd_ops = {
    .create = kupl_fence_rd_create,
    .destroy = kupl_fence_rd_destroy,
    .wait = kupl_fence_rd_wait,
};

void kupl_set_fence_rd_ops()
{
    kupl_fence_ops_set(KUPL_FENCE_ALGO_RD, &fence_rd_ops);
}
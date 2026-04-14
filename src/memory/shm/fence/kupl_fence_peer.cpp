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

#define KUPL_SHM_P2P_FENCE_MAX_SIZE      (2)

void kupl_fence_peer_destroy(kupl_fence_h fence)
{
    kupl_safe_free(fence->flag_peer_idx);
}

int kupl_fence_peer_create(kupl_fence_h fence)
{
    auto size = static_cast<size_t>(fence->size);
    fence->flag_peer_idx = static_cast<int*>(kupl_calloc(size, sizeof(int)));
    if (fence->flag_peer_idx == nullptr) {
        return KUPL_ERROR;
    }
    return KUPL_OK;
}

void kupl_fence_peer_wait(kupl_fence_h fence, int local_id, int remote_id)
{
    // change of flag of this process
    int base_id;
    int cur_flag_idx;
    base_id = fence->local_win->rank;
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

static kupl_fence_ops_t fence_peer_ops = {
    .create = kupl_fence_peer_create,
    .destroy = kupl_fence_peer_destroy,
    .wait = kupl_fence_peer_wait,
};

void kupl_set_fence_peer_ops()
{
    kupl_fence_ops_set(KUPL_FENCE_ALGO_P2P, &fence_peer_ops);
}
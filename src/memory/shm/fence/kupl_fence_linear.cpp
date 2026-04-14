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


int kupl_fence_linear_create(kupl_fence_h fence)
{
    (void)fence;
    return KUPL_OK;
}

void kupl_fence_linear_destroy(kupl_fence_h fence)
{
    (void)fence;
}

void kupl_fence_linear_wait(kupl_fence_h fence, int local_id, int local_num)
{
    // change of flag of this process/thread
    int this_flag_idx = fence->flag_idx;
    int this_flag_val = 1 - kupl_fence_get_flag(fence, local_id, local_num, 0, this_flag_idx);
    kupl_fence_set_flag(fence, local_id, local_num, this_flag_val, 0, this_flag_idx);

    // spy other processes
    for (int pi = 0; pi < local_num; pi++) {
        // get other flags
        while (kupl_fence_get_flag(fence, pi, local_num, 0, this_flag_idx) != this_flag_val) {}
    }

    // this sync is finished, update idx to use next flag
    kupl_fence_set_flag_idx(fence, local_id, 1 - this_flag_idx);
}

static kupl_fence_ops_t fence_linear_ops = {
    .create = kupl_fence_linear_create,
    .destroy = kupl_fence_linear_destroy,
    .wait = kupl_fence_linear_wait,
};

void kupl_set_fence_linear_ops()
{
    kupl_fence_ops_set(KUPL_FENCE_ALGO_LINEAR, &fence_linear_ops);
}

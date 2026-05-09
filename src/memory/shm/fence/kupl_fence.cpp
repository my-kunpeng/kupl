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
#include "kupl_fence.h"
#include <string>
#include "kupl.h"
#include "utils/config/kupl_config.h"

static kupl_fence_ops_t g_fence_ops[KUPL_FENCE_ALGO_MAX];

void kupl_fence_ops_set(kupl_fence_algo_t algo, kupl_fence_ops_t *ops)
{
    if (algo < 0 || algo >= KUPL_FENCE_ALGO_MAX) {
        kupl_error("kupl_fence_ops_set invalid fence algo");
        return;
    }
    g_fence_ops[algo] = *ops;
}

void kupl_fence_init()
{
    kupl_set_fence_linear_ops();
    kupl_set_fence_peer_ops();
    kupl_set_fence_rd_ops();
}

kupl_fence_h kupl_fence_create(kupl_fence_algo_t algo, kupl_shm_win_h local_win)
{
    auto fence = static_cast<kupl_fence_t *>(kupl_malloc_inner(sizeof(kupl_fence_t)));
    if (kupl_unlikely(fence == nullptr)) {
        return nullptr;
    }

    fence->algo = kupl_fence_algo_select();
    if (algo != KUPL_FENCE_ALGO_DEFAULT) {
        fence->algo = algo;
    }

    fence->local_win = local_win;
    fence->size = local_win->comm->size;

    fence->ops = &g_fence_ops[fence->algo];
    fence->flag_idx = 0;
    int ret = fence->ops->create(fence);
    if (ret == KUPL_ERROR) {
        kupl_fence_destroy(fence);
        return nullptr;
    }
    return fence;
}

void kupl_fence_destroy(kupl_fence_h fence)
{
    if (kupl_unlikely(fence == nullptr)) {
        return;
    }
    fence->ops->destroy(fence);
    kupl_safe_free(fence);
}

kupl_fence_algo_t kupl_fence_algo_select()
{
    kupl_fence_algo_t algo;
    std::string algo_str = kupl_config_get_value_str(KUPL_SHM_FENCE_ALGORITHM);
    if (algo_str == "1") {
        algo = KUPL_FENCE_ALGO_LINEAR;
    } else {
        algo = KUPL_FENCE_ALGO_RD;
    }
    return algo;
}

void kupl_fence_wait(kupl_fence_h fence, int local_tid, int local_tnum)
{
    if (kupl_unlikely(fence == nullptr)) {
        return;
    }

    fence->ops->wait(fence, local_tid, local_tnum);
}

void kupl_fence_set_flag_idx(kupl_fence_h fence, int local_id, int flag_idx)
{
    (void)local_id;
    fence->flag_idx = flag_idx;
}
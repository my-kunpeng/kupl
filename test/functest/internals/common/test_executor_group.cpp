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
#include <omp.h>
#include "gtest/gtest.h"
#include "kupl.h"
#include "utils/type/kupl_status.h"
#include "executor/kupl_executor_group.h"


TEST(test_executor_group_inner, kupl_egroup)
{
    int src_exe[4] = {0, 1, 2, 3};

    // executor group create
    auto src_eg = kupl_egroup_create(src_exe, 4);
    ASSERT_TRUE(src_eg != nullptr);

    // get next
    int next;
    for (int i = 0; i <= 4; i++) {
        next = kupl_egroup_get_next(src_eg);
        ASSERT_TRUE(next == (i % 4));
    }

    next = kupl_egroup_get_next(nullptr);
    ASSERT_TRUE(next == -1);

    // get current size
    int sz = kupl_egroup_get_cur_size(src_eg);
    ASSERT_TRUE(sz == 4);

    // get current cpuset
    auto cur_cpu = kupl_egroup_get_cur_cpuset(src_eg);
    ASSERT_TRUE(cur_cpu != nullptr);

    auto null_cpu = kupl_egroup_get_cur_cpuset(nullptr);
    ASSERT_TRUE(null_cpu == nullptr);

    // destroy
    kupl_egroup_destroy(src_eg);
}
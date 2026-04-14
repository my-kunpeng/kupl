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
#include "gtest/gtest.h"
#include "kupl.h"
#include "mt/kupl_parallel_for.h"

static void task_add(void *args, int tid, int tnum)
{
    KUPL_ATOMIC_INT *count = (KUPL_ATOMIC_INT *)args;
    int old_count = tid;
    int new_count = tid + 1;
    int geid = kupl_get_global_executor_id();
    ASSERT_TRUE(geid == tid);
    while (!KUPL_ARCH_ATOMIC_CAS_WEA(count, old_count, new_count)) {
        old_count = tid;
    }
}

TEST(test_parallel_inner, kupl_invoke_parallel)
{
    int tnum = kupl_get_num_executors();
    KUPL_ATOMIC_INT count = {0};
    kupl_invoke_parallel(task_add, &count);
    ASSERT_TRUE(count == tnum);
}

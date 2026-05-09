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

kupl_egroup_h src_eg, dest_eg, empty_eg, one_eg;
kupl_egroup_h src_eg_err, empty_eg_err;

TEST(test_executor_group, kupl_egroup_create)
{
    int src_exe[4] = {0, 1, 2, 3};
    int dest_exe[4] = {4, 5, 6, 7};
    int one_exe[1] = {0};

    // executor group create
    src_eg = kupl_egroup_create(src_exe, 4);
    ASSERT_TRUE(src_eg != nullptr);

    dest_eg = kupl_egroup_create(dest_exe, 4);
    ASSERT_TRUE(dest_eg != nullptr);

    empty_eg = kupl_egroup_create(nullptr, 0);
    ASSERT_TRUE(empty_eg != nullptr);

    one_eg = kupl_egroup_create(one_exe, 1);
    ASSERT_TRUE(one_eg != nullptr);
}

TEST(test_executor_group, kupl_egroup_create_err)
{
    int src_exe[4] = {0, 1, 2, 3};

    // executor group create
    src_eg_err = kupl_egroup_create(src_exe, 1025);
    ASSERT_TRUE(src_eg_err == nullptr);

    empty_eg_err = kupl_egroup_create(nullptr, 4);
    ASSERT_TRUE(empty_eg_err == nullptr);
}

TEST(test_executor_group, kupl_egroup_reset)
{
    // reset
    kupl_egroup_reset(src_eg);

    // reset empty
    kupl_egroup_reset(nullptr);
}

TEST(test_executor_group, kupl_egroup_borrow_return)
{
    // borrow and return
    int ret = kupl_egroup_borrow(nullptr, src_eg);
    ASSERT_TRUE(ret == KUPL_ERROR);

    ret = kupl_egroup_borrow(dest_eg, nullptr);
    ASSERT_TRUE(ret == 4);

    ret = kupl_egroup_borrow(dest_eg, empty_eg);
    ASSERT_TRUE(ret == 4);

    ret = kupl_egroup_return(nullptr, src_eg);
    ASSERT_TRUE(ret == KUPL_ERROR);

    ret = kupl_egroup_return(dest_eg, nullptr);
    ASSERT_TRUE(ret == 4);

    ret = kupl_egroup_borrow(dest_eg, src_eg);
    ASSERT_TRUE(ret == 8);

    ret = kupl_egroup_return(src_eg, dest_eg);
    ASSERT_TRUE(ret == 8);
}

TEST(test_executor_group, kupl_egroup_barrier)
{
    // barrier
    kupl_egroup_barrier(nullptr);

#if defined(_OPENMP)
    #pragma omp parallel num_threads(8)
    {
        kupl_egroup_barrier(src_eg);
    }
#endif
    kupl_egroup_barrier(one_eg);
}

TEST(test_executor_group, kupl_egroup_fork_join)
{
    // barrier
    kupl_egroup_join_barrier(nullptr);
    kupl_egroup_fork_barrier(nullptr);

#if defined(_OPENMP)
    #pragma omp parallel num_threads(8)
    {
        kupl_egroup_join_barrier(src_eg);
        kupl_egroup_fork_barrier(src_eg);
    }
#endif
    kupl_egroup_join_barrier(one_eg);
    kupl_egroup_fork_barrier(one_eg);
}

TEST(test_executor_group, kupl_egroup_destroy)
{
    // destroy
    kupl_egroup_destroy(src_eg);
    kupl_egroup_destroy(dest_eg);
    kupl_egroup_destroy(empty_eg);
    kupl_egroup_destroy(one_eg);
    kupl_egroup_destroy(src_eg_err);
    kupl_egroup_destroy(empty_eg_err);
}

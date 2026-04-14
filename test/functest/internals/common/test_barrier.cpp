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
#include "mt/barrier/kupl_barrier.h"

TEST(test_barrier_inner, kupl_barrier_default)
{
    auto barrier = kupl_barrier_create(KUPL_BARRIER_ALGO_DEFAULT);
    ASSERT_TRUE(barrier != nullptr);
    kupl_barrier_destroy(barrier);
    kupl_barrier_destroy(nullptr);
}

TEST(test_barrier_inner, kupl_barrier_dist)
{
    auto barrier = kupl_barrier_create(KUPL_BARRIER_ALGO_DIST);
    ASSERT_TRUE(barrier != nullptr);
    kupl_barrier_destroy(barrier);
}

TEST(test_barrier_inner, kupl_barrier_wait)
{
    kupl_barrier_wait(nullptr, 0, 0);
}

TEST(test_barrier_inner, kupl_barrier_prepare)
{
    kupl_barrier_prepare(nullptr, nullptr, 0);
}


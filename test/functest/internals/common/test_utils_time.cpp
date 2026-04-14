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
#include <unistd.h>
#include "gtest/gtest.h"
#include "kupl.h"
#include "utils/time/kupl_time.h"

TEST(test_utils_time_inner, kupl_now_ns)
{
    uint64_t bt = kupl_now_ns();
    usleep(1000);
    uint64_t ed = kupl_now_ns();
    ASSERT_TRUE((ed - bt) >= 1000000);
}

TEST(test_utils_time_inner, kupl_timestamp)
{
    char buffer[KUPL_TIME_BUF_SIZE];
    kupl_timestamp(nullptr, 0);
    kupl_timestamp(buffer, 0);
    kupl_timestamp(buffer, KUPL_TIME_BUF_SIZE);
}
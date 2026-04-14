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
#include "executor/kupl_executor.h"
#include "executor/backend/kupl_executor_backend.h"
#include "mt/scheduler/kupl_sched.h"

TEST(test_executor_inner, executor_err)
{
    int ret;

    int err_geid = 1023;
    kupl_set_global_executor_id(err_geid); // set geid to a err num

    ret = kupl_get_executor_num();
    ASSERT_TRUE(ret == -1);

    kupl_executor_t *ret_executor;
    ret_executor = kupl_executor_get_current_executor();
    ASSERT_TRUE(ret_executor == nullptr);

    kupl_executor_set_current_tb(nullptr);
    kupl_taskbase_t *ret_tb;
    ret_tb = kupl_executor_get_current_tb();
    ASSERT_TRUE(ret_tb == nullptr);

    kupl_executor_disable(err_geid);
    kupl_executor_enable(err_geid);

    kupl_set_global_executor_id(0);    // set geid back to 0
}

TEST(test_executor_inner, executor_normal)
{
    int master_core_id = kupl_executor_get_master_core_id();
    ASSERT_TRUE(master_core_id == 0);

    int executor_id = kupl_get_executor_num();
    ASSERT_TRUE(executor_id == 0);

    kupl_executor_disable(executor_id);
    kupl_executor_enable(executor_id);
}

TEST(test_executor_inner, executor_sched)
{
    kupl_sched_t *sched = kupl_sched_create(nullptr);
    int executor_id = kupl_get_executor_num();
    ASSERT_TRUE(executor_id == 0);

    // set sched failed
    kupl_executor_disable(executor_id);
    kupl_executor_enable(executor_id);

    kupl_sched_cleanup(sched);
}
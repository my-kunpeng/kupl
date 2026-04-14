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
#include "utils/type/kupl_status.h"
#include "executor/backend/kupl_executor_backend.h"
#include "mt/kupl_task.h"
#include "mt/kupl_dag.h"

static kupl_task_h g_task0, g_task1;

static void func_test(void *args)
{
}

TEST(test_dag_inner, kupl_gnode_init)
{
    kupl_task_desc_t task_desc = {
        .func = func_test,
        .args = nullptr,
    };
    kupl_tb_desc_t *user_desc = (kupl_tb_desc_t*)(&task_desc);
    kupl_task_param_t param = {
        .super = {
            .type       = KUPL_TB_TYPE_TASK,
            .user_desc  = user_desc,
            .graph      = nullptr,
            .count      = nullptr,
        },
        .kind           = KUPL_TASK_KIND_COMM_DYNAMIC,
        .inplace        = nullptr,
        .udata_size     = 0,
    };
    int geid = kupl_get_global_executor_id();
    g_task0 = kupl_task_init(&param, geid);
    ASSERT_TRUE(g_task0 != nullptr);
    g_task1 = kupl_task_init(&param, geid);
    ASSERT_TRUE(g_task1 != nullptr);
}

TEST(test_dag_inner, kupl_gnode_precede)
{
    int geid = kupl_get_global_executor_id();
    uint32_t num = kupl_gnode_precede(&g_task0->gnode, nullptr, geid);
    ASSERT_TRUE(num == 0);

    num = kupl_gnode_precede(&g_task0->gnode, &g_task1->gnode, geid);
    ASSERT_TRUE(num == 1);
}

TEST(test_dag_inner, kupl_gnode_release_ready)
{
    kupl_task_t *ready_tasks[1];
    int n_ready_tasks = kupl_gnode_release_ready(&g_task0->gnode, &ready_tasks[0]);
    ASSERT_TRUE(n_ready_tasks == 1);
}

TEST(test_dag_inner, kupl_gnode_cleanup)
{
    kupl_task_cleanup(g_task0);
    kupl_task_cleanup(g_task1);
}
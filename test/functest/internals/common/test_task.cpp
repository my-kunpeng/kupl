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

static kupl_task_h g_task;

static void func_test(void *args)
{
    printf("func test body.\n");
}

TEST(test_task_inner, kupl_task_create)
{
    g_task = kupl_task_create();
    ASSERT_TRUE(g_task != nullptr);
}

TEST(test_task_inner, kupl_task_init)
{
    kupl_task_desc_t task_desc = {
        .func = func_test,
        .args = nullptr,
    };
    kupl_tb_desc_t *user_desc = (kupl_tb_desc_t*)(&task_desc);
    kupl_task_param_t param = {
        .super = {
            .type           = KUPL_TB_TYPE_TASK,
            .user_desc      = user_desc,
            .graph          = nullptr,
            .count          = nullptr,
        },
        .kind               = KUPL_TASK_KIND_COMM_DYNAMIC,
        .inplace            = g_task,
        .udata_size         = 0
    };
    int geid = kupl_get_global_executor_id();
    g_task = kupl_task_init(&param, geid);
    ASSERT_TRUE(g_task != nullptr);
}

TEST(test_task_inner, task_ref_deref)
{
    int before_ref = g_task->tb.ref.load();
    g_task->tb.ops->ref(&g_task->tb);
    int after_ref = g_task->tb.ref.load();
    ASSERT_TRUE((before_ref + 1) == after_ref);

    int geid = kupl_get_global_executor_id();
    g_task->tb.ops->deref(&g_task->tb, geid);
    g_task->tb.ops->deref(&g_task->tb, geid);
}

TEST(test_task_inner, kupl_task_invoke)
{
    int ret = kupl_task_invoke(&g_task->tb);
    ASSERT_TRUE(ret == 1);
    ret = kupl_task_invoke(nullptr);
    ASSERT_TRUE(ret == 0);
}

TEST(test_task_inner, kupl_task_wait_err)
{
    int ret = kupl_task_wait(nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);
    kupl_task_kind_t before_kind = g_task->kind;
    g_task->kind = KUPL_TASK_KIND_UNKNOW;
    ret = kupl_task_wait(g_task);
    ASSERT_TRUE(ret == KUPL_ERROR);
    g_task->kind = before_kind;
}

TEST(test_task_inner, kupl_task_cleanup)
{
    g_task->tb.name = strdup("hello");
    kupl_task_cleanup(g_task);
    kupl_task_cleanup(nullptr);
}

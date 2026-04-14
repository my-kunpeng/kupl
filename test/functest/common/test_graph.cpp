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

static kupl_graph_h g_graph;

static void func_test(void *args)
{
}

TEST(test_graph, kupl_graph_create)
{
    g_graph = kupl_graph_create(nullptr);
    ASSERT_TRUE(g_graph != nullptr);
}

TEST(test_graph, kupl_graph_submit)
{
    kupl_task_desc_t desc = {
        .field_mask = KUPL_TASK_DESC_FIELD_NAME,
        .func = func_test,
        .args = nullptr,
        .name = "test_graph_submit",
    };
    kupl_task_desc_t invalid_desc = {
        .field_mask = KUPL_TASK_DESC_FIELD_NAME,
        .func = nullptr,
        .args = nullptr,
        .name = "test_graph_submit",
    };
    kupl_task_info_t info = {
        .type = KUPL_TASK_TYPE_SINGLE,
        .desc = &desc,
    };
    kupl_task_info_t invalid_info = {
        .type = KUPL_TASK_TYPE_SINGLE,
        .desc = &invalid_desc,
    };
    int ret = kupl_graph_submit(g_graph, &info);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_graph_submit(nullptr, &info);
    ASSERT_TRUE(ret == KUPL_ERROR);
    ret = kupl_graph_submit(g_graph, &invalid_info);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST(test_graph, kupl_graph_wait)
{
    kupl_graph_wait(g_graph);
    kupl_graph_wait(nullptr);
}

TEST(test_graph, kupl_graph_submit_lambda)
{
    kupl_task_desc_t desc = {
        .field_mask = KUPL_TASK_DESC_FIELD_NAME,
        .name = "test_lambda",
    };
    int ret = kupl::graph_submit(nullptr, &desc, [&]() {});
    ASSERT_TRUE(ret == KUPL_ERROR);

    ret = kupl::graph_submit(g_graph, &desc, nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);

    ret = kupl::graph_submit(g_graph, &desc, [&]() {});
    ASSERT_TRUE(ret == KUPL_OK);
}

TEST(test_graph, kupl_graph_wait_lambda)
{
    kupl_graph_wait(g_graph);
    kupl_graph_wait(nullptr);
}

TEST(test_graph, kupl_graph_destroy)
{
    kupl_graph_destroy(g_graph);
    kupl_graph_destroy(nullptr);
}
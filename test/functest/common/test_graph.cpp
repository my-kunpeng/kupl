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

static inline void task_str(void *args)
{
    const char *str = (const char *)args;
    printf("[thread:%lu] get string %s\n", pthread_self(), str);
}

TEST(test_graph, kupl_graph_submit_with_dep)
{
    kupl_graph_h graph = kupl_graph_create(nullptr);
    int a, b, c, d1, d2, f, all;
    kupl_task_dep_t a_dep[2] = {
        {&a, KUPL_TASK_DEP_TYPE_IN},
        {&b, KUPL_TASK_DEP_TYPE_OUT},
    };
    kupl_task_desc_t desc = {
        .field_mask = KUPL_TASK_DESC_FIELD_DEP,
        .func = task_str,
        .args = (void *)"A",
        .name = "A",
        .ndep = 2,
        .dep_list = a_dep,
    };
    kupl_task_info_t info = {
        .type = KUPL_TASK_TYPE_SINGLE,
        .desc = &desc,
    };

    int ret = kupl_graph_submit(graph, &info);
    ASSERT_TRUE(ret == KUPL_OK);

    kupl_task_dep_t b_dep[2] = {
        {&b, KUPL_TASK_DEP_TYPE_INOUT},
        {&c, KUPL_TASK_DEP_TYPE_OUT},
    };
    desc.args = (void *)"B";
    desc.name = "B";
    desc.dep_list = b_dep;
    ret = kupl_graph_submit(graph, &info);
    ASSERT_TRUE(ret == KUPL_OK);

    kupl_task_dep_t c1_dep[2] = {
        {&b, KUPL_TASK_DEP_TYPE_IN},
        {&d1, KUPL_TASK_DEP_TYPE_OUT},
    };
    desc.args = (void *)"C1";
    desc.name = "C1";
    desc.dep_list = c1_dep;
    ret = kupl_graph_submit(graph, &info);
    ASSERT_TRUE(ret == KUPL_OK);

    kupl_task_dep_t all_dep[1] = {
        {&all, KUPL_TASK_DEP_TYPE_ALL},
    };
    desc.args = (void *)"ALL";
    desc.name = "ALL";
    desc.dep_list = all_dep;
    desc.ndep = 1;
    ret = kupl_graph_submit(graph, &info);
    ASSERT_TRUE(ret == KUPL_OK);

    kupl_task_dep_t c2_dep[2] = {
        {&c, KUPL_TASK_DEP_TYPE_IN},
        {&d2, KUPL_TASK_DEP_TYPE_OUT},
    };
    desc.args = (void *)"C2";
    desc.name = "C2";
    desc.dep_list = c2_dep;
    desc.ndep = 2;
    ret = kupl_graph_submit(graph, &info);
    ASSERT_TRUE(ret == KUPL_OK);

    kupl_task_dep_t d_dep[3] = {
        {&d1, KUPL_TASK_DEP_TYPE_IN},
        {&d2, KUPL_TASK_DEP_TYPE_IN},
        {&f, KUPL_TASK_DEP_TYPE_OUT},
    };
    desc.args = (void *)"D";
    desc.name = "D";
    desc.dep_list = d_dep;
    desc.ndep = 3;
    ret = kupl_graph_submit(graph, &info);
    ASSERT_TRUE(ret == KUPL_OK);
    kupl_graph_wait(graph);

    desc.args = (void *)"ERR";
    desc.name = "ERR";
    desc.dep_list = nullptr;
    desc.ndep = 2;
    ret = kupl_graph_submit(graph, &info);
    ASSERT_TRUE(ret == KUPL_ERROR);

    desc.args = (void *)"ZERO";
    desc.name = "ZERO";
    desc.ndep = 0;
    ret = kupl_graph_submit(graph, &info);
    ASSERT_TRUE(ret == KUPL_OK);

    kupl_graph_wait(graph);
    kupl_graph_destroy(graph);
}

TEST(test_graph, kupl_graph_submit_with_dep_lambda)
{
    kupl_graph_h graph_lambda = kupl_graph_create(nullptr);
    int a, b, c, d1, d2, f, all;
    kupl_task_dep_t a_dep[2] = {
        {&a, KUPL_TASK_DEP_TYPE_IN},
        {&b, KUPL_TASK_DEP_TYPE_OUT},
    };
    kupl_task_desc_t desc = {
        .field_mask = KUPL_TASK_DESC_FIELD_DEP,
        .name = "A",
        .ndep = 2,
        .dep_list = a_dep,
    };

    int ret = kupl::graph_submit(graph_lambda, &desc, [&]() {
                                 printf("[thread:%lu] get lambda_string %s\n", pthread_self(), "A");
                                });
    ASSERT_TRUE(ret == KUPL_OK);

    kupl_task_dep_t b_dep[2] = {
        {&b, KUPL_TASK_DEP_TYPE_INOUT},
        {&c, KUPL_TASK_DEP_TYPE_OUT},
    };
    desc.name = "B";
    desc.dep_list = b_dep;
    ret = kupl::graph_submit(graph_lambda, &desc, [&]() {
                             printf("[thread:%lu] get lambda_string %s\n", pthread_self(), "B");
                            });
    ASSERT_TRUE(ret == KUPL_OK);

    kupl_task_dep_t c1_dep[2] = {
        {&b, KUPL_TASK_DEP_TYPE_IN},
        {&d1, KUPL_TASK_DEP_TYPE_OUT},
    };
    desc.name = "C1";
    desc.dep_list = c1_dep;
    ret = kupl::graph_submit(graph_lambda, &desc, [&]() {
                             printf("[thread:%lu] get lambda_string %s\n", pthread_self(), "C1");
                            });
    ASSERT_TRUE(ret == KUPL_OK);

    kupl_task_dep_t all_dep[1] = {
        {&all, KUPL_TASK_DEP_TYPE_ALL},
    };
    desc.name = "ALL";
    desc.dep_list = all_dep;
    desc.ndep = 1;
    ret = kupl::graph_submit(graph_lambda, &desc, [&]() {
                             printf("[thread:%lu] get lambda_string %s\n", pthread_self(), "ALL");
                            });
    ASSERT_TRUE(ret == KUPL_OK);

    kupl_task_dep_t c2_dep[2] = {
        {&c, KUPL_TASK_DEP_TYPE_IN},
        {&d2, KUPL_TASK_DEP_TYPE_OUT},
    };
    desc.name = "C2";
    desc.dep_list = c2_dep;
    desc.ndep = 2;
    ret = kupl::graph_submit(graph_lambda, &desc, [&]() {
                             printf("[thread:%lu] get lambda_string %s\n", pthread_self(), "C2");
                            });
    ASSERT_TRUE(ret == KUPL_OK);

    kupl_task_dep_t d_dep[3] = {
        {&d1, KUPL_TASK_DEP_TYPE_IN},
        {&d2, KUPL_TASK_DEP_TYPE_IN},
        {&f, KUPL_TASK_DEP_TYPE_OUT},
    };
    desc.name = "D";
    desc.dep_list = d_dep;
    desc.ndep = 3;
    ret = kupl::graph_submit(graph_lambda, &desc, [&]() {
                             printf("[thread:%lu] get lambda_string %s\n", pthread_self(), "D");
                            });
    ASSERT_TRUE(ret == KUPL_OK);
    kupl_graph_wait(graph_lambda);

    desc.name = "ERR";
    desc.dep_list = nullptr;
    desc.ndep = 2;
    ret = kupl::graph_submit(graph_lambda, &desc, [&]() {
                             printf("[thread:%lu] get lambda_string %s\n", pthread_self(), "ERR");
                            });
    ASSERT_TRUE(ret == KUPL_ERROR);

    desc.name = "ZERO";
    desc.ndep = 0;
    ret = kupl::graph_submit(graph_lambda, &desc, [&]() {
                             printf("[thread:%lu] get lambda_string %s\n", pthread_self(), "ZERO");
                            });
    ASSERT_TRUE(ret == KUPL_OK);

    kupl_graph_wait(graph_lambda);
    kupl_graph_destroy(graph_lambda);
}
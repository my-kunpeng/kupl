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

kupl_sgraph_h sgraph;
kupl_sgraph_node_h task_gnode_1;
kupl_sgraph_node_h task_gnode_2;
kupl_sgraph_node_h task_gnode_3;
kupl_sgraph_node_h task_gnode_4;
kupl_sgraph_node_h task_gnode_5;
kupl_sgraph_node_h task_gnode_6;
kupl_sgraph_node_h task_gnode_7;
kupl_sgraph_node_h task_gnode_8;
const char *lambda_text = "static e";

static inline void task_str(void *args)
{
    const char *str = (const char *)args;
    printf("[thread:%lu] get string %s\n", pthread_self(), str);
    usleep(10000);
}

static kupl_sgraph_node_h add_node(kupl_sgraph_h sgraph, const char *text)
{
    kupl_sgraph_node_desc_t node_desc = {
        .func = task_str,
        .args = (void *)text,
    };
    return kupl_sgraph_add_node(sgraph, &node_desc);
}

static kupl_sgraph_node_h add_node_lambda(kupl_sgraph_h sgraph)
{
    kupl_sgraph_node_desc_t node_desc = {
        .field_mask = KUPL_SGRAPH_NODE_DESC_FIELD_NAME,
        .name = "node_lambda",
    };
    kupl_sgraph_node_h node = kupl::sgraph_add_node(sgraph, &node_desc, [&]() {
                                  printf("[thread:%lu] get lambda_string %s\n", pthread_self(), lambda_text);
                                  usleep(10000);
                              });
    return node;
}

static kupl_sgraph_node_h add_node_null_lambda(kupl_sgraph_h sgraph)
{
    kupl_sgraph_node_desc_t node_desc = {
        .field_mask = KUPL_SGRAPH_NODE_DESC_FIELD_NAME,
        .name = "node_lambda",
    };
    kupl_sgraph_node_h node = kupl::sgraph_add_node(sgraph, &node_desc, nullptr);
    return node;
}

TEST(test_static_graph, kupl_sgraph_create) {
    sgraph = kupl_sgraph_create();
    ASSERT_TRUE(sgraph != nullptr);
}

TEST(test_static_graph, kupl_sgraph_add_node) {
    task_gnode_1 = add_node(sgraph, "static a");
    ASSERT_TRUE(task_gnode_1 != nullptr);
    task_gnode_2 = add_node(sgraph, "static b");
    ASSERT_TRUE(task_gnode_2 != nullptr);
    task_gnode_3 = add_node(sgraph, "static c");
    ASSERT_TRUE(task_gnode_3 != nullptr);
    task_gnode_4 = add_node(sgraph, "static d");
    ASSERT_TRUE(task_gnode_4 != nullptr);
    task_gnode_5 = add_node(nullptr, "static a");
    ASSERT_TRUE(task_gnode_5 == nullptr);
    task_gnode_6 = add_node_lambda(sgraph);
    ASSERT_TRUE(task_gnode_6 != nullptr);
    task_gnode_7 = add_node_lambda(nullptr);
    ASSERT_TRUE(task_gnode_7 == nullptr);
    task_gnode_8 = add_node_null_lambda(sgraph);
    ASSERT_TRUE(task_gnode_8 == nullptr);
}

TEST(test_static_graph, kupl_sgraph_add_dep) {
    int ret = KUPL_OK;
    ret = kupl_sgraph_add_dep(task_gnode_1, task_gnode_2);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_sgraph_add_dep(task_gnode_1, task_gnode_3);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_sgraph_add_dep(task_gnode_2, task_gnode_4);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_sgraph_add_dep(task_gnode_3, task_gnode_4);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_sgraph_add_dep(task_gnode_4, task_gnode_6);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_sgraph_add_dep(task_gnode_4, nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST(test_static_graph, kupl_graph_submit_sgraph_task)
{
    auto graph = kupl_graph_create(nullptr);
    kupl_sgraph_task_desc_t desc = {
        .field_mask = 0,
        .sgraph = sgraph
    };
    kupl_task_info_t info = {
        .type = KUPL_TASK_TYPE_SGRAPH,
        .desc = &desc,
    };
    kupl_task_info_t invalid_info = {
        .type = KUPL_TASK_TYPE_SGRAPH,
        .desc = nullptr,
    };
    int ret = kupl_graph_submit(graph, &info);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_graph_submit(graph, &invalid_info);
    ASSERT_TRUE(ret == KUPL_ERROR);
    kupl_graph_wait(graph);
    kupl_graph_destroy(graph);
}

TEST(test_static_graph, kupl_sgraph_destroy) {
    kupl_sgraph_destroy(sgraph);
    kupl_sgraph_destroy(nullptr);
}
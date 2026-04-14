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

static void node_func1(void *args)
{
    int eid = kupl_get_executor_num();
    ASSERT_TRUE(eid == 0);
}

static void node_func2(void *args)
{
    int eid = kupl_get_executor_num();
    ASSERT_TRUE(eid == 2);
}

static void node_func3(void *args)
{
    int eid = kupl_get_executor_num();
    ASSERT_TRUE(eid == 4);
}

TEST(test_static_graph, kupl_sgraph_node_egroup)
{
    auto sgraph = kupl_sgraph_create();
    auto graph = kupl_graph_create(nullptr);

    int exe1[4] = {0, 1, 2, 3};
    int exe2[4] = {2, 3, 4, 5};
    int exe3[4] = {4, 5, 6, 7};
    auto egroup1 = kupl_egroup_create(exe1, 4);
    auto egroup2 = kupl_egroup_create(exe2, 4);
    auto egroup3 = kupl_egroup_create(exe3, 4);

    kupl_sgraph_node_desc_t desc = {
        .field_mask = KUPL_SGRAPH_NODE_DESC_FIELD_EGROUP,
        .func = node_func1,
        .args = nullptr,
        .egroup = egroup1,
    };
    kupl_sgraph_add_node(sgraph, &desc);
    desc.func = node_func2;
    desc.egroup = egroup2;
    kupl_sgraph_add_node(sgraph, &desc);
    desc.func = node_func3;
    desc.egroup = egroup3;
    kupl_sgraph_add_node(sgraph, &desc);
    kupl_sgraph_task_desc_t sgraph_desc = {
        .field_mask = 0,
        .sgraph = sgraph
    };
    kupl_task_info_t info = {
        .type = KUPL_TASK_TYPE_SGRAPH,
        .desc = &sgraph_desc,
    };
    kupl_graph_submit(graph, &info);
    kupl_graph_wait(graph);

    kupl_egroup_destroy(egroup1);
    kupl_egroup_destroy(egroup2);
    kupl_egroup_destroy(egroup3);
    kupl_sgraph_destroy(sgraph);
    kupl_graph_destroy(graph);
}


TEST(test_static_graph, kupl_sgraph_node_egroup_nullptr)
{
    auto sgraph = kupl_sgraph_create();
    auto graph = kupl_graph_create(nullptr);

    kupl_sgraph_node_desc_t desc = {
        .field_mask = KUPL_SGRAPH_NODE_DESC_FIELD_EGROUP,
        .func = node_func1,
        .args = nullptr,
        .egroup = nullptr,
    };
    kupl_sgraph_add_node(sgraph, &desc);
    kupl_sgraph_task_desc_t sgraph_desc = {
        .field_mask = 0,
        .sgraph = sgraph
    };
    kupl_task_info_t info = {
        .type = KUPL_TASK_TYPE_SGRAPH,
        .desc = &sgraph_desc,
    };
    int ret = kupl_graph_submit(graph, &info);
    ASSERT_TRUE(ret == KUPL_OK);
    kupl_graph_wait(graph);

    kupl_sgraph_destroy(sgraph);
    kupl_graph_destroy(graph);
}


TEST(test_static_graph, kupl_sgraph_node_lambda_egroup)
{
    auto sgraph = kupl_sgraph_create();
    auto graph = kupl_graph_create(nullptr);

    int exe1[4] = {0, 1, 2, 3};
    int exe2[4] = {2, 3, 4, 5};
    int exe3[4] = {4, 5, 6, 7};
    auto egroup1 = kupl_egroup_create(exe1, 4);
    auto egroup2 = kupl_egroup_create(exe2, 4);
    auto egroup3 = kupl_egroup_create(exe3, 4);

    kupl_sgraph_node_desc_t desc = {
        .field_mask = KUPL_SGRAPH_NODE_DESC_FIELD_EGROUP,
        .egroup = egroup1,
    };
    kupl::sgraph_add_node(sgraph, &desc, []() {
        int eid = kupl_get_executor_num();
        ASSERT_TRUE(eid == 0);
    });
    desc.func = node_func2;
    desc.egroup = egroup2;
    kupl::sgraph_add_node(sgraph, &desc, []() {
        int eid = kupl_get_executor_num();
        ASSERT_TRUE(eid == 2);
    });
    desc.func = node_func3;
    desc.egroup = egroup3;
    kupl::sgraph_add_node(sgraph, &desc, []() {
        int eid = kupl_get_executor_num();
        ASSERT_TRUE(eid == 4);
    });
    kupl_sgraph_task_desc_t sgraph_desc = {
        .field_mask = 0,
        .sgraph = sgraph
    };
    kupl_task_info_t info = {
        .type = KUPL_TASK_TYPE_SGRAPH,
        .desc = &sgraph_desc,
    };
    kupl_graph_submit(graph, &info);
    kupl_graph_wait(graph);

    kupl_egroup_destroy(egroup1);
    kupl_egroup_destroy(egroup2);
    kupl_egroup_destroy(egroup3);
    kupl_sgraph_destroy(sgraph);
    kupl_graph_destroy(graph);
}

TEST(test_static_graph, kupl_sgraph_node_egroup_lambda_nullptr)
{
    auto sgraph = kupl_sgraph_create();
    auto graph = kupl_graph_create(nullptr);

    kupl_sgraph_node_desc_t desc = {
        .field_mask = KUPL_SGRAPH_NODE_DESC_FIELD_EGROUP,
        .egroup = nullptr,
    };
    kupl::sgraph_add_node(sgraph, &desc, []() {
        int eid = kupl_get_executor_num();
        ASSERT_TRUE(eid == 0);
    });
    kupl_sgraph_task_desc_t sgraph_desc = {
        .field_mask = 0,
        .sgraph = sgraph
    };
    kupl_task_info_t info = {
        .type = KUPL_TASK_TYPE_SGRAPH,
        .desc = &sgraph_desc,
    };
    int ret = kupl_graph_submit(graph, &info);
    ASSERT_TRUE(ret == KUPL_OK);
    kupl_graph_wait(graph);

    kupl_sgraph_destroy(sgraph);
    kupl_graph_destroy(graph);
}

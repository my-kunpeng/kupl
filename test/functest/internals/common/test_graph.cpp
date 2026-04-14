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
#include "executor/kupl_executor_group.h"
#include "executor/backend/kupl_executor_backend.h"
#include "mt/kupl_graph.h"
#include "mt/kupl_ult.h"

static kupl_graph_h g_graph, g_graph_emp;
static kupl_egroup_h g_egroup;

static void func_test(void *args)
{
}

TEST(test_graph_inner, kupl_graph_create)
{
    g_graph = kupl_graph_create(nullptr);
    ASSERT_TRUE(g_graph != nullptr);
    g_egroup = kupl_egroup_create(nullptr, 0);
    ASSERT_TRUE(g_egroup != nullptr);
    g_graph_emp = kupl_graph_create(g_egroup);
    ASSERT_TRUE(g_graph_emp == nullptr);
}

TEST(test_graph_inner, kupl_graph_destroy)
{
    kupl_graph_destroy(g_graph);
    kupl_graph_destroy(g_graph_emp);
    kupl_egroup_destroy(g_egroup);
    kupl_graph_destroy(nullptr);
}
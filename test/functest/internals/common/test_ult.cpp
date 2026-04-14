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
#include "mt/kupl_ult.h"

static kupl_ult_h g_ult;

static void func_test(void *args)
{
    printf("func test body.\n");
}

TEST(test_ult_inner, kupl_ult_create)
{
    g_ult = kupl_ult_create();
    ASSERT_TRUE(g_ult != nullptr);
}

TEST(test_ult_inner, kupl_ult_init)
{
    kupl_ult_desc_t ult_desc = {
        .func = func_test,
        .args = nullptr,
    };
    kupl_ult_param_t param = {
        .super = {
            .type           = KUPL_TB_TYPE_ULT,
            .user_desc      = &ult_desc,
            .graph          = nullptr,
            .count          = nullptr,
        },
        .kind               = KUPL_ULT_KIND_COMM_DYNAMIC,
        .inplace            = g_ult,
    };
    int geid = kupl_get_global_executor_id();
    g_ult = kupl_ult_init(&param, geid);
    ASSERT_TRUE(g_ult != nullptr);
}

TEST(test_ult_inner, ult_ref_deref)
{
    int before_ref = g_ult->tb.ref.load();
    g_ult->tb.ops->ref(nullptr);
    g_ult->tb.ops->ref(&g_ult->tb);
    int after_ref = g_ult->tb.ref.load();
    ASSERT_TRUE((before_ref + 1) == after_ref);

    int geid = kupl_get_global_executor_id();
    g_ult->tb.ops->deref(&g_ult->tb, geid);
    g_ult->tb.ops->deref(&g_ult->tb, geid);
    g_ult->tb.ops->deref(nullptr, geid);
}

TEST(test_ult_inner, kupl_ult_cleanup)
{
    g_ult->tb.name = strdup("hello");
    kupl_ult_cleanup(g_ult);
    kupl_ult_cleanup(nullptr);
}

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
#include "utils/type/kupl_status.h"
#include "mt/barrier/kupl_barrier.h"
#include "mt/kupl_ult.h"
#include "mt/scheduler/plugin/kupl_sched_plugin_load.h"
#include "mt/scheduler/plugin/kupl_sched_plugin_api.h"
#include "executor/backend/kupl_executor_backend.h"

static kupl_sched_plugin_api_t *g_plugin_api;
static kupl_sched_plugin_property_t *g_plugin_property;
static void *g_sched;
static kupl_ult_h g_ult;
static kupl_ult_h g_notify_ult;

static void func_test(void *args)
{
}

TEST(test_sched_inner, kupl_sched_create_destroy)
{
    // invalid schduler name
    auto sched = kupl_sched_create("invalid");
    ASSERT_TRUE(sched == nullptr);
}

// mq
TEST(test_sched_inner, kupl_sched_mq_plugin_find)
{
    g_plugin_api = (kupl_sched_plugin_api_t *)kupl_malloc_inner(sizeof(kupl_sched_plugin_api_t));
    g_plugin_property = (kupl_sched_plugin_property_t *)kupl_malloc_inner(sizeof(kupl_sched_plugin_property_t));
    int ret = kupl_sched_plugin_find("mq", g_plugin_api, g_plugin_property);
    ASSERT_TRUE(ret == KUPL_OK);
}

TEST(test_sched_inner, mq_plugin_create)
{
    g_sched = g_plugin_api->create();
    ASSERT_TRUE(g_sched != nullptr);
}

TEST(test_sched_inner, mq_plugin_add_tb)
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
        .inplace            = nullptr,
    };
    int geid = kupl_get_global_executor_id();
    g_ult = kupl_ult_init(&param, geid);
    ASSERT_TRUE(g_ult != nullptr);

    int ret = g_plugin_api->add_tb(g_sched, &g_ult->tb);
    ASSERT_TRUE(ret == KUPL_OK);
}

TEST(test_sched_inner, mq_plugin_get_tb)
{
    int gcid = kupl_get_global_core_id();
    kupl_compute_place_t cp = kupl_get_compute_place(gcid);
    auto ret_tb = g_plugin_api->get_tb(g_sched, cp);
    ASSERT_TRUE(ret_tb == &g_ult->tb);

    ret_tb = g_plugin_api->get_tb(g_sched, cp);
    ASSERT_TRUE(ret_tb == nullptr);

#if defined(_OPENMP)
    // add and get ult at slave thread
    #pragma omp parallel num_threads(2)
    {
        int tid = omp_get_thread_num();
        if (tid == 1) {
            int ret = g_plugin_api->add_tb(g_sched, &g_ult->tb);
            EXPECT_EQ(ret, KUPL_OK);
            int gcid_slave = kupl_get_global_core_id();
            kupl_compute_place_t cp_slave = kupl_get_compute_place(gcid_slave);
            ret_tb = g_plugin_api->get_tb(g_sched, cp_slave);
            EXPECT_EQ(ret_tb, &g_ult->tb);
        }
    }
#endif
}

TEST(test_sched_inner, mq_plugin_unnormal_tb)
{
    // add ult to place queue
    g_ult->tb.executor_id = KUPL_TB_EXECUTOR_ID_PLACE;
    int ret = g_plugin_api->add_tb(g_sched, &g_ult->tb);
    ASSERT_TRUE(ret == KUPL_OK);
    int gcid = kupl_get_global_core_id();
    kupl_compute_place_t cp = kupl_get_compute_place(gcid);
    auto ret_tb = g_plugin_api->get_tb(g_sched, cp);
    ASSERT_TRUE(ret_tb == &g_ult->tb);

    // add ult to specified queue
    int geid = kupl_get_global_executor_id();
    g_ult->tb.executor_id = geid;
    ret = g_plugin_api->add_tb(g_sched, &g_ult->tb);
    ASSERT_TRUE(ret == KUPL_OK);
    ret_tb = g_plugin_api->get_tb(g_sched, cp);
    ASSERT_TRUE(ret_tb == &g_ult->tb);

    // add ult to wrong queue
    g_ult->tb.executor_id = -3;
    ret = g_plugin_api->add_tb(g_sched, &g_ult->tb);
    ASSERT_TRUE(ret == KUPL_OK);
    ret_tb = g_plugin_api->get_tb(g_sched, cp);
    ASSERT_TRUE(ret_tb == &g_ult->tb);
}

TEST(test_sched_inner, mq_plugin_add_priority_tb)
{
    kupl_ult_cleanup(g_ult);
    kupl_ult_desc_t ult_desc = {
        .field_mask = KUPL_TB_DESC_FIELD_PRIORITY,
        .func = func_test,
        .args = nullptr,
        .priority = 1,
    };
    kupl_ult_param_t param = {
        .super = {
            .type       = KUPL_TB_TYPE_ULT,
            .user_desc  = &ult_desc,
            .graph      = nullptr,
            .count      = nullptr,
        },
        .kind           = KUPL_ULT_KIND_COMM_DYNAMIC,
        .inplace        = nullptr,
    };
    int geid = kupl_get_global_executor_id();
    g_ult = kupl_ult_init(&param, geid);
    ASSERT_TRUE(g_ult != nullptr);

    int ret = g_plugin_api->add_tb(g_sched, &g_ult->tb);
    ASSERT_TRUE(ret == KUPL_OK);
}

TEST(test_sched_inner, mq_plugin_get_priority_tb)
{
    int gcid = kupl_get_global_core_id();
    kupl_compute_place_t cp = kupl_get_compute_place(gcid);
    auto ret_tb = g_plugin_api->get_tb(g_sched, cp);
    ASSERT_TRUE(ret_tb == &g_ult->tb);

    ret_tb = g_plugin_api->get_tb(g_sched, cp);
    ASSERT_TRUE(ret_tb == nullptr);
}

TEST(test_sched_inner, mq_sched_cleanup)
{
    kupl_ult_cleanup(g_ult);
    g_plugin_api->cleanup(g_sched);
    g_plugin_api->cleanup(nullptr);
    kupl_free_inner(g_plugin_api);
    kupl_free_inner(g_plugin_property);
}

// hybrid
TEST(test_sched_inner, kupl_sched_hybrid_plugin_find)
{
    g_plugin_api = (kupl_sched_plugin_api_t *)kupl_malloc_inner(sizeof(kupl_sched_plugin_api_t));
    g_plugin_property = (kupl_sched_plugin_property_t *)kupl_malloc_inner(sizeof(kupl_sched_plugin_property_t));
    int ret = kupl_sched_plugin_find("hybrid", g_plugin_api, g_plugin_property);
    ASSERT_TRUE(ret == KUPL_OK);
}

TEST(test_sched_inner, hybrid_plugin_create)
{
    g_sched = g_plugin_api->create();
    ASSERT_TRUE(g_sched != nullptr);
}

TEST(test_sched_inner, hybrid_plugin_add_inner_tb)
{
    kupl_ult_cleanup(g_ult);
    kupl_ult_desc_t ult_desc = {
        .field_mask = KUPL_TB_DESC_FIELD_FLAG,
        .func = func_test,
        .args = nullptr,
        .flag = KUPL_TB_FLAG_HYBRID_INNER,
    };
    kupl_ult_param_t param = {
        .super = {
            .type           = KUPL_TB_TYPE_ULT,
            .user_desc      = &ult_desc,
            .graph          = nullptr,
            .count          = nullptr,
        },
        .kind               = KUPL_ULT_KIND_COMM_DYNAMIC,
        .inplace            = nullptr,
    };
    int geid = kupl_get_global_executor_id();
    g_ult = kupl_ult_init(&param, geid);
    ASSERT_TRUE(g_ult != nullptr);

    int ret = g_plugin_api->add_tb(g_sched, &g_ult->tb);
    ASSERT_TRUE(ret == KUPL_OK);
}

TEST(test_sched_inner, hybrid_plugin_get_inner_tb)
{
    int gcid = kupl_get_global_core_id();
    kupl_compute_place_t cp = kupl_get_compute_place(gcid);
    auto ret_tb = g_plugin_api->get_tb(g_sched, cp);
    ASSERT_TRUE(ret_tb == &g_ult->tb);

    ret_tb = g_plugin_api->get_tb(g_sched, cp);
    ASSERT_TRUE(ret_tb == nullptr);
}

TEST(test_sched_inner, hybrid_plugin_add_outer_tb)
{
    kupl_ult_cleanup(g_ult);
    kupl_ult_desc_t ult_desc = {
        .field_mask = KUPL_TB_DESC_FIELD_FLAG | KUPL_TB_DESC_FIELD_EXECUTOR_ID,
        .func = func_test,
        .args = nullptr,
        .flag = KUPL_TB_FLAG_HYBRID_OUTER,
        .executor_id = 0,
    };
    kupl_ult_param_t param = {
        .super = {
            .type           = KUPL_TB_TYPE_ULT,
            .user_desc      = &ult_desc,
            .graph          = nullptr,
            .count          = nullptr,
        },
        .kind               = KUPL_ULT_KIND_COMM_DYNAMIC,
        .inplace            = nullptr,
    };
    int geid = kupl_get_global_executor_id();
    g_ult = kupl_ult_init(&param, geid);
    ASSERT_TRUE(g_ult != nullptr);

    int ret = g_plugin_api->add_tb(g_sched, &g_ult->tb);
    ASSERT_TRUE(ret == KUPL_OK);

    g_ult->tb.executor_id = -1;
    ret = g_plugin_api->add_tb(g_sched, &g_ult->tb);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST(test_sched_inner, hybrid_plugin_get_outer_tb)
{
    int gcid = kupl_get_global_core_id();
    kupl_compute_place_t cp = kupl_get_compute_place(gcid);
    auto ret_tb = g_plugin_api->get_tb(g_sched, cp);
    ASSERT_TRUE(ret_tb == nullptr);

    int exe[1] = {0};
    auto egroup = kupl_egroup_create(exe, 1);
    auto &barrier = kupl::FlagBarrier::getInstance();
    barrier.notify(egroup, 1);
    ret_tb = g_plugin_api->get_tb(g_sched, cp);
    ASSERT_TRUE(ret_tb == &g_ult->tb);
    kupl_egroup_destroy(egroup);
}

TEST(test_sched_inner, hybrid_sched_cleanup)
{
    kupl_ult_cleanup(g_ult);
    g_plugin_api->cleanup(g_sched);
    g_plugin_api->cleanup(nullptr);
    kupl_free_inner(g_plugin_api);
    kupl_free_inner(g_plugin_property);
}

// sspe
TEST(test_sched_inner, kupl_sched_sspe_plugin_find)
{
    g_plugin_api = (kupl_sched_plugin_api_t *)kupl_malloc_inner(sizeof(kupl_sched_plugin_api_t));
    g_plugin_property = (kupl_sched_plugin_property_t *)kupl_malloc_inner(sizeof(kupl_sched_plugin_property_t));
    int ret = kupl_sched_plugin_find("sspe", g_plugin_api, g_plugin_property);
    ASSERT_TRUE(ret == KUPL_OK);
}

TEST(test_sched_inner, sspe_plugin_create)
{
    g_sched = g_plugin_api->create();
    ASSERT_TRUE(g_sched != nullptr);
}

TEST(test_sched_inner, sspe_plugin_add_tb)
{
    kupl_ult_desc_t ult_desc = {
        .field_mask = KUPL_TB_DESC_FIELD_EXECUTOR_ID,
        .func = func_test,
        .args = nullptr,
        .executor_id = 0,
    };
    kupl_ult_param_t param = {
        .super = {
            .type           = KUPL_TB_TYPE_ULT,
            .user_desc      = &ult_desc,
            .graph          = nullptr,
            .count          = nullptr,
        },
        .kind               = KUPL_ULT_KIND_COMM_DYNAMIC,
        .inplace            = nullptr,
    };
    int geid = kupl_get_global_executor_id();
    g_ult = kupl_ult_init(&param, geid);
    ASSERT_TRUE(g_ult != nullptr);

    int ret = g_plugin_api->add_tb(g_sched, &g_ult->tb);
    ASSERT_TRUE(ret == KUPL_OK);

    g_ult->tb.executor_id = -1;
    ret = g_plugin_api->add_tb(g_sched, &g_ult->tb);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST(test_sched_inner, sspe_plugin_get_tb)
{
    int gcid = kupl_get_global_core_id();
    kupl_compute_place_t cp = kupl_get_compute_place(gcid);
    auto ret_tb = g_plugin_api->get_tb(g_sched, cp);
    ASSERT_TRUE(ret_tb == nullptr);

    int exe[1] = {0};
    auto egroup = kupl_egroup_create(exe, 1);
    auto &barrier = kupl::FlagBarrier::getInstance();
    barrier.notify(egroup, 1);
    ret_tb = g_plugin_api->get_tb(g_sched, cp);
    ASSERT_TRUE(ret_tb == &g_ult->tb);
    barrier.leave(egroup, 1, 0);
    kupl_egroup_destroy(egroup);
}

TEST(test_sched_inner, sspe_sched_cleanup)
{
    kupl_ult_cleanup(g_ult);
    g_plugin_api->cleanup(g_sched);
    g_plugin_api->cleanup(nullptr);
    kupl_free_inner(g_plugin_api);
    kupl_free_inner(g_plugin_property);
}
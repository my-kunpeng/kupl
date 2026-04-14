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
#include "executor/backend/omp/kupl_omp.h"
#include "executor/backend/pthread/kupl_pthread.h"
#include "executor/kupl_executor.h"

static kupl_executor_ops_t *g_ops;

// global
TEST(test_executor_backend_inner, kupl_set_executor_core_mapping)
{
    auto ret = kupl_set_executor_core_mapping();
    ASSERT_TRUE(ret == KUPL_OK);
}

TEST(test_executor_backend_inner, kupl_get_self_affinity)
{
    auto core_id1 = kupl_get_self_affinity();
    ASSERT_TRUE(core_id1 != KUPL_EIDCID_INIT);
    auto core_id2 = kupl_get_self_affinity();
    ASSERT_TRUE(core_id2 == core_id1);
}

TEST(test_executor_backend_inner, kupl_global_eid2cid)
{
    int ret = kupl_global_eid2cid(1);
    ASSERT_TRUE(ret == 1);
}

TEST(test_executor_backend_inner, kupl_global_cid2eid)
{
    int ret = kupl_global_cid2eid(1);
    ASSERT_TRUE(ret == 1);
}

TEST(test_executor_backend_inner, kupl_set_global_executor_id)
{
    int ret = kupl_set_global_executor_id(1);
    ASSERT_TRUE(ret == KUPL_OK);
}

TEST(test_executor_backend_inner, kupl_get_global_executor_id)
{
    int ret = kupl_get_global_executor_id();
    ASSERT_TRUE(ret == 1);
    // set geid back to 0
    ret = kupl_set_global_executor_id(0);
    ASSERT_TRUE(ret == KUPL_OK);
}

TEST(test_executor_backend_inner, kupl_backend_init_fini)
{
    int ret = kupl_backend_init(nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);

    kupl_backend_fini(nullptr);

    kupl_executor_base_t exec;
    ret = kupl_backend_init(&exec);
    ASSERT_TRUE(ret == KUPL_OK);

    exec.stop = true;
    kupl_backend_fini(&exec);
}

TEST(test_executor_backend_inner, kupl_backend_type_set_invalid)
{
    std::string backend_type_str = "invali_backend";
    kupl_backend_type_set(backend_type_str);
    // have to set it back to valid type
    kupl_backend_type_select();
}
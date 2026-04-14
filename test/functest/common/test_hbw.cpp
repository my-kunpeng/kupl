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

TEST(test_hbw, kupl_hbw_check_available)
{
    ASSERT_EQ(kupl_hbw_check_available(), 1);
}

TEST(test_hbw, kupl_hbw_get_policy)
{
    ASSERT_EQ(kupl_hbw_get_policy(), KUPL_HBW_POLICY_BIND);
}

TEST(test_hbw, kupl_hbw_set_policy)
{
    ASSERT_EQ(kupl_hbw_get_policy(), KUPL_HBW_POLICY_BIND);
    int ret = kupl_hbw_set_policy(KUPL_HBW_POLICY_BIND);
    ASSERT_EQ(ret, KUPL_OK);
    ASSERT_EQ(kupl_hbw_get_policy(), KUPL_HBW_POLICY_BIND);
}

TEST(test_hbw, kupl_hbw_set_policy_err)
{
    int ret = kupl_hbw_set_policy((kupl_hbw_policy_t)10);
    ASSERT_EQ(ret, KUPL_ERROR);
}

TEST(test_hbw, kupl_hbw_malloc)
{
    char *ptr = (char*)kupl_hbw_malloc(1024);
    ASSERT_TRUE(ptr != nullptr);
    ptr[0] = 'a';
    ptr[1023] = 'b';
    kupl_hbw_free(ptr);
}

TEST(test_hbw, kupl_hbw_malloc_err)
{
    void* ptr = nullptr;
    ptr = kupl_hbw_malloc(0);
    ASSERT_TRUE(ptr == nullptr);
    kupl_hbw_free(ptr);
}

TEST(test_hbw, kupl_hbw_verify_flag)
{
    int result;
    void* ptr_ddr = kupl_malloc(KUPL_MEM_LARGE_CAP, 1024);
    result = kupl_hbw_verify(ptr_ddr, 1024, KUPL_HBW_TOUCH_PAGES);
    ASSERT_EQ(result, KUPL_IS_NOT_HBW_MEMORY);

    void* ptr_hbw = kupl_hbw_malloc(1024);
    result = kupl_hbw_verify(ptr_hbw, 1024, KUPL_HBW_TOUCH_PAGES);
    ASSERT_EQ(result, KUPL_IS_HBW_MEMORY);

    result = kupl_hbw_verify(nullptr, 1024, KUPL_HBW_TOUCH_PAGES);
    ASSERT_EQ(result, KUPL_HBW_VERIFY_ERROR);

    result = kupl_hbw_verify(ptr_hbw, 0, KUPL_HBW_TOUCH_PAGES);
    ASSERT_EQ(result, KUPL_HBW_VERIFY_ERROR);

    result = kupl_hbw_verify(ptr_hbw, 1024, (1 << 2));
    ASSERT_EQ(result, KUPL_HBW_VERIFY_ERROR);

    kupl_free(KUPL_MEM_LARGE_CAP, ptr_ddr);
    kupl_hbw_free(ptr_hbw);
}

TEST(test_hbw, kupl_hbw_verify_without_flag)
{
    int result;
    char* ptr_ddr = (char*)kupl_malloc(KUPL_MEM_LARGE_CAP, 1024);
    ptr_ddr[0] = 'a';
    result = kupl_hbw_verify(ptr_ddr, 1024, 0);
    ASSERT_EQ(result, KUPL_IS_NOT_HBW_MEMORY);

    char* ptr_hbw = (char*)kupl_hbw_malloc(1024);
    ptr_hbw[0] = 'a';
    result = kupl_hbw_verify(ptr_hbw, 1024, 0);
    ASSERT_EQ(result, KUPL_IS_HBW_MEMORY);

    result = kupl_hbw_verify(nullptr, 1024, 0);
    ASSERT_EQ(result, KUPL_HBW_VERIFY_ERROR);

    result = kupl_hbw_verify(ptr_hbw, 0, 0);
    ASSERT_EQ(result, KUPL_HBW_VERIFY_ERROR);

    kupl_free(KUPL_MEM_LARGE_CAP, ptr_ddr);
    kupl_hbw_free(ptr_hbw);
}

TEST(test_hbw, kupl_hbw_err)
{
    void* ptr = nullptr;
    ptr = kupl_hbw_malloc((1uLL << 32) + 1);
    ASSERT_TRUE(ptr == nullptr);
    kupl_hbw_free(ptr);

    char* ptr_hbw = (char*)kupl_hbw_malloc(1024);
    int result = kupl_hbw_verify(ptr_hbw, (1uLL << 32) + 1, KUPL_HBW_TOUCH_PAGES);
    ASSERT_EQ(result, KUPL_HBW_VERIFY_ERROR);
    kupl_hbw_free(ptr_hbw);
}
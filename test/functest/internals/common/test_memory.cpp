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
#include "executor/kupl_executor.h"
#include "memory/mpool/kupl_mpool.h"
#include "dm/memcpy/kupl_memcpy.h"

TEST(test_memory_inner, kupl_kernel_concurrency)
{
    int num_executors = kupl_get_num_executors();
    kupl_set_kernel_concurrency(num_executors);
    ASSERT_EQ(kupl_get_kernel_concurrency_inner(), num_executors);
}

TEST(test_memory_inner, kupl_kernel_concurrency_local)
{
    int num_executors = kupl_get_num_executors();
    kupl_set_kernel_concurrency_local(num_executors);
    ASSERT_EQ(kupl_get_kernel_concurrency_local(), num_executors);
}

void kupl_mlock_boundary_test(size_t sz)
{
    void *buffer = malloc(sz + 1);
    ASSERT_TRUE(buffer != nullptr);
    int ret = kupl_mlock(buffer, sz);
    if (kupl_get_sdma_mpool_func_init()) {
        ASSERT_TRUE(ret == KUPL_OK);
    }
    ret = kupl_mlock(buffer, sz - 1);
    if (kupl_get_sdma_mpool_func_init()) {
        ASSERT_TRUE(ret == KUPL_OK);
    }
    ret = kupl_mlock(buffer, sz + 1);
    if (kupl_get_sdma_mpool_func_init()) {
        ASSERT_TRUE(ret == KUPL_OK);
    }
    ret = kupl_munlock(buffer, sz);
    if (kupl_get_sdma_mpool_func_init()) {
        ASSERT_TRUE(ret == KUPL_ERROR);
    }
    ret = kupl_munlock(buffer, sz + 1);
    if (kupl_get_sdma_mpool_func_init()) {
        ASSERT_TRUE(ret == KUPL_OK);
    }
    free(buffer);
}

void kupl_mlock_twice(size_t sz)
{
    void *buffer = malloc(sz);
    ASSERT_TRUE(buffer != nullptr);
    int ret = kupl_mlock(buffer, sz);
    if (kupl_get_sdma_mpool_func_init()) {
        ASSERT_TRUE(ret == KUPL_OK);
    }
    ret = kupl_mlock(buffer, sz);
    if (kupl_get_sdma_mpool_func_init()) {
        ASSERT_TRUE(ret == KUPL_OK);
    }
    kupl_munlock(buffer, sz);
    free(buffer);
}

void kupl_malloc_mlock(size_t sz)
{
    void *buffer = kupl_malloc(KUPL_MEM_DEFAULT, sz);
    ASSERT_TRUE(buffer != nullptr);
    int ret = kupl_mlock(buffer, sz);
    if (kupl_get_sdma_mpool_func_init()) {
        ASSERT_TRUE(ret == KUPL_OK);
    }
    kupl_munlock(buffer, sz);
    kupl_free(KUPL_MEM_DEFAULT, buffer);
}

TEST(test_memory_inner, kupl_mlock)
{
    kupl_mlock_twice(1024);
    kupl_malloc_mlock(1024);

    kupl_mlock_boundary_test(512);
}

void kupl_munlock_twice(size_t sz)
{
    void *buffer = malloc(sz);
    ASSERT_TRUE(buffer != nullptr);
    kupl_mlock(buffer, sz);
    int ret = kupl_munlock(buffer, sz);
    if (kupl_get_sdma_mpool_func_init()) {
        ASSERT_TRUE(ret == KUPL_OK);
    }
    ret = kupl_munlock(buffer, sz);
    if (kupl_get_sdma_mpool_func_init()) {
        ASSERT_TRUE(ret == KUPL_ERROR);
    }
    free(buffer);
}

void kupl_munlock_free(size_t sz)
{
    void *buffer = kupl_malloc(KUPL_MEM_DEFAULT, sz);
    ASSERT_TRUE(buffer != nullptr);
    kupl_mlock(buffer, sz);
    int ret = kupl_munlock(buffer, sz);
    if (kupl_get_sdma_mpool_func_init()) {
        ASSERT_TRUE(ret == KUPL_OK);
    }
    kupl_free(KUPL_MEM_DEFAULT, buffer);
}

TEST(test_memory_inner, kupl_munlock)
{
    kupl_munlock_twice(1024);
    kupl_munlock_free(1024);
}

TEST(test_memory_inner, kupl_memory_pool)
{
    bool res = kupl_memory_is_inited();
    ASSERT_TRUE(res);

    int geid0 = 0;
    int geid1 = 1;
    int geid2 = 2;

    // allocate err memory
    size_t sz_err = 0;
    void *buffer_err = kupl_memory_calloc(sz_err, geid0);
    ASSERT_TRUE(buffer_err == nullptr);

    // allocate large memory which kupl will use malloc directly
    size_t sz_large = 8388608;          // 8 * 1024 * 1024
    void *buffer_large = kupl_memory_calloc(sz_large, geid0);
    ASSERT_TRUE(buffer_large != nullptr);
    kupl_memory_free(buffer_large, geid0);

    // allocate fast memory
    size_t sz_fast = 1024;
    void *buffer_fast0 = kupl_memory_calloc(sz_fast, geid0);
    ASSERT_TRUE(buffer_fast0 != nullptr);
    kupl_memory_free(buffer_fast0, geid2);     // free geid0's fast memory it to geid2's memory pool
    void *buffer_fast1 = kupl_memory_calloc(sz_fast, geid1);
    kupl_memory_free(buffer_fast1, geid2);     // test geid0's fast memory will back to itself

    // allocate normal memory
    size_t sz_normal = 45744;
    void *buffer_normal = kupl_memory_calloc(sz_normal, geid0);
    ASSERT_TRUE(buffer_normal != nullptr);
    int cnt = 82;
    void *buffer_normal_vec[cnt];       // test the normal memory not cutting logic
    for (int i = 0; i < cnt; i++) {
        buffer_normal_vec[i] = kupl_memory_calloc(sz_normal, geid0);
    }
    kupl_memory_free(buffer_normal, geid0);
    for (int i = 0; i < cnt; i++) {
        kupl_memory_free(buffer_normal_vec[i], geid0);
    }
    buffer_normal = kupl_memory_calloc(sz_normal, geid0);
    kupl_memory_free(buffer_normal, geid2);    // free geid0's normal memory to geid2's memory pool
}

TEST(test_memory_inner, kupl_memory_err)
{
    kupl_sdma_wait_event(nullptr);
    kupl_sdma_query_event(nullptr);
    kupl_set_kernel_concurrency(0);
    kupl_set_kernel_concurrency_local(0);
}
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
#include <unistd.h>
#include <atomic>
#include "gtest/gtest.h"
#include "kupl.h"

class test_queue_submit : public testing::Test {
public:
    static void SetUpTestCase()
    {
    }
    static void TearDownTestCase()
    {
    }
    void SetUp()
    {
        q1_ = kupl_queue_create();
        q2_ = kupl_queue_create();
        e_ = kupl_event_create();
        int num_executors = kupl_get_num_executors();
        int exe[num_executors];
        for (int i = 0; i < num_executors; i++) {
            exe[i] = i;
        }
        egroup = kupl_egroup_create(exe, num_executors);
        KUPL_1D_RANGE_INIT(range, 0, num_executors);
    }
    void TearDown()
    {
        kupl_event_destroy(e_);
        e_ = nullptr;
        kupl_queue_destroy(q2_);
        q2_ = nullptr;
        kupl_queue_destroy(q1_);
        q1_ = nullptr;
        kupl_egroup_destroy(egroup);
        egroup = nullptr;
    }
    kupl_queue_h q1_ = nullptr;
    kupl_queue_h q2_ = nullptr;
    kupl_event_h e_ = nullptr;
    kupl_egroup_h egroup = nullptr;
    kupl_nd_range_t range;
};

static inline void task_int_loop_1D(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
    static const int ms = 1000;
    std::atomic<int> *count = (std::atomic<int> *)args;
    for (int i = nd_range->nd_range[0].lower; i < nd_range->nd_range[0].upper; i += nd_range->nd_range[0].step) {
        count->fetch_add(1);
    }
    usleep(ms);
}

static void kernel_parallel(void *args)
{
    std::atomic<int> count = {0};
    int num_executors = kupl_get_num_executors();
    int exe[num_executors];
    for (int i = 0; i < num_executors; i++) {
        exe[i] = i;
    }
    kupl_egroup_h eg = kupl_egroup_create(exe, num_executors);
    kupl_nd_range_t range;
    KUPL_1D_RANGE_INIT(range, 0, num_executors);
    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range,
        .egroup = eg,
        .concurrency = num_executors,
        .policy = KUPL_LOOP_POLICY_STATIC
    };
    int ret = kupl_parallel_for(&desc, task_int_loop_1D, &count);
    EXPECT_EQ(ret, KUPL_OK);
    EXPECT_EQ(count.load(), num_executors);
    kupl_egroup_destroy(eg);
}

TEST_F(test_queue_submit, kupl_queue_submit_parallel)
{
    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
        .func = kernel_parallel,
        .args = nullptr,
        .name = "parallel"
    };
    int ret = kupl_queue_submit(q1_, &desc);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_queue_wait(q1_);
    ASSERT_TRUE(ret == KUPL_OK);
}

static void egroup_func1(void *args)
{
    int eid = kupl_get_executor_num();
    ASSERT_TRUE(eid == 0);
}

static void egroup_func2(void *args)
{
    int eid = kupl_get_executor_num();
    ASSERT_TRUE(eid == 2);
}

static void egroup_func3(void *args)
{
    int eid = kupl_get_executor_num();
    ASSERT_TRUE(eid == 4);
}

TEST_F(test_queue_submit, kupl_queue_submit_egroup)
{
    int exe1[4] = {0, 1, 2, 3};
    int exe2[4] = {2, 3, 4, 5};
    int exe3[4] = {4, 5, 6, 7};
    auto egroup1 = kupl_egroup_create(exe1, 4);
    auto egroup2 = kupl_egroup_create(exe2, 4);
    auto egroup3 = kupl_egroup_create(exe3, 4);

    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME | KUPL_QUEUE_ITEM_DESC_FIELD_EGROUP,
        .func = egroup_func1,
        .args = nullptr,
        .name = "egroup",
        .egroup = egroup1
    };
    kupl_queue_submit(q1_, &desc);
    kupl_queue_wait(q1_);

    desc.func = egroup_func2;
    desc.egroup = egroup2;
    kupl_queue_submit(q1_, &desc);
    kupl_queue_wait(q1_);
    desc.func = egroup_func3;
    desc.egroup = egroup3;
    kupl_queue_submit(q1_, &desc);
    kupl_queue_wait(q1_);
    kupl_egroup_destroy(egroup1);
    kupl_egroup_destroy(egroup2);
    kupl_egroup_destroy(egroup3);
}

TEST_F(test_queue_submit, kupl_queue_submit_egroup_nullptr)
{
    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME | KUPL_QUEUE_ITEM_DESC_FIELD_EGROUP,
        .func = egroup_func1,
        .args = nullptr,
        .name = "egroup",
        .egroup = nullptr
    };
    int ret = kupl_queue_submit(q1_, &desc);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_queue_wait(q1_);
    ASSERT_TRUE(ret == KUPL_OK);
}

TEST_F(test_queue_submit, kupl_queue_submit_lambda_egroup)
{
    int exe1[4] = {0, 1, 2, 3};
    int exe2[4] = {2, 3, 4, 5};
    int exe3[4] = {4, 5, 6, 7};
    auto egroup1 = kupl_egroup_create(exe1, 4);
    auto egroup2 = kupl_egroup_create(exe2, 4);
    auto egroup3 = kupl_egroup_create(exe3, 4);

    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME | KUPL_QUEUE_ITEM_DESC_FIELD_EGROUP,
        .name = "egroup",
        .egroup = egroup1
    };
    kupl::queue_submit(q1_, &desc, []() {
        int eid = kupl_get_executor_num();
        ASSERT_TRUE(eid == 0);
    });
    kupl_queue_wait(q1_);

    desc.func = egroup_func2;
    desc.egroup = egroup2;
    kupl::queue_submit(q1_, &desc, []() {
        int eid = kupl_get_executor_num();
        ASSERT_TRUE(eid == 2);
    });
    kupl_queue_wait(q1_);
    desc.func = egroup_func3;
    desc.egroup = egroup3;
    kupl::queue_submit(q1_, &desc, []() {
        int eid = kupl_get_executor_num();
        ASSERT_TRUE(eid == 4);
    });
    kupl_queue_wait(q1_);
    kupl_egroup_destroy(egroup1);
    kupl_egroup_destroy(egroup2);
    kupl_egroup_destroy(egroup3);
}

TEST_F(test_queue_submit, kupl_queue_submit_egroup_lambda_nullptr)
{
    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME | KUPL_QUEUE_ITEM_DESC_FIELD_EGROUP,
        .name = "egroup",
        .egroup = nullptr
    };
    int ret = kupl::queue_submit(q1_, &desc, []() {
        int eid = kupl_get_executor_num();
        ASSERT_TRUE(eid == 0);
    });
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_queue_wait(q1_);
    ASSERT_TRUE(ret == KUPL_OK);
}

TEST_F(test_queue_submit, kupl_queue_submit_lambda_kernel_0)
{
    size_t total = 0;
    size_t num_executors = (size_t)kupl_get_num_executors();
    for (size_t i = 0; i < num_executors; i++) {
        total += i;
    }
    kupl_queue_kernel_desc_t desc = {
        .field_mask = 0,
        .range = &range,
        .egroup = egroup,
    };
    std::atomic<int> sum(0);
    int ret = kupl::queue_submit(q1_, &desc, [&](const kupl_nd_range_t *nd_range) {
        sum += nd_range->nd_range[0].lower;
    });
    ASSERT_TRUE(ret == KUPL_OK);
    kupl_queue_wait(q1_);
    ASSERT_TRUE(total == sum.load());
}

TEST_F(test_queue_submit, kupl_queue_submit_kernel_lambda_1)
{
    size_t total = ((size_t)kupl_get_num_executors()) >> 1;
    KUPL_1D_RANGE_INIT(range, 0, total);
    kupl_queue_kernel_desc_t desc = {
        .field_mask = 0,
        .range = &range,
        .egroup = egroup,
    };
    std::atomic<int> sum(0);
    int ret = kupl::queue_submit(q1_, &desc, [&](const kupl_nd_range_t *nd_range) {
        sum += 1;
    });
    ASSERT_TRUE(ret == KUPL_OK);
    kupl_queue_wait(q1_);
    ASSERT_TRUE(total == sum.load());
}

TEST_F(test_queue_submit, kupl_queue_submit_kernel_lambda_2)
{
    size_t total = 0;
    const size_t arr_size = 1000;
    size_t arr[arr_size] = {0};
    for (size_t i = 0; i < arr_size; i++) {
        arr[i] = i;
        total += arr[i];
    }
    KUPL_1D_RANGE_INIT(range, 0, arr_size);
    kupl_queue_kernel_desc_t desc = {
        .field_mask = 0,
        .range = &range,
        .egroup = egroup,
    };
    std::atomic<int> sum(0);
    int ret = kupl::queue_submit(q1_, &desc, [&](const kupl_nd_range_t *nd_range) {
        int index = nd_range->nd_range[0].lower;
        sum += arr[index];
    });
    ASSERT_TRUE(ret == KUPL_OK);
    kupl_queue_wait(q1_);
    ASSERT_TRUE(total == sum.load());
}

TEST_F(test_queue_submit, kupl_queue_submit_lambda_kernel_invalid_0)
{
    kupl_queue_kernel_desc_t desc = {
        .field_mask = 0,
        .range = &range,
        .egroup = egroup,
    };
    int ret = kupl::queue_submit(nullptr, &desc, [&](const kupl_nd_range_t *nd_range) {});
    ASSERT_TRUE(ret == KUPL_ERROR);
    ret = kupl::queue_submit(q1_, nullptr, [&](const kupl_nd_range_t *nd_range) {});
    ASSERT_TRUE(ret == KUPL_ERROR);
    ret = kupl::queue_submit(q1_, &desc, nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_queue_submit, kupl_queue_submit_lambda_kernel_invalid_1)
{
    kupl_queue_kernel_desc_t desc = {
        .field_mask = 0,
        .range = nullptr,
        .egroup = egroup,
    };
    std::atomic<int> sum(0);
    int ret = kupl::queue_submit(q1_, &desc, [&](const kupl_nd_range_t *nd_range) {
        sum += nd_range->nd_range[0].lower;
    });
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_queue_submit, kupl_queue_submit_lambda_kernel_invalid_2)
{
    kupl_queue_kernel_desc_t desc = {
        .field_mask = 0,
        .range = &range,
        .egroup = nullptr,
    };
    std::atomic<int> sum(0);
    int ret = kupl::queue_submit(q1_, &desc, [&](const kupl_nd_range_t *nd_range) {
        sum += nd_range->nd_range[0].lower;
    });
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_queue_submit, kupl_queue_submit_lambda_kernel_invalid_3)
{
    auto egroup_empty = kupl_egroup_create(nullptr, 0);
    kupl_queue_kernel_desc_t desc = {
        .field_mask = 0,
        .range = &range,
        .egroup = egroup_empty,
    };
    std::atomic<int> sum(0);
    int ret = kupl::queue_submit(q1_, &desc, [&](const kupl_nd_range_t *nd_range) {
        sum += nd_range->nd_range[0].lower;
    });
    ASSERT_TRUE(ret == KUPL_ERROR);
    kupl_egroup_destroy(egroup_empty);
}

TEST_F(test_queue_submit, kupl_queue_submit_lambda_kernel_invalid_4)
{
    kupl_queue_kernel_desc_t desc = {
        .field_mask = KUPL_QUEUE_KERNEL_DESC_FIELD_NAME,
        .range = &range,
        .egroup = egroup,
        .name = nullptr,
    };
    std::atomic<int> sum(0);
    int ret = kupl::queue_submit(q1_, &desc, [&](const kupl_nd_range_t *nd_range) {
        sum += nd_range->nd_range[0].lower;
    });
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_queue_submit, kupl_queue_submit_lambda_kernel_invalid_5)
{
    size_t num_executors = (size_t)kupl_get_num_executors();
    // currently not support 2d
    KUPL_2D_RANGE_INIT(range, 0, num_executors, 0, num_executors);
    kupl_queue_kernel_desc_t desc = {
        .field_mask = 0,
        .range = &range,
        .egroup = egroup,
    };
    std::atomic<int> sum(0);
    int ret = kupl::queue_submit(q1_, &desc, [&](const kupl_nd_range_t *nd_range) {
        sum += nd_range->nd_range[0].lower;
    });
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_queue_submit, kupl_queue_submit_lambda_kernel_invalid_6)
{
    size_t num_executors = (size_t)kupl_get_num_executors();
    KUPL_1D_RANGE_INIT(range, 0, UINT64_MAX);
    kupl_queue_kernel_desc_t desc = {
        .field_mask = 0,
        .range = &range,
        .egroup = egroup,
    };
    std::atomic<int> sum(0);
    int ret = kupl::queue_submit(q1_, &desc, [&](const kupl_nd_range_t *nd_range) {
        sum += nd_range->nd_range[0].lower;
    });
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_queue_submit, kupl_queue_submit_lambda)
{
    size_t total = 1;

    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
        .name = "queue_submit_lambda"
    };

    // invalid func
    int ret = kupl::queue_submit(q1_, &desc, nullptr);
    EXPECT_EQ(ret, KUPL_ERROR);

    // invalid desc
    ret = kupl::queue_submit(q1_, nullptr, []() {});
    EXPECT_EQ(ret, KUPL_ERROR);

    size_t sum = 0;
    ret = kupl::queue_submit(q1_, &desc, [&]() {
        sum += 1;
    });
    EXPECT_EQ(ret, KUPL_OK);

    kupl_queue_wait(q1_);
    EXPECT_EQ(sum, total);
}
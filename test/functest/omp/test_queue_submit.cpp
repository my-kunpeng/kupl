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
    std::atomic<int> *count = (std::atomic<int>*)args;
    for (int i = nd_range->nd_range[0].lower; i < nd_range->nd_range[0].upper; i += nd_range->nd_range[0].step) {
        count->fetch_add(1);
    }
    usleep(ms);
}

static void queue_task_parallel(void *args)
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
    #pragma omp parallel num_threads(num_executors)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg);
    }

    EXPECT_EQ(count.load(), num_executors);
    kupl_egroup_destroy(eg);
}

TEST_F(test_queue_submit, kupl_queue_submit_parallel)
{
    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
        .func = queue_task_parallel,
        .args = nullptr,
        .name = "parallel"
    };
    int ret = kupl_queue_submit(q1_, &desc);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_queue_wait(q1_);
    ASSERT_TRUE(ret == KUPL_OK);
}

static void kernel_parallel_multi(void *args)
{
    kupl_egroup_h eg = (kupl_egroup_h)args;
    int num_executors = kupl_get_num_executors() / 2;
    std::atomic<int> count = {0};
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
}

TEST_F(test_queue_submit, kupl_queue_submit_parallel_multi)
{
    int num_executors = kupl_get_num_executors();
    int num_executors_half = num_executors / 2;
    int exe1[num_executors_half];
    int exe2[num_executors_half];
    int exe_all[num_executors];
    for (int i = 0; i < num_executors_half; i++) {
        exe1[i] = i;
        exe2[i] = i + num_executors_half;
    }
    for (int i = 0; i < num_executors; i++) {
        exe_all[i] = i;
    }
    kupl_egroup_h eg1 = kupl_egroup_create(exe1, num_executors_half);
    kupl_egroup_h eg2 = kupl_egroup_create(exe2, num_executors_half);
    kupl_egroup_h eg_all = kupl_egroup_create(exe_all, num_executors);

    #pragma omp parallel num_threads(num_executors)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            kupl_queue_item_desc_t desc = {
                .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
                .func = kernel_parallel_multi,
                .args = eg1,
                .name = "parallel_1",
                .egroup = eg1,
            };
            kupl_queue_submit(q1_, &desc);
            desc = {
                .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
                .func = kernel_parallel_multi,
                .args = eg2,
                .name = "parallel_2",
                .egroup = eg2,
            };
            kupl_queue_submit(q2_, &desc);
            kupl_queue_wait(q1_);
            kupl_queue_wait(q2_);
        }
        kupl_egroup_barrier(eg_all);
    }

    kupl_egroup_destroy(eg1);
    kupl_egroup_destroy(eg2);
    kupl_egroup_destroy(eg_all);
}

TEST_F(test_queue_submit, kupl_queue_submit_lambda_kernel)
{
    size_t total = 0;
    size_t num_executors = (size_t)kupl_get_num_executors();
    for (size_t i = 0; i < num_executors; i++) {
        total += i;
    }
    #pragma omp parallel num_threads(num_executors)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            kupl_queue_kernel_desc_t desc = {
                .field_mask = 0,
                .range = &range,
                .egroup = egroup,
            };
            std::atomic<int> sum(0);
            int ret = kupl::queue_submit(q1_, &desc, [&](const kupl_nd_range_t *nd_range) {
                sum += nd_range->nd_range[0].lower;
            });
            EXPECT_EQ(ret, KUPL_OK);

            kupl_queue_wait(q1_);
            EXPECT_EQ(sum.load(), total);
        }
        kupl_egroup_barrier(egroup);
    }
}

TEST_F(test_queue_submit, kupl_queue_submit_lambda)
{
    size_t total = 1;
    size_t num_executors = (size_t)kupl_get_num_executors();

    #pragma omp parallel num_threads(num_executors)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            kupl_queue_item_desc_t desc = {
                .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
                .name = "queue_submit_lambda"
            };
            size_t sum = 0;
            int ret = kupl::queue_submit(q1_, &desc, [&]() {
                sum += 1;
            });
            EXPECT_EQ(ret, KUPL_OK);

            kupl_queue_wait(q1_);
            EXPECT_EQ(sum, total);
        }
        kupl_egroup_barrier(egroup);
    }
}
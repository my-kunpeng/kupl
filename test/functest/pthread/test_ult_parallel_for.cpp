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
#include <atomic>
#include "gtest/gtest.h"
#include "kupl.h"

static const int DIM_0 = 0;
static const int DIM_1 = 1;
static const int DIM_2 = 2;

static inline void task_int_loop_1D(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
    std::atomic<int> *count = (std::atomic<int> *)args;
    for (int i = nd_range->nd_range[DIM_0].lower;
         i < nd_range->nd_range[DIM_0].upper; i += nd_range->nd_range[DIM_0].step) {
        count->fetch_add(1);
    }
}

static void task_int_loop_1D_backwards(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
    std::atomic<int> *count = (std::atomic<int> *)args;
    for (int i = nd_range->nd_range[DIM_0].lower;
         i > nd_range->nd_range[DIM_0].upper; i += nd_range->nd_range[DIM_0].step) {
        count->fetch_add(1);
    }
    return;
}

static inline void task_int_loop_2D(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
    std::atomic<int> *count = (std::atomic<int> *)args;
    for (int i = nd_range->nd_range[DIM_0].lower;
         i < nd_range->nd_range[DIM_0].upper; i += nd_range->nd_range[DIM_0].step) {
        for (int j = nd_range->nd_range[DIM_1].lower;
             j < nd_range->nd_range[DIM_1].upper; j += nd_range->nd_range[DIM_1].step) {
                count->fetch_add(1);
        }
    }
}

static inline void task_int_loop_3D(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
    std::atomic<int> *count = (std::atomic<int> *)args;
    for (int i = nd_range->nd_range[DIM_0].lower;
         i < nd_range->nd_range[DIM_0].upper; i += nd_range->nd_range[DIM_0].step) {
        for (int j = nd_range->nd_range[DIM_1].lower;
             j < nd_range->nd_range[DIM_1].upper; j += nd_range->nd_range[DIM_1].step) {
            for (int k = nd_range->nd_range[DIM_2].lower;
                 k < nd_range->nd_range[DIM_2].upper; k += nd_range->nd_range[DIM_2].step) {
                count->fetch_add(1);
            }
        }
    }
}

TEST(test_ult_pf, kupl_parallel_for)
{
    std::atomic<int> count = {0};

    kupl_nd_range_t range4;
    int exe8[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    kupl_egroup_h eg8 = kupl_egroup_create(exe8, 8);
    EXPECT_NE(exe8, nullptr);
    kupl_parallel_for_desc_t desc4 = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range4,
        .egroup = eg8,
        .concurrency = 4,
        .policy = KUPL_LOOP_POLICY_STATIC
    };

    // thread=4, range=5, block=1
    KUPL_1D_RANGE_INIT(range4, 1, 6);
    int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(ret, KUPL_OK);
    EXPECT_EQ(count.load(), 5);

    // thread=4, range=4, block=1
    count = 0;
    KUPL_1D_RANGE_INIT(range4, 1, 5);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 4);

    // thread=4, range=2, block=1
    count = 0;
    KUPL_1D_RANGE_INIT(range4, 1, 3);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 2);

    // thread=4, range=2, block=1(negative bondary)
    count = 0;
    KUPL_1D_RANGE_INIT(range4, -3, -1);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 2);

    // thread=4, range=20, step=-4
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, 22, 2, -4, 1);
    kupl_parallel_for(&desc4, task_int_loop_1D_backwards, &count);
    EXPECT_EQ(count.load(), 5);

    // thread=4, range=18, step=-4
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, 20, 2, -4, 1);
    kupl_parallel_for(&desc4, task_int_loop_1D_backwards, &count);
    EXPECT_EQ(count.load(), 5);

    // thread=4, range=8, block=4
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, 2, 14, 1, 4);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 12);

    // thread=4, range=18, block=4
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, 2, 20, 1, 4);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 18);

    // thread=4, range=22, block=4
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, 2, 24, 1, 4);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 22);

    // thread=4, range=22, block=4(negative bondary)
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, -24, -2, 1, 4);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 22);

    // dynamic policy
    // policy=dynamic thread=4, range=5, block=1
    desc4.policy = KUPL_LOOP_POLICY_DYNAMIC;
    count = 0;
    KUPL_1D_RANGE_INIT(range4, 1, 6);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 5);

    // policy=dynamic thread=4, range=4, block=1
    count = 0;
    KUPL_1D_RANGE_INIT(range4, 1, 5);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 4);

    // policy=dynamic thread=4, range=2, step=1
    count = 0;
    KUPL_1D_RANGE_INIT(range4, 1, 3);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 2);

    // policy=dynamic thread=4, range=2, step=1(negative bondary)
    count = 0;
    KUPL_1D_RANGE_INIT(range4, -3, -1);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 2);

    // policy=dynamic thread=4, range=8, step=4
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, 2, 14, 1, 4);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 12);

    // policy=dynamic thread=4, range=18, step=4
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, 2, 20, 1, 4);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 18);

    // policy=dynamic thread=4, range=22, step=4
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, 2, 24, 1, 4);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 22);


    // test for reuse policy static after dynamic
    // thread=4, range=5, block=1
    count = 0;
    desc4.policy = KUPL_LOOP_POLICY_STATIC;
    KUPL_1D_RANGE_INIT(range4, 1, 6);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 5);

    kupl_egroup_destroy(eg8);
}

TEST(test_ult_pf_2d, kupl_parallel_for)
{
    std::atomic<int> count = {0};

    kupl_nd_range_t range4_2d;
    int exe8[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    kupl_egroup_h eg8 = kupl_egroup_create(exe8, 8);
    EXPECT_NE(exe8, nullptr);
    kupl_parallel_for_desc_t desc4 = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range4_2d,
        .egroup = eg8,
        .concurrency = 4,
        .policy = KUPL_LOOP_POLICY_STATIC
    };
// policy static
// thread=4, range[0, 1]=6, 8, block=1
    count = 0;
    range4_2d.dim = 2;
    int upper_list[2] = {6, 8};
    for (int i = 0; i < range4_2d.dim; i++) {
        range4_2d.nd_range[i].lower = 0;
        range4_2d.nd_range[i].upper = upper_list[i];
        range4_2d.nd_range[i].step = 1;
        range4_2d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_2D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 48);

// thread=4, range[0, 1]=10, 15, block=1
    count = 0;
    int upper_list_2[2] = {10, 15};
    for (int i = 0; i < range4_2d.dim; i++) {
        range4_2d.nd_range[i].lower = 0;
        range4_2d.nd_range[i].upper = upper_list_2[i];
        range4_2d.nd_range[i].step = 1;
        range4_2d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_2D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 150);

// thread=4, range[0, 1]=40, 20, block=1
    count = 0;
    int upper_list_3[2] = {40, 20};
    for (int i = 0; i < range4_2d.dim; i++) {
        range4_2d.nd_range[i].lower = 0;
        range4_2d.nd_range[i].upper = upper_list_3[i];
        range4_2d.nd_range[i].step = 1;
        range4_2d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_2D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 800);

// policy dynamic
    desc4.policy = KUPL_LOOP_POLICY_DYNAMIC;
// thread=4, range[0, 1]=6, 8, block=1
    count = 0;
    range4_2d.dim = 2;
    for (int i = 0; i < range4_2d.dim; i++) {
        range4_2d.nd_range[i].lower = 0;
        range4_2d.nd_range[i].upper = upper_list[i];
        range4_2d.nd_range[i].step = 1;
        range4_2d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_2D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 48);

// thread=4, range[0, 1]=10, 15, block=1
    count = 0;
    for (int i = 0; i < range4_2d.dim; i++) {
        range4_2d.nd_range[i].lower = 0;
        range4_2d.nd_range[i].upper = upper_list_2[i];
        range4_2d.nd_range[i].step = 1;
        range4_2d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_2D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 150);

// thread=4, range[0, 1]=40, 20, block=1
    count = 0;
    for (int i = 0; i < range4_2d.dim; i++) {
        range4_2d.nd_range[i].lower = 0;
        range4_2d.nd_range[i].upper = upper_list_3[i];
        range4_2d.nd_range[i].step = 1;
        range4_2d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_2D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 800);

    kupl_egroup_destroy(eg8);
}

TEST(test_ult_pf_3d, kupl_parallel_for)
{
    std::atomic<int> count = {0};

    kupl_nd_range_t range4_3d;
    int exe8[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    kupl_egroup_h eg8 = kupl_egroup_create(exe8, 8);
    EXPECT_NE(exe8, nullptr);
    kupl_parallel_for_desc_t desc4 = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range4_3d,
        .egroup = eg8,
        .concurrency = 4,
        .policy = KUPL_LOOP_POLICY_STATIC
    };

// thread=4, range[0, 1, 2]=4, 6, 8, block=1
    count = 0;
    range4_3d.dim = 3;
    int upper_list[3] = {4, 6, 8};
    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 192);

// thread=4, range[0, 1, 2]=3, 5, 10, block=1
    count = 0;
    range4_3d.dim = 3;
    int upper_list_1[3] = {3, 5, 10};
    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list_1[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 150);

// thread=4, range[0, 1, 2]=7, 11, 14, block=1
    count = 0;
    range4_3d.dim = 3;
    int upper_list_2[3] = {7, 11, 14};
    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list_2[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 1078);

// thread=4, range[0, 1, 2]=9, 13, 16, block=1
    count = 0;
    range4_3d.dim = 3;
    int upper_list_3[3] = {9, 13, 16};
    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list_3[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 1872);

// thread=4, range[0, 1, 2]=11, 15, 18, block=1
    count = 0;
    range4_3d.dim = 3;
    int upper_list_4[3] = {11, 15, 18};
    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list_4[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 2970);

// thread=4, range[0, 1, 2]=13, 17, 20, block=1
    count = 0;
    range4_3d.dim = 3;
    int upper_list_5[3] = {13, 17, 20};
    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list_5[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 4420);

// thread=4, range[0, 1, 2]=15, 19, 22, block=1
    count = 0;
    range4_3d.dim = 3;
    int upper_list_6[3] = {15, 19, 22};
    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list_6[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 6270);

// thread=4, range[0, 1, 2]=17, 21, 24, block=1
    count = 0;
    range4_3d.dim = 3;
    int upper_list_7[3] = {17, 21, 24};
    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list_7[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 8568);

// thread=4, range[0, 1, 2]=17, 21, 24, block=1 lower not start from 0
    count = 0;
    range4_3d.dim = 3;
    int lower_list_7[3] = {1, 2, 3};
    // upper_list_7[3] = {17, 21, 24}; no change
    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = lower_list_7[i];
        range4_3d.nd_range[i].upper = lower_list_7[i] + upper_list_7[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 8568);

    // dynamic policy
    // thread=4, range[0, 1, 2]=4, 6, 8, block=1
    desc4.policy = KUPL_LOOP_POLICY_DYNAMIC;
    count = 0;
    range4_3d.dim = 3;

    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 192);

// thread=4, range[0, 1, 2]=3, 5, 10, block=1
    count = 0;
    range4_3d.dim = 3;

    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list_1[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 150);

// thread=4, range[0, 1, 2]=7, 11, 14, block=1
    count = 0;
    range4_3d.dim = 3;

    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list_2[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 1078);

// thread=4, range[0, 1, 2]=9, 13, 16, block=1
    count = 0;
    range4_3d.dim = 3;

    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list_3[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 1872);

// thread=4, range[0, 1, 2]=11, 15, 18, block=1
    count = 0;
    range4_3d.dim = 3;

    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list_4[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 2970);

// thread=4, range[0, 1, 2]=13, 17, 20, block=1
    count = 0;
    range4_3d.dim = 3;

    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list_5[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 4420);

// thread=4, range[0, 1, 2]=15, 19, 22, block=1
    count = 0;
    range4_3d.dim = 3;

    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list_6[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 6270);

// thread=4, range[0, 1, 2]=17, 21, 24, block=1
    count = 0;
    range4_3d.dim = 3;

    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = 0;
        range4_3d.nd_range[i].upper = upper_list_7[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 8568);

// thread=4, range[0, 1, 2]=17, 21, 24, block=1 lower not start from 0
    count = 0;
    range4_3d.dim = 3;

    // upper_list_7[3] = {17, 21, 24}; no change
    for (int i = 0; i < range4_3d.dim; i++) {
        range4_3d.nd_range[i].lower = lower_list_7[i];
        range4_3d.nd_range[i].upper = lower_list_7[i] + upper_list_7[i];
        range4_3d.nd_range[i].step = 1;
        range4_3d.nd_range[i].blocksize = 1;
    }
    kupl_parallel_for(&desc4, task_int_loop_3D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 8568);

    kupl_egroup_destroy(eg8);
}

TEST(test_ult_pf, kupl_parallel_for_err)
{
    std::atomic<int> count = {0};
    kupl_nd_range_t range4;
    int exe8[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    kupl_egroup_h eg8 = kupl_egroup_create(exe8, 8);
    KUPL_1D_RANGE_INIT(range4, 0, 24);
    kupl_parallel_for_desc_t desc4 = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range4,
        .egroup = eg8,
        .concurrency = 4,
        .policy = KUPL_LOOP_POLICY_STATIC
    };

    // Error branch handling
    kupl_egroup_h eg_empty = kupl_egroup_create(nullptr, 0);
    desc4.egroup = eg_empty;
    int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(ret, KUPL_ERROR);
    desc4.egroup = eg8;

    ret = kupl_parallel_for(nullptr, task_int_loop_1D, &count);
    EXPECT_EQ(ret, KUPL_ERROR);

    desc4.concurrency = 10000;
    ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(ret, KUPL_ERROR);
    desc4.concurrency = 4;

    range4.nd_range[DIM_0].lower = range4.nd_range[DIM_0].upper + 1;
    ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(ret, KUPL_ERROR);
    range4.nd_range[DIM_0].lower = 2;

    range4.dim = 4;
    ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(ret, KUPL_ERROR);

    range4.dim = 0;
    ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(ret, KUPL_ERROR);
    range4.dim = 1;

    range4.nd_range[DIM_0].upper = -1;
    ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(ret, KUPL_ERROR);
    range4.nd_range[DIM_0].upper = 24;

    range4.nd_range[DIM_0].step = 0;
    ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(ret, KUPL_ERROR);
    range4.nd_range[DIM_0].step = 1;

    range4.nd_range[DIM_0].step = -1;
    ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(ret, KUPL_ERROR);
    range4.nd_range[DIM_0].step = 1;

    kupl_egroup_destroy(eg8);
}

static inline void task_int_loop_no_range(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
    std::atomic<int> *count = (std::atomic<int> *)args;
    count->fetch_add(1);
}

TEST(test_ult_pf, kupl_parallel_for_special_condition)
{
    std::atomic<int> count = {0};

    kupl_nd_range_t range4;
    int exe8[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    kupl_egroup_h eg8 = kupl_egroup_create(exe8, 8);
    EXPECT_NE(exe8, nullptr);
    kupl_parallel_for_desc_t desc4 = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range4,
        .egroup = eg8,
        .concurrency = KUPL_CONCURRENCY_DEFAULT,
        .policy = KUPL_LOOP_POLICY_STATIC
    };

    // undefined concurrency condition
    KUPL_1D_RANGE_INIT(range4, 1, 6);
    int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 5);
    EXPECT_EQ(ret, KUPL_OK);
    desc4.concurrency = 4;

    // 1 thread condition
    desc4.concurrency = 1;
    count = 0;
    ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 5);
    EXPECT_EQ(ret, KUPL_OK);
    desc4.concurrency = 4;

    // null range condition
    desc4.range = nullptr;
    count = 0;
    ret = kupl_parallel_for(&desc4, task_int_loop_no_range, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), desc4.concurrency);
    EXPECT_EQ(ret, KUPL_OK);
    desc4.range = &range4;

    // null egroup condition
    desc4.egroup = nullptr;
    count = 0;
    ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 5);
    EXPECT_EQ(ret, KUPL_OK);
    desc4.egroup = eg8;

    kupl_nd_range_t range4_2d;
    range4_2d.dim = 2;
    int upper_list[2] = {6, 8};
    for (int i = 0; i < range4_2d.dim; i++) {
        range4_2d.nd_range[i].lower = 0;
        range4_2d.nd_range[i].upper = upper_list[i];
        range4_2d.nd_range[i].step = 1;
        range4_2d.nd_range[i].blocksize = 1;
    }
    desc4.range = &range4_2d;

    // undefined concurrency condition pf2d
    desc4.concurrency = KUPL_CONCURRENCY_DEFAULT;
    count = 0;
    ret = kupl_parallel_for(&desc4, task_int_loop_2D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 48);
    EXPECT_EQ(ret, KUPL_OK);
    desc4.concurrency = 4;

    // null egroup condition pf2d
    desc4.egroup = nullptr;
    count = 0;
    ret = kupl_parallel_for(&desc4, task_int_loop_2D, &count);
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 48);
    EXPECT_EQ(ret, KUPL_OK);

    kupl_egroup_destroy(eg8);
}

static inline void task_barrier(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
    kupl_egroup_join_barrier(nullptr);
    kupl_egroup_fork_barrier(nullptr);
}

TEST(test_ult_pf, kupl_parallel_for_egroup_barrier)
{
    int exe8[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    kupl_egroup_h eg8 = kupl_egroup_create(exe8, 8);
    kupl_parallel_for_desc_t desc4 = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = nullptr,
        .egroup = eg8,
        .concurrency = KUPL_CONCURRENCY_DEFAULT,
        .policy = KUPL_LOOP_POLICY_STATIC
    };

    int ret = kupl_parallel_for(&desc4, task_barrier, nullptr);
    EXPECT_EQ(ret, KUPL_OK);

    kupl_egroup_destroy(eg8);
}

TEST(test_ult_pf, kupl_parallel_for_lambda)
{
    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = nullptr,
        .egroup = nullptr,
        .concurrency = KUPL_CONCURRENCY_DEFAULT,
        .policy = KUPL_LOOP_POLICY_STATIC
    };
    int ret = kupl::parallel_for(&desc, [](const kupl_nd_range_t *nd_range, const int tid, const int tnum) {});
    EXPECT_EQ(ret, KUPL_OK);
}

TEST(test_ult_pf, kupl_parallel_for_lambda_invalid)
{
    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_RANGE |
                      KUPL_PARALLEL_FOR_DESC_FIELD_EGROUP |
                      KUPL_PARALLEL_FOR_DESC_FIELD_CONCURRENCY,
        .range = nullptr,
        .egroup = nullptr,
        .concurrency = KUPL_CONCURRENCY_DEFAULT,
        .policy = KUPL_LOOP_POLICY_STATIC
    };
    int ret = kupl::parallel_for(&desc, [](const kupl_nd_range_t *nd_range, const int tid, const int tnum) {});
    EXPECT_EQ(ret, KUPL_ERROR);

    desc.field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_RANGE |
                      KUPL_PARALLEL_FOR_DESC_FIELD_EGROUP |
                      KUPL_PARALLEL_FOR_DESC_FIELD_POLICY;
    ret = kupl::parallel_for(&desc, [](const kupl_nd_range_t *nd_range, const int tid, const int tnum) {});
    EXPECT_EQ(ret, KUPL_ERROR);

    desc.field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_RANGE |
                      KUPL_PARALLEL_FOR_DESC_FIELD_CONCURRENCY |
                      KUPL_PARALLEL_FOR_DESC_FIELD_POLICY;
    ret = kupl::parallel_for(&desc, [](const kupl_nd_range_t *nd_range, const int tid, const int tnum) {});
    EXPECT_EQ(ret, KUPL_ERROR);

    desc.field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_EGROUP |
                      KUPL_PARALLEL_FOR_DESC_FIELD_CONCURRENCY |
                      KUPL_PARALLEL_FOR_DESC_FIELD_POLICY;
    ret = kupl::parallel_for(&desc, [](const kupl_nd_range_t *nd_range, const int tid, const int tnum) {});
    EXPECT_EQ(ret, KUPL_ERROR);
}
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
#include <atomic>
#include <climits>
#include <cfloat>
#include "gtest/gtest.h"
#include "kupl.h"

static const int DIM_0 = 0;
static const int DIM_1 = 1;
static const int DIM_2 = 2;

int task_loop = 0;

static void task_int_loop_1D(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
    std::atomic<int> *count = (std::atomic<int> *)args;
    for (int i = nd_range->nd_range[DIM_0].lower;
         i < nd_range->nd_range[DIM_0].upper; i += nd_range->nd_range[DIM_0].step) {
        count->fetch_add(1);
    }
    return;
}

static void task_int_loop_1D_nullrange(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
    return;
}

static void task_int_loop_2D(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
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

static void task_int_loop_3D(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
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

TEST(test_ult_pf, kupl_parallel_for_static_block1)
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
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 5);

    // thread=4, range=4, block=1
    count = 0;
    KUPL_1D_RANGE_INIT(range4, 1, 5);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 4);

    // thread=4, range=2, block=1
    count = 0;
    KUPL_1D_RANGE_INIT(range4, 1, 3);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 2);

    // thread=4, range=2(negative bondary) block=1
    count = 0;
    KUPL_1D_RANGE_INIT(range4, -3, -1);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 2);

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
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_2D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
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
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_2D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
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
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_2D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
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
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_2D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 48);

// thread=4, range[0, 1]=10, 15, block=1
    count = 0;
    for (int i = 0; i < range4_2d.dim; i++) {
        range4_2d.nd_range[i].lower = 0;
        range4_2d.nd_range[i].upper = upper_list_2[i];
        range4_2d.nd_range[i].step = 1;
        range4_2d.nd_range[i].blocksize = 1;
    }
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_2D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 150);

// thread=4, range[0, 1]=40, 20, block=1
    count = 0;
    for (int i = 0; i < range4_2d.dim; i++) {
        range4_2d.nd_range[i].lower = 0;
        range4_2d.nd_range[i].upper = upper_list_3[i];
        range4_2d.nd_range[i].step = 1;
        range4_2d.nd_range[i].blocksize = 1;
    }
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_2D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
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
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_3D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
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
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_3D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
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
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_3D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
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
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_3D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
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
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_3D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
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
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_3D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
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
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_3D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
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
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_3D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
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
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_3D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 8568);

    kupl_egroup_destroy(eg8);
}

TEST(test_ult_pf, kupl_parallel_for_dynamic_block1)
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
        .policy = KUPL_LOOP_POLICY_DYNAMIC
    };

    // thread=4, range=5, block=1
    KUPL_1D_RANGE_INIT(range4, 1, 6);
#pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 5);

    // thread=4, range=4, block=1
    count = 0;
    KUPL_1D_RANGE_INIT(range4, 1, 5);
#pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 4);

    // thread=4, range=2, block=1
    count = 0;
    KUPL_1D_RANGE_INIT(range4, 1, 3);
#pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 2);

    // thread=4, range=2, block=1(negative bondary)
    count = 0;
    KUPL_1D_RANGE_INIT(range4, -3, -1);
#pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 2);

    kupl_egroup_destroy(eg8);
}

TEST(test_ult_pf, kupl_parallel_for_static_block4)
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

    // thread=4, range=8, block=4
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, 2, 14, 1, 4);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 12);

    // thread=4, range=18, block=4
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, 2, 20, 1, 4);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 18);

    // thread=4, range=22, block=4
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, 2, 24, 1, 4);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 22);

    // thread=4, range=22, block=4(negative bondary)
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, -24, -2, 1, 4);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 22);

    kupl_egroup_destroy(eg8);
}

TEST(test_ult_pf, kupl_parallel_for_task_block1)
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
        .policy = KUPL_LOOP_POLICY_TASK
    };

    // thread=4, range=5, block=1
    KUPL_1D_RANGE_INIT(range4, 1, 6);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 5);

    // thread=4, range=4, block=1
    count = 0;
    KUPL_1D_RANGE_INIT(range4, 1, 5);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 4);

    // thread=4, range=2, block=1
    count = 0;
    KUPL_1D_RANGE_INIT(range4, 1, 3);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 2);

    // thread=4, range=2, block=1(negative bondary)
    count = 0;
    KUPL_1D_RANGE_INIT(range4, -3, -1);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 2);

    kupl_egroup_destroy(eg8);
}

TEST(test_ult_pf, kupl_parallel_for_task_block4)
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
        .policy = KUPL_LOOP_POLICY_TASK
    };

    // thread=4, range=8, block=4
    count = 0;
    task_loop = 1;
    KUPL_STRIDE_1D_RANGE_INIT(range4, 2, 14, 1, 4);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 12);
    task_loop = 0;

    // thread=4, range=18, block=4
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, 2, 20, 1, 4);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 18);

    // thread=4, range=22, block=4
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, 2, 24, 1, 4);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 22);

    // thread=4, range=22, block=4(negative bondary)
    count = 0;
    KUPL_STRIDE_1D_RANGE_INIT(range4, -24, -2, 1, 4);
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(), 22);

    kupl_egroup_destroy(eg8);
}

TEST(test_ult_pf, kupl_parallel_for_not_in_parallel)
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
    count = 0;
    KUPL_1D_RANGE_INIT(range4, 1, 6);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 5);

    // thread=4, range=2, block=1
    count = 0;
    KUPL_1D_RANGE_INIT(range4, 1, 3);
    kupl_parallel_for(&desc4, task_int_loop_1D, &count);
    EXPECT_EQ(count.load(), 2);

    // thread=4, range=12, block=4
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

    kupl_parallel_for_desc_t desc_null_range = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = nullptr,
        .egroup = eg8,
        .concurrency = 4,
        .policy = KUPL_LOOP_POLICY_STATIC
    };

    // thread=4, range=null
    count = 0;
    kupl_parallel_for(&desc_null_range, task_int_loop_1D_nullrange, &count);

    kupl_egroup_destroy(eg8);
}

TEST(test_ult_pf, kupl_parallel_for_default_args)
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
    kupl_nd_range_t range4_2d;
    range4_2d.dim = 2;
    int upper_list[2] = {6, 8};
    for (int i = 0; i < range4_2d.dim; i++) {
        range4_2d.nd_range[i].lower = 0;
        range4_2d.nd_range[i].upper = upper_list[i];
        range4_2d.nd_range[i].step = 1;
        range4_2d.nd_range[i].blocksize = 1;
    }

    // undefined concurrency condition
    KUPL_1D_RANGE_INIT(range4, 1, 6);
    desc4.concurrency = KUPL_CONCURRENCY_DEFAULT;
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 5);

    count = 0;
    desc4.range = &range4_2d;
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_2D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 48);
    desc4.range = &range4;
    desc4.concurrency = 4;

    // null egroup condition
    desc4.egroup = nullptr;
    count = 0;
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 5);

    count = 0;
    desc4.range = &range4_2d;
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for(&desc4, task_int_loop_2D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(count.load(std::memory_order_seq_cst), 48);

    kupl_egroup_destroy(eg8);
}

TEST(test_ult_pf, kupl_parallel_for_master_eid_not_min)
{
    int num_executors = 8;
    int exe[num_executors];
    for (int i = 0; i < num_executors; i++) {
        exe[i] = i;
    }
    auto egroup = kupl_egroup_create(exe, num_executors);
    kupl_nd_range_t range;
    KUPL_1D_RANGE_INIT(range, 0, 8);
    std::atomic<int> count = {0};
    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range,
        .egroup = egroup,
        .concurrency = 4,
        .policy = KUPL_LOOP_POLICY_STATIC
    };

    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 1) {
            int ret = kupl_parallel_for(&desc, task_int_loop_1D, &count);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(egroup);
    }
    EXPECT_EQ(count.load(), 8);
    kupl_egroup_destroy(egroup);
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

static void task_reduce_int_add(kupl_nd_range_t *nd_range, void *args, int tid, int tnum, kupl_reduce_args_t *rd_args)
{
    int *data = (int *)args;
    int localsum = 0;
    for (int i = nd_range->nd_range[DIM_0].lower; i < nd_range->nd_range[DIM_0].upper; i++) {
        localsum += data[i];
    }
    *(int *)rd_args->items[0].buffer += localsum;
}

static void task_reduce_double_sub(kupl_nd_range_t *nd_range, void *args, int tid, int tnum, kupl_reduce_args_t *rd_args)
{
    double *data = (double *)args;
    double localsum = 0.0;
    for (int i = nd_range->nd_range[DIM_0].lower; i < nd_range->nd_range[DIM_0].upper; i++) {
        localsum -= data[i];
    }
    *(double *)rd_args->items[0].buffer += localsum;
}

static void task_reduce_int_max(kupl_nd_range_t *nd_range, void *args, int tid, int tnum, kupl_reduce_args_t *rd_args)
{
    int *data = (int *)args;
    int localmax = INT_MIN;
    for (int i = nd_range->nd_range[DIM_0].lower; i < nd_range->nd_range[DIM_0].upper; i++) {
        localmax = std::max(localmax, data[i]);
    }
    *(int *)rd_args->items[0].buffer = std::max(*(int *)rd_args->items[0].buffer, localmax);
}

static void task_reduce_float_min(kupl_nd_range_t *nd_range, void *args, int tid, int tnum, kupl_reduce_args_t *rd_args)
{
    float *data = (float *)args;
    float localmin = FLT_MAX;
    for (int i = nd_range->nd_range[DIM_0].lower; i < nd_range->nd_range[DIM_0].upper; i++) {
        localmin = std::min(localmin, data[i]);
    }
    *(float *)rd_args->items[0].buffer = std::min(*(float *)rd_args->items[0].buffer, localmin);
}

TEST(test_ult_pf, kupl_parallel_for_reduce_static_1d)
{
    const int n = 100;
    kupl_nd_range_t range4;
    int exe8[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    kupl_egroup_h eg8 = kupl_egroup_create(exe8, 8);
    EXPECT_NE(eg8, nullptr);

    kupl_parallel_for_desc_t desc4 = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range4,
        .egroup = eg8,
        .concurrency = 4,
        .policy = KUPL_LOOP_POLICY_STATIC
    };

    int intData[n];
    double doubleData[n];
    float floatData[n];
    for (int i = 0; i < n; i++) {
        intData[i] = i + 1;
        doubleData[i] = (double)(i + 1);
        floatData[i] = (float)(i + 1);
    }

    // Test 1: int ADD
    KUPL_1D_RANGE_INIT(range4, 0, n);
    int sum_int = 0;
    kupl_reduce_item_t param_int[1] = {{ .buffer = &sum_int, .type = KUPL_DATATYPE_INT, .op = KUPL_RD_ADD }};
    kupl_reduce_args_t rd_args_int = { .num = 1, .items = param_int };
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for_reduce(&desc4, task_reduce_int_add, intData, &rd_args_int);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(sum_int, n * (n + 1) / 2);

    // Test 2: double SUB
    double sum_double = 0.0;
    kupl_reduce_item_t param_double[1] = {{ .buffer = &sum_double, .type = KUPL_DATATYPE_DOUBLE, .op = KUPL_RD_SUB }};
    kupl_reduce_args_t rd_args_double = { .num = 1, .items = param_double };
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for_reduce(&desc4, task_reduce_double_sub, doubleData, &rd_args_double);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_DOUBLE_EQ(sum_double, -(double)(n * (n + 1) / 2));

    // Test 3: int MAX
    int max_int = INT_MIN;
    kupl_reduce_item_t param_max[1] = {{ .buffer = &max_int, .type = KUPL_DATATYPE_INT, .op = KUPL_RD_MAX }};
    kupl_reduce_args_t rd_args_max = { .num = 1, .items = param_max };
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for_reduce(&desc4, task_reduce_int_max, intData, &rd_args_max);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_EQ(max_int, n);

    // Test 4: float MIN
    float min_float = FLT_MAX;
    kupl_reduce_item_t param_min[1] = {{ .buffer = &min_float, .type = KUPL_DATATYPE_FLOAT, .op = KUPL_RD_MIN }};
    kupl_reduce_args_t rd_args_min = { .num = 1, .items = param_min };
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            int ret = kupl_parallel_for_reduce(&desc4, task_reduce_float_min, floatData, &rd_args_min);
            EXPECT_EQ(ret, KUPL_OK);
        }
        kupl_egroup_barrier(eg8);
    }
    EXPECT_FLOAT_EQ(min_float, 1.0f);

    kupl_egroup_destroy(eg8);
}

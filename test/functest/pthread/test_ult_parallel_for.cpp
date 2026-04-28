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
#include <complex>
#include <climits>
#include "gtest/gtest.h"
#include "kupl.h"

static const int DIM_0 = 0;
static const int DIM_1 = 1;
static const int DIM_2 = 2;

typedef struct {
    int *data1;
    float *data2;
    double *data3;
    std::complex<float> *data4;
    std::complex<double> *data5;
} UserArgs;

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

static inline void task_in_loop_reduce1(kupl_nd_range_t *nd_range, void *args, int tid, int tnum, kupl_reduce_args_t *rd_args)
{
    int *data = (int *)args;
    int localsum = 0;
    for (int i = nd_range->nd_range[0].lower; i < nd_range->nd_range[0].upper; i += nd_range->nd_range[0].step) {
        localsum += data[i];
    }
    *(int *)rd_args->items[0].buffer += localsum;
}

static inline void task_in_loop_reduce2(kupl_nd_range_t *nd_range, void *args, int tid, int tnum, kupl_reduce_args_t *rd_args)
{
    double *data = (double *)args;
    double localsum = 0.0;
    for (int i = nd_range->nd_range[0].lower; i > nd_range->nd_range[0].upper; i += nd_range->nd_range[0].step) {
        localsum -= data[i];
    }
    *(double *)rd_args->items[0].buffer += localsum;
}

static inline void task_in_loop_reduce3(kupl_nd_range_t *nd_range, void *args, int tid, int tnum, kupl_reduce_args_t *rd_args)
{
    UserArgs *p = (UserArgs *)args;
    int localsum1 = INT_MIN;
    float localsum2 = FLT_MAX;
    double localsum3 = 0.0;
    std::complex<float> localsum4 = {0.0f, 0.0f};
    std::complex<double> localsum5 = {0.0, 0.0};
    for (int i = nd_range->nd_range[0].lower; i < nd_range->nd_range[0].upper; i += nd_range->nd_range[0].step) {
        localsum1 = std::max(localsum1, p->data1[i]);
        localsum2 = std::min(localsum2, p->data2[i]);
        localsum3 -= p->data3[i];
        localsum4 += p->data4[i];
        localsum5 += p->data5[i];
    }
    *(int *)rd_args->items[0].buffer = std::max(localsum1, *(int *)rd_args->items[0].buffer);
    *(float *)rd_args->items[1].buffer = std::min(localsum2, *(float *)rd_args->items[1].buffer);
    *(double *)rd_args->items[2].buffer += localsum3;
    *(std::complex<float> *)rd_args->items[3].buffer += localsum4;
    *(std::complex<double> *)rd_args->items[4].buffer += localsum5;
}

static inline void task_in_loop_reduce4(kupl_nd_range_t *nd_range, void *args, int tid, int tnum, kupl_reduce_args_t *rd_args)
{
    int localsum = 1;
    *(int *)rd_args->items[0].buffer = localsum;
}

static inline void task_in_loop_reduce_2d(kupl_nd_range_t *nd_range, void *args, int tid, int tnum, kupl_reduce_args_t *rd_args)
{
    int *data = (int *)args;
    int localsum = 0;
    const int COLS = 10;
    for (int i = nd_range->nd_range[0].lower; i < nd_range->nd_range[0].upper; i++) {
        for (int j = nd_range->nd_range[1].lower; j < nd_range->nd_range[1].upper; j++) {
            localsum += data[i * COLS + j];
        }
    }
    *(int *)rd_args->items[0].buffer += localsum;
}

static inline void task_in_loop_reduce_3d(kupl_nd_range_t *nd_range, void *args, int tid, int tnum, kupl_reduce_args_t *rd_args)
{
    int *data = (int *)args;
    int localsum = 0;
    for (int i = nd_range->nd_range[0].lower; i < nd_range->nd_range[0].upper; i++) {
        for (int j = nd_range->nd_range[1].lower; j < nd_range->nd_range[1].upper; j++) {
            for (int k = nd_range->nd_range[2].lower; k < nd_range->nd_range[2].upper; k++) {
                localsum += data[i * 25 + j * 5 + k];
            }
        }
    }
    *(int *)rd_args->items[0].buffer += localsum;
}

TEST(test_ult_pf, kupl_parallel_for_reduce_basic)
{
    const int n = 100;
    kupl_nd_range_t range_forward, range_backward;
    KUPL_1D_RANGE_INIT(range_forward, 0, n);
    KUPL_STRIDE_1D_RANGE_INIT(range_backward, n-1, -1, -1, 1);

    int intData[n];
    double doubleData[n];
    for (int i = 0; i < n; i++) {
        intData[i] = i + 1;
        doubleData[i] = (double)(i + 1);
    }

    int exe8[8] = {0,1,2,3,4,5,6,7};
    kupl_egroup_h eg8 = kupl_egroup_create(exe8, 8);
    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range_forward,
        .egroup = eg8,
        .concurrency = 4,
        .policy = KUPL_LOOP_POLICY_STATIC
    };

    int sum_int = 0;
    kupl_reduce_item_t param_int[1] = {{ .buffer = &sum_int, .type = KUPL_DATATYPE_INT, .op = KUPL_RD_ADD }};
    kupl_reduce_args_t rd_args_int = { .num = 1, .items = param_int };

    double sum_double = 0.0;
    kupl_reduce_item_t param_double[1] = {{ .buffer = &sum_double, .type = KUPL_DATATYPE_DOUBLE, .op = KUPL_RD_SUB }};
    kupl_reduce_args_t rd_args_double = { .num = 1, .items = param_double };

    // STATIC policy - forward range: int ADD
    kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, intData, &rd_args_int);
    EXPECT_EQ(sum_int, n * (n + 1) / 2);

    // STATIC policy - backward range: double SUB
    desc.range = &range_backward;
    kupl_parallel_for_reduce(&desc, task_in_loop_reduce2, doubleData, &rd_args_double);
    EXPECT_DOUBLE_EQ(sum_double, -(double)(n * (n + 1) / 2));

    // DYNAMIC policy - forward range: int ADD
    desc.range = &range_forward;
    desc.policy = KUPL_LOOP_POLICY_DYNAMIC;
    sum_int = 0;
    kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, intData, &rd_args_int);
    EXPECT_EQ(sum_int, n * (n + 1) / 2);

    // DYNAMIC policy - backward range: double SUB
    desc.range = &range_backward;
    sum_double = 0.0;
    kupl_parallel_for_reduce(&desc, task_in_loop_reduce2, doubleData, &rd_args_double);
    EXPECT_DOUBLE_EQ(sum_double, -(double)(n * (n + 1) / 2));

    kupl_egroup_destroy(eg8);
}

TEST(test_ult_pf, kupl_parallel_for_reduce_multi_type)
{
    const int n = 100;
    kupl_nd_range_t range;
    KUPL_1D_RANGE_INIT(range, 0, n);

    int data1[n];
    float data2[n];
    double data3[n];
    std::complex<float> data4[n];
    std::complex<double> data5[n];
    for (int i = 0; i < n; i++) {
        data1[i] = i + 1;
        data2[i] = (float)(i + 1);
        data3[i] = (double)(i + 1);
        data4[i] = std::complex<float>((float)(i+1), (float)(i+1));
        data5[i] = std::complex<double>((double)(i+1), (double)(i+1));
    }

    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range,
        .egroup = NULL,
        .concurrency = 4,
        .policy = KUPL_LOOP_POLICY_STATIC
    };

    kupl_reduce_item_t param[5] = {
        { .buffer = nullptr, .type = KUPL_DATATYPE_INT, .op = KUPL_RD_MAX },
        { .buffer = nullptr, .type = KUPL_DATATYPE_FLOAT, .op = KUPL_RD_MIN },
        { .buffer = nullptr, .type = KUPL_DATATYPE_DOUBLE, .op = KUPL_RD_SUB },
        { .buffer = nullptr, .type = KUPL_DATATYPE_FLOAT_COMPLEX, .op = KUPL_RD_ADD },
        { .buffer = nullptr, .type = KUPL_DATATYPE_DOUBLE_COMPLEX, .op = KUPL_RD_ADD }
    };
    kupl_reduce_args_t rd_args = { .num = 5, .items = param };
    UserArgs args = { data1, data2, data3, data4, data5 };

    // STATIC policy
    int sum1_s = INT_MIN;
    float sum2_s = FLT_MAX;
    double sum3_s = 0.0;
    std::complex<float> sum4_s(0.0f, 0.0f);
    std::complex<double> sum5_s(0.0, 0.0);
    param[0].buffer = &sum1_s;
    param[1].buffer = &sum2_s;
    param[2].buffer = &sum3_s;
    param[3].buffer = &sum4_s;
    param[4].buffer = &sum5_s;

    kupl_parallel_for_reduce(&desc, task_in_loop_reduce3, &args, &rd_args);
    EXPECT_EQ(sum1_s, n);
    EXPECT_FLOAT_EQ(sum2_s, 1.0f);
    EXPECT_DOUBLE_EQ(sum3_s, -(double)(n * (n + 1) / 2));
    EXPECT_EQ(sum4_s, std::complex<float>((float)(n * (n + 1) / 2), (float)(n * (n + 1) / 2)));
    EXPECT_EQ(sum5_s, std::complex<double>((double)(n * (n + 1) / 2), (double)(n * (n + 1) / 2)));

    // DYNAMIC policy
    desc.policy = KUPL_LOOP_POLICY_DYNAMIC;
    int sum1_d = INT_MIN;
    float sum2_d = FLT_MAX;
    double sum3_d = 0.0;
    std::complex<float> sum4_d(0.0f, 0.0f);
    std::complex<double> sum5_d(0.0, 0.0);
    param[0].buffer = &sum1_d;
    param[1].buffer = &sum2_d;
    param[2].buffer = &sum3_d;
    param[3].buffer = &sum4_d;
    param[4].buffer = &sum5_d;

    kupl_parallel_for_reduce(&desc, task_in_loop_reduce3, &args, &rd_args);
    EXPECT_EQ(sum1_d, n);
    EXPECT_FLOAT_EQ(sum2_d, 1.0f);
    EXPECT_DOUBLE_EQ(sum3_d, -(double)(n * (n + 1) / 2));
    EXPECT_EQ(sum4_d, std::complex<float>((float)(n * (n + 1) / 2), (float)(n * (n + 1) / 2)));
    EXPECT_EQ(sum5_d, std::complex<double>((double)(n * (n + 1) / 2), (double)(n * (n + 1) / 2)));

    // TASK policy
    desc.policy = KUPL_LOOP_POLICY_TASK;
    int sum1_t = INT_MIN;
    float sum2_t = FLT_MAX;
    double sum3_t = 0.0;
    std::complex<float> sum4_t(0.0f, 0.0f);
    std::complex<double> sum5_t(0.0, 0.0);
    param[0].buffer = &sum1_t;
    param[1].buffer = &sum2_t;
    param[2].buffer = &sum3_t;
    param[3].buffer = &sum4_t;
    param[4].buffer = &sum5_t;

    kupl_parallel_for_reduce(&desc, task_in_loop_reduce3, &args, &rd_args);
    EXPECT_EQ(sum1_t, n);
    EXPECT_FLOAT_EQ(sum2_t, 1.0f);
    EXPECT_DOUBLE_EQ(sum3_t, -(double)(n * (n + 1) / 2));
    EXPECT_EQ(sum4_t, std::complex<float>((float)(n * (n + 1) / 2), (float)(n * (n + 1) / 2)));
    EXPECT_EQ(sum5_t, std::complex<double>((double)(n * (n + 1) / 2), (double)(n * (n + 1) / 2)));
}

TEST(test_ult_pf, kupl_parallel_for_reduce_edge_cases)
{
    kupl_nd_range_t range;
    int exe8[8] = {0,1,2,3,4,5,6,7};
    kupl_egroup_h eg8 = kupl_egroup_create(exe8, 8);

    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range,
        .egroup = eg8,
        .concurrency = 4,
        .policy = KUPL_LOOP_POLICY_STATIC
    };

    kupl_reduce_item_t param[1] = {{ .buffer = nullptr, .type = KUPL_DATATYPE_INT, .op = KUPL_RD_ADD }};
    kupl_reduce_args_t rd_args = { .num = 1, .items = param };

    // small range (n=5)
    const int n_small = 5;
    int data_small[n_small] = {1,2,3,4,5};
    KUPL_1D_RANGE_INIT(range, 0, n_small);

    int sum_small_s = 0, sum_small_d = 0;
    param[0].buffer = &sum_small_s;
    kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, data_small, &rd_args);
    EXPECT_EQ(sum_small_s, 15);

    desc.policy = KUPL_LOOP_POLICY_DYNAMIC;
    param[0].buffer = &sum_small_d;
    kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, data_small, &rd_args);
    EXPECT_EQ(sum_small_d, 15);

    // single thread (concurrency=1)
    desc.concurrency = 1;
    desc.policy = KUPL_LOOP_POLICY_STATIC;

    const int n_single = 10;
    int data_single[n_single];
    for (int i = 0; i < n_single; i++) data_single[i] = i + 1;
    KUPL_1D_RANGE_INIT(range, 0, n_single);

    int sum_single_s = 0;
    param[0].buffer = &sum_single_s;
    kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, data_single, &rd_args);
    EXPECT_EQ(sum_single_s, 55);

    desc.policy = KUPL_LOOP_POLICY_DYNAMIC;
    int sum_single_d = 0;
    param[0].buffer = &sum_single_d;
    kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, data_single, &rd_args);
    EXPECT_EQ(sum_single_d, 55);

    // large range (n=1000)
    desc.concurrency = 4;
    const int n_large = 1000;
    int data_large[n_large];
    for (int i = 0; i < n_large; i++) data_large[i] = i + 1;
    KUPL_1D_RANGE_INIT(range, 0, n_large);

    int sum_large_s = 0, sum_large_d = 0;
    desc.policy = KUPL_LOOP_POLICY_STATIC;
    param[0].buffer = &sum_large_s;
    kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, data_large, &rd_args);
    EXPECT_EQ(sum_large_s, n_large * (n_large + 1) / 2);

    desc.policy = KUPL_LOOP_POLICY_DYNAMIC;
    param[0].buffer = &sum_large_d;
    kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, data_large, &rd_args);
    EXPECT_EQ(sum_large_d, n_large * (n_large + 1) / 2);

    kupl_egroup_destroy(eg8);
}

TEST(test_ult_pf, kupl_parallel_for_reduce_no_range)
{
    int N = 8;
    int sum = 0;

    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = nullptr,
        .egroup = nullptr,
        .concurrency = N,
        .policy = KUPL_LOOP_POLICY_STATIC
    };
    kupl_reduce_item_t param[1] = {{ .buffer = &sum, .type = KUPL_DATATYPE_INT, .op = KUPL_RD_ADD }};
    kupl_reduce_args_t rd_args = { .num = 1, .items = param };

    kupl_parallel_for_reduce(&desc, task_in_loop_reduce4, nullptr, &rd_args);
    EXPECT_EQ(sum, N);
}

static inline void task_in_loop_nested(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
    int data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    kupl_nd_range_t range;
    KUPL_1D_RANGE_INIT(range, 0, 10);

    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range,
        .egroup = NULL,
        .concurrency = 2,
        .policy = KUPL_LOOP_POLICY_STATIC,
    };
    int sum = 0;
    kupl_reduce_item_t params[1] = {{ .buffer = &sum, .type = KUPL_DATATYPE_INT, .op = KUPL_RD_ADD }};
    kupl_reduce_args_t args_reduce = { .num = 1, .items = params };

    kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, data, &args_reduce);
    EXPECT_EQ(sum, 55);
}

TEST(test_ult_pf, kupl_parallel_for_reduce_nested)
{
    kupl_nd_range_t range;
    KUPL_1D_RANGE_INIT(range, 0, 10);

    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range,
        .egroup = NULL,
        .concurrency = 2,
        .policy = KUPL_LOOP_POLICY_STATIC,
    };
    kupl_parallel_for(&desc, task_in_loop_nested, nullptr);
}

TEST(test_ult_pf, kupl_parallel_for_reduce_nd_static)
{
    const int ROWS = 10, COLS = 10;
    kupl_nd_range_t range_2d;
    range_2d.dim = 2;
    range_2d.nd_range[0] = {0, ROWS, 1, 1};
    range_2d.nd_range[1] = {0, COLS, 1, 1};

    int data[ROWS * COLS];
    for (int i = 0; i < ROWS * COLS; i++) data[i] = i + 1;

    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range_2d,
        .egroup = NULL,
        .concurrency = 4,
        .policy = KUPL_LOOP_POLICY_STATIC
    };

    int sum = 0;
    kupl_reduce_item_t param[1] = {{ .buffer = &sum, .type = KUPL_DATATYPE_INT, .op = KUPL_RD_ADD }};
    kupl_reduce_args_t rd_args = { .num = 1, .items = param };

    kupl_parallel_for_reduce(&desc, task_in_loop_reduce_2d, data, &rd_args);
    EXPECT_EQ(sum, ROWS * COLS * (ROWS * COLS + 1) / 2);
}

TEST(test_ult_pf, kupl_parallel_for_reduce_3d_static)
{
    const int D0 = 4, D1 = 5, D2 = 5;
    kupl_nd_range_t range_3d;
    range_3d.dim = 3;
    range_3d.nd_range[0] = {0, D0, 1, 1};
    range_3d.nd_range[1] = {0, D1, 1, 1};
    range_3d.nd_range[2] = {0, D2, 1, 1};

    int total = D0 * D1 * D2;
    int data[total];
    for (int i = 0; i < total; i++) data[i] = i + 1;

    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range_3d,
        .egroup = NULL,
        .concurrency = 4,
        .policy = KUPL_LOOP_POLICY_STATIC
    };

    int sum = 0;
    kupl_reduce_item_t param[1] = {{ .buffer = &sum, .type = KUPL_DATATYPE_INT, .op = KUPL_RD_ADD }};
    kupl_reduce_args_t rd_args = { .num = 1, .items = param };

    kupl_parallel_for_reduce(&desc, task_in_loop_reduce_3d, data, &rd_args);
    EXPECT_EQ(sum, total * (total + 1) / 2);
}

TEST(test_ult_pf, kupl_parallel_for_reduce_invalid)
{
    kupl_nd_range_t range;
    KUPL_1D_RANGE_INIT(range, 0, 10);
    int sum = 0;

    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = nullptr,
        .egroup = nullptr,
        .concurrency = KUPL_CONCURRENCY_DEFAULT,
        .policy = KUPL_LOOP_POLICY_STATIC,
    };
    kupl_reduce_item_t rd_items[1] = {{ .buffer = &sum, .type = KUPL_DATATYPE_INT, .op = KUPL_RD_ADD }};
    kupl_reduce_args_t rd_args = { .num = 1, .items = rd_items };

    // invalid op
    rd_items[0].op = (kupl_reduce_op_t)-1;
    EXPECT_EQ(kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, nullptr, &rd_args), KUPL_ERROR);
    rd_items[0].op = (kupl_reduce_op_t)100;
    EXPECT_EQ(kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, nullptr, &rd_args), KUPL_ERROR);

    // invalid type
    rd_items[0].type = (kupl_datatype_t)-1;
    rd_items[0].op = KUPL_RD_ADD;
    EXPECT_EQ(kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, nullptr, &rd_args), KUPL_ERROR);
    rd_items[0].type = (kupl_datatype_t)100;
    EXPECT_EQ(kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, nullptr, &rd_args), KUPL_ERROR);

    // complex + MAX/MIN
    rd_items[0].type = KUPL_DATATYPE_FLOAT_COMPLEX;
    rd_items[0].op = KUPL_RD_MAX;
    EXPECT_EQ(kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, nullptr, &rd_args), KUPL_ERROR);
    rd_items[0].type = KUPL_DATATYPE_DOUBLE_COMPLEX;
    rd_items[0].op = KUPL_RD_MIN;
    EXPECT_EQ(kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, nullptr, &rd_args), KUPL_ERROR);

    // rd_num invalid
    rd_args.num = 0;
    rd_items[0].type = KUPL_DATATYPE_INT;
    rd_items[0].op = KUPL_RD_ADD;
    EXPECT_EQ(kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, nullptr, &rd_args), KUPL_ERROR);
    rd_args.num = 1000;
    EXPECT_EQ(kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, nullptr, &rd_args), KUPL_ERROR);

    // items nullptr
    rd_args.num = 1;
    rd_args.items = nullptr;
    EXPECT_EQ(kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, nullptr, &rd_args), KUPL_ERROR);

    // rd_args nullptr
    EXPECT_EQ(kupl_parallel_for_reduce(&desc, task_in_loop_reduce1, nullptr, nullptr), KUPL_ERROR);

    // func nullptr
    rd_args.items = rd_items;
    EXPECT_EQ(kupl_parallel_for_reduce(&desc, nullptr, nullptr, &rd_args), KUPL_ERROR);
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
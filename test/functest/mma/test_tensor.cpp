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
#if defined(ENABLE_KUPL_MMA)

#include <arm_bf16.h>
#include <cstdint>
#include "gtest/gtest.h"
#include "kupl_mma.h"
using namespace kupl::tensor;

TEST(test_tensor, tensor_index)
{
    const int M = 32;
    const int N = 16;
    double *data = (double*)malloc(sizeof(double) * M * N);
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            data[i * N + j] = i * N + j;
        }
    }

    auto shape = make_shape(Int<32>{}, Int<16>{});
    auto stride = make_stride(Int<16>{}, Int<1>{});
    auto layout = make_layout(shape, stride);
    auto tensor = make_tensor(data, layout);

    auto coord1 = make_coord(Int<2>{}, Int<2>{});
    auto ret1 = tensor(coord1);
    ASSERT_TRUE(ret1 == data[2 * 16 +2]);

    auto coord2 = make_coord(Int<2>{}, Underscore{});
    auto ret2 = tensor(coord2);
    auto coord3 = make_coord(Int<2>{});
    auto ret3 = ret2(coord3);
    ASSERT_TRUE(ret3 == data[2 * 16 +2]);

    free(data);
}

TEST(test_tensor, tensor_operator_add)
{
    const int M = 3;
    const int N = 8;
    double *data_a = (double*)malloc(sizeof(double) * M * N);
    double *data_b = (double*)malloc(sizeof(double) * M * N);
    double *data_c = (double*)malloc(sizeof(double) * M * N);
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            data_a[i * N + j] = i * N + j;
            data_b[i * N + j] = M * N - i * N - j;
            data_c[i * N + j] = 0;
        }
    }

    auto shape = make_shape(Int<3>{}, Int<8>{});
    auto stride = make_stride(Int<8>{}, Int<1>{});
    auto layout = make_layout(shape, stride);
    auto tensor_a = make_tensor(data_a, layout);
    auto tensor_b = make_tensor(data_b, layout);
    auto tensor_c = make_tensor(data_c, layout);

    tensor_c = tensor_a + tensor_b;

    bool ret = true;
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            auto coord = make_coord(i, j);
            if (tensor_c(coord) != M * N) {
                ret = false;
            }
        }
    }
    ASSERT_TRUE(ret = true);

    free(data_c);
    free(data_b);
    free(data_a);
}

TEST(test_tensor, tensor_operator_leftscalmul)
{
    const int M = 3;
    const int N = 8;
    double *data_a = (double*)malloc(sizeof(double) * M * N);
    double *data_b = (double*)malloc(sizeof(double) * M * N);
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            data_a[i * N + j] = i * N + j;
            data_b[i * N + j] = 0;
        }
    }

    auto shape = make_shape(Int<3>{}, Int<8>{});
    auto stride = make_stride(Int<8>{}, Int<1>{});
    auto layout = make_layout(shape, stride);
    auto tensor_a = make_tensor(data_a, layout);
    auto tensor_b = make_tensor(data_b, layout);

    tensor_b = tensor_a * 2.0;

    bool ret = true;
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            auto coord = make_coord(i, j);
            if (tensor_b(coord) != 2.0 * (i * N + j)) {
                ret = false;
            }
        }
    }
    ASSERT_TRUE(ret = true);

    free(data_b);
    free(data_a);
}

TEST(test_tensor, tensor_operator_rightscalmul)
{
    const int M = 3;
    const int N = 8;
    double *data_a = (double*)malloc(sizeof(double) * M * N);
    double *data_b = (double*)malloc(sizeof(double) * M * N);
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            data_a[i * N + j] = i * N + j;
            data_b[i * N + j] = 0;
        }
    }

    auto shape = make_shape(Int<3>{}, Int<8>{});
    auto stride = make_stride(Int<8>{}, Int<1>{});
    auto layout = make_layout(shape, stride);
    auto tensor_a = make_tensor(data_a, layout);
    auto tensor_b = make_tensor(data_b, layout);

    tensor_b = 2.0 * tensor_a;

    bool ret = true;
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            auto coord = make_coord(i, j);
            if (tensor_b(coord) != 2.0 * (i * N + j)) {
                ret = false;
            }
        }
    }
    ASSERT_TRUE(ret = true);

    free(data_b);
    free(data_a);
}

#endif
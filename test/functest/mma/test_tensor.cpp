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

#endif
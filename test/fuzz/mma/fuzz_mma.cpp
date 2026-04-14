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
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <arm_bf16.h>
#include <cstdint>
#include "kupl_mma.h"
#include "common/fuzz_common.h"

using namespace kupl::tensor;

#if defined(__clang__)
#define test_kupl_za
#define test_kupl_streaming
#elif defined(__GNUC__)
#define test_kupl_za        __arm_new("za")
#define test_kupl_streaming __arm_streaming
#endif

test_kupl_za
void mma_32x16x512_F64F64F64_kernel(double *data_a, double *data_b, double *data_c) test_kupl_streaming
{
    auto shape_a = make_shape(Int<32>{}, Int<512>{});
    auto shape_b = make_shape(Int<512>{}, Int<16>{});
    auto shape_c = make_shape(Int<32>{}, Int<16>{});

    auto stride_a = make_stride(Int<1>{}, Int<32>{});
    auto stride_b = make_stride(Int<16>{}, Int<1>{});
    auto stride_c = make_stride(Int<16>{}, Int<1>{});

    auto layout_a = make_layout(shape_a, stride_a);
    auto layout_b = make_layout(shape_b, stride_b);
    auto layout_c = make_layout(shape_c, stride_c);

    auto atom_mma_shape = make_shape(Int<1>{}, Int<1>{}, Int<1>{});
    auto tiled_mma = make_tiled_mma(Ops<MMA_32x16x512_F64F64F64>{}, atom_mma_shape);
    auto atom_store_shape = make_shape(Int<1>{}, Int<1>{});
    auto tiled_store = make_tiled_store(Ops<STORE_32x16_F64>{}, atom_store_shape);

    auto tensor_a = make_tensor(data_a, layout_a);
    auto tensor_b = make_tensor(data_b, layout_b);
    auto tensor_c = make_tensor(data_c, layout_c);
    tensor_tiled_mma(tiled_mma, tensor_c, tensor_a, tensor_b, tensor_c);
    tensor_tiled_store(tiled_store, tensor_c);
}

void mma_32x16x512_F64F64F64_coverage()
{
    const int M = 32;
    const int N = 16;
    const int K = 512;
    double *data_a = (double*)malloc(sizeof(double) * M * K);
    double *data_b = (double*)malloc(sizeof(double) * K * N);
    double *data_c = (double*)malloc(sizeof(double) * M * N);

    mma_32x16x512_F64F64F64_kernel(data_a, data_b, data_c);

    free(data_c);
    free(data_b);
    free(data_a);
}

test_kupl_za
void mma_16x64x2_BF16BF16F32_kernel(bfloat16_t *data_a, bfloat16_t *data_b, float *data_c) test_kupl_streaming
{
    auto shape_a = make_shape(Int<16>{}, make_shape(Int<2>{}, Int<288>{}));
    auto shape_b = make_shape(make_shape(Int<2>{}, Int<288>{}), Int<64>{});
    auto shape_c = make_shape(Int<16>{}, Int<64>{});

    auto stride_a = make_stride(Int<2>{}, make_stride(Int<1>{}, Int<32>{}));
    auto stride_b = make_stride(make_stride(Int<1>{}, Int<128>{}), Int<2>{});
    auto stride_c = make_stride(Int<64>{}, Int<1>{});

    auto layout_a = make_layout(shape_a, stride_a);
    auto layout_b = make_layout(shape_b, stride_b);
    auto layout_c = make_layout(shape_c, stride_c);

    auto atom_mma_shape = make_shape(Int<1>{}, Int<1>{}, Int<288>{});
    auto tiled_mma = make_tiled_mma(Ops<MMA_16x64x2_BF16BF16F32>{}, atom_mma_shape);
    auto atom_store_shape = make_shape(Int<1>{}, Int<1>{});
    auto tile_store = make_tiled_store(Ops<STORE_16x64_F32>{}, atom_store_shape);

    auto tensor_a = make_tensor(data_a, layout_a);
    auto tensor_b = make_tensor(data_b, layout_b);
    auto tensor_c = make_tensor(data_c, layout_c);
    tensor_tiled_mma(tiled_mma, tensor_c, tensor_a, tensor_b, tensor_c);
    tensor_tiled_store(tile_store, tensor_c);
}

void mma_16x64x2_BF16BF16F32_coverage()
{
    const int M = 16;
    const int N = 64;
    const int K = 576;
    bfloat16_t *data_a = (bfloat16_t *)malloc(sizeof(bfloat16_t) * M * K);
    bfloat16_t *data_b = (bfloat16_t *)malloc(sizeof(bfloat16_t) * K * N);
    float *data_c = (float *)malloc(sizeof(float) * M * N);

    mma_16x64x2_BF16BF16F32_kernel(data_a, data_b, data_c);

    free(data_a);
    free(data_b);
    free(data_c);
}

test_kupl_za
void mma_16x64x1_BF16BF16F32_kernel(bfloat16_t *data_a, bfloat16_t *data_b, float *data_c) test_kupl_streaming
{
    auto shape_a = make_shape(Int<16>{}, Int<576>{});
    auto shape_b = make_shape(Int<576>{}, Int<64>{});
    auto shape_c = make_shape(Int<16>{}, Int<64>{});

    auto stride_a = make_stride(Int<1>{}, Int<16>{});
    auto stride_b = make_stride(Int<64>{}, Int<1>{});
    auto stride_c = make_stride(Int<64>{}, Int<1>{});

    auto layout_a = make_layout(shape_a, stride_a);
    auto layout_b = make_layout(shape_b, stride_b);
    auto layout_c = make_layout(shape_c, stride_c);

    auto atom_mma_shape = make_shape(Int<1>{}, Int<1>{}, Int<576>{});
    auto tiled_mma = make_tiled_mma(Ops<MMA_16x64x1_BF16BF16F32>{}, atom_mma_shape);
    auto atom_store_shape = make_shape(Int<1>{}, Int<1>{});
    auto tile_store = make_tiled_store(Ops<STORE_16x64_F32>{}, atom_store_shape);

    auto tensor_a = make_tensor(data_a, layout_a);
    auto tensor_b = make_tensor(data_b, layout_b);
    auto tensor_c = make_tensor(data_c, layout_c);
    tensor_tiled_mma(tiled_mma, tensor_c, tensor_a, tensor_b, tensor_c);
    tensor_tiled_store(tile_store, tensor_c);
}

void mma_16x64x1_BF16BF16F32_coverage()
{
    const int M = 16;
    const int N = 64;
    const int K = 576;
    bfloat16_t *data_a = (bfloat16_t *)malloc(sizeof(bfloat16_t) * M * K);
    bfloat16_t *data_b = (bfloat16_t *)malloc(sizeof(bfloat16_t) * K * N);
    float *data_c = (float *)malloc(sizeof(float) * M * N);

    mma_16x64x1_BF16BF16F32_kernel(data_a, data_b, data_c);

    free(data_a);
    free(data_b);
    free(data_c);
}

test_kupl_za
void mma_16x64x4_INT8INT8INT32_kernel(int8_t *data_a, int8_t *data_b, int32_t *data_c) test_kupl_streaming
{
    auto shape_a = make_shape(Int<16>{}, make_shape(Int<4>{}, Int<144>{}));
    auto shape_b = make_shape(make_shape(Int<4>{}, Int<144>{}), Int<64>{});
    auto shape_c = make_shape(Int<16>{}, Int<64>{});

    auto stride_a = make_stride(Int<4>{}, make_stride(Int<1>{}, Int<64>{}));
    auto stride_b = make_stride(make_stride(Int<1>{}, Int<256>{}), Int<4>{});
    auto stride_c = make_stride(Int<64>{}, Int<1>{});

    auto layout_a = make_layout(shape_a, stride_a);
    auto layout_b = make_layout(shape_b, stride_b);
    auto layout_c = make_layout(shape_c, stride_c);

    auto atom_mma_shape = make_shape(Int<1>{}, Int<1>{}, Int<144>{});
    auto tiled_mma = make_tiled_mma(Ops<MMA_16x64x4_INT8INT8INT32>{}, atom_mma_shape);
    auto atom_store_shape = make_shape(Int<1>{}, Int<1>{});
    auto tile_store = make_tiled_store(Ops<STORE_16x64_INT32>{}, atom_store_shape);

    auto tensor_a = make_tensor(data_a, layout_a);
    auto tensor_b = make_tensor(data_b, layout_b);
    auto tensor_c = make_tensor(data_c, layout_c);
    tensor_tiled_mma(tiled_mma, tensor_c, tensor_a, tensor_b, tensor_c);
    tensor_tiled_store(tile_store, tensor_c);
}

void mma_16x64x4_INT8INT8INT32_coverage()
{
    const int M = 16;
    const int N = 64;
    const int K = 576;
    int8_t *data_a = (int8_t *)malloc(sizeof(int8_t) * M * K);
    int8_t *data_b = (int8_t *)malloc(sizeof(int8_t) * K * N);
    int32_t *data_c = (int32_t *)malloc(sizeof(int32_t) * M * N);

    mma_16x64x4_INT8INT8INT32_kernel(data_a, data_b, data_c);

    free(data_a);
    free(data_b);
    free(data_c);
}

test_kupl_za
void mma_32x32x4_INT8INT8INT32_kernel(int8_t *data_a, int8_t *data_b, int32_t *data_c) test_kupl_streaming
{
    auto shape_a = make_shape(Int<32>{}, make_shape(Int<4>{}, Int<144>{}));
    auto shape_b = make_shape(make_shape(Int<4>{}, Int<144>{}), Int<32>{});
    auto shape_c = make_shape(Int<32>{}, Int<32>{});

    auto stride_a = make_stride(Int<4>{}, make_stride(Int<1>{}, Int<128>{}));
    auto stride_b = make_stride(make_stride(Int<1>{}, Int<128>{}), Int<4>{});
    auto stride_c = make_stride(Int<32>{}, Int<1>{});

    auto layout_a = make_layout(shape_a, stride_a);
    auto layout_b = make_layout(shape_b, stride_b);
    auto layout_c = make_layout(shape_c, stride_c);

    auto atom_mma_shape = make_shape(Int<1>{}, Int<1>{}, Int<144>{});
    auto tiled_mma = make_tiled_mma(Ops<MMA_32x32x4_INT8INT8INT32>{}, atom_mma_shape);
    auto atom_store_shape = make_shape(Int<1>{}, Int<1>{});
    auto tile_store = make_tiled_store(Ops<STORE_32x32_INT32>{}, atom_store_shape);

    auto tensor_a = make_tensor(data_a, layout_a);
    auto tensor_b = make_tensor(data_b, layout_b);
    auto tensor_c = make_tensor(data_c, layout_c);
    tensor_tiled_mma(tiled_mma, tensor_c, tensor_a, tensor_b, tensor_c);
    tensor_tiled_store(tile_store, tensor_c);
}

void mma_32x32x4_INT8INT8INT32_coverage()
{
    const int M = 32;
    const int N = 32;
    const int K = 576;
    int8_t *data_a = (int8_t *)malloc(sizeof(int8_t) * M * K);
    int8_t *data_b = (int8_t *)malloc(sizeof(int8_t) * K * N);
    int32_t *data_c = (int32_t *)malloc(sizeof(int32_t) * M * N);

    mma_32x32x4_INT8INT8INT32_kernel(data_a, data_b, data_c);

    free(data_a);
    free(data_b);
    free(data_c);
}

void mma_coverage()
{
    mma_32x16x512_F64F64F64_coverage();
    mma_16x64x2_BF16BF16F32_coverage();
    mma_16x64x1_BF16BF16F32_coverage();
    mma_16x64x4_INT8INT8INT32_coverage();
    mma_32x32x4_INT8INT8INT32_coverage();
}

test_kupl_za
void mma_32x16x1_F64F64F64_kernel(double *data_a, double *data_b, double *data_c) test_kupl_streaming
{
    auto shape_a = make_shape(Int<32>{}, Int<512>{});
    auto shape_b = make_shape(Int<512>{}, Int<16>{});
    auto shape_c = make_shape(Int<32>{}, Int<16>{});

    auto stride_a = make_stride(Int<1>{}, Int<32>{});
    auto stride_b = make_stride(Int<16>{}, Int<1>{});
    auto stride_c = make_stride(Int<16>{}, Int<1>{});

    auto layout_a = make_layout(shape_a, stride_a);
    auto layout_b = make_layout(shape_b, stride_b);
    auto layout_c = make_layout(shape_c, stride_c);

    auto atom_mma_shape = make_shape(Int<1>{}, Int<1>{}, Int<512>{});
    auto tiled_mma = make_tiled_mma(Ops<MMA_32x16x1_F64F64F64>{}, atom_mma_shape);
    auto atom_store_shape = make_shape(Int<1>{}, Int<1>{});
    auto tiled_store = make_tiled_store(Ops<STORE_32x16_F64>{}, atom_store_shape);

    auto tensor_a = make_tensor(data_a, layout_a);
    auto tensor_b = make_tensor(data_b, layout_b);
    auto tensor_c = make_tensor(data_c, layout_c);
    tensor_tiled_mma(tiled_mma, tensor_c, tensor_a, tensor_b, tensor_c);
    tensor_tiled_store(tiled_store, tensor_c);
}

void mma_base_example(int test_count)
{
    mma_coverage();
    const int M = 32;
    const int N = 16;
    const int K = 512;
    double *data_a_bak = (double*)malloc(sizeof(double) * M * K);
    double *data_b_bak = (double*)malloc(sizeof(double) * K * N);
    double *data_c_bak = (double*)malloc(sizeof(double) * M * N);
    for (int i = 0; i < K; i++) {
        for (int j = 0; j < M; j++) {
            if (i == j) {
                data_a_bak[i * M + j] = 1.0;
            } else {
                data_a_bak[i * M + j] = 0.0;
            }
        }
    }
    for (int i = 0; i < K; i++) {
        for (int j = 0; j < N; j++) {
            data_b_bak[i * N + j] = (double)(i * N + j);
        }
    }
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            data_c_bak[i * N + j] = 0.0;
        }
    }

    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        double *data_a = (double*)DT_SetGetFixBlob(&g_Element[cnt++], sizeof(double) * M * K,
                                                   sizeof(double) * M * K, (char*)data_a_bak);
        double *data_b = (double*)DT_SetGetFixBlob(&g_Element[cnt++], sizeof(double) * K * N,
                                                   sizeof(double) * K * N, (char*)data_b_bak);
        double *data_c = (double*)DT_SetGetFixBlob(&g_Element[cnt++], sizeof(double) * M * N,
                                                   sizeof(double) * M * N, (char*)data_c_bak);

        mma_32x16x1_F64F64F64_kernel(data_a, data_b, data_c);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);

    free(data_c_bak);
    free(data_b_bak);
    free(data_a_bak);
}
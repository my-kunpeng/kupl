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
void copy_1x16_F64_CM2RM_kernel(double *dst, double *src) test_kupl_streaming
{
    constexpr int M = 512;
    constexpr int N = 16;
    auto shape_d = make_shape(Int<M>{}, Int<N>{});
    auto shape_s = make_shape(Int<M>{}, Int<N>{});

    auto stride_d = make_stride(Int<16>{}, Int<1>{});
    auto stride_s = make_stride(Int<1>{}, Int<M>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<M / 1>{}, Int<1>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_1x16_F64_CM2RM>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

void copy_1x16_F64_CM2RM_coverage()
{
    constexpr int M = 512;
    constexpr int N = 16;
    double *dst = static_cast<double *>(malloc(sizeof(double) * M * N));
    double *src = static_cast<double *>(malloc(sizeof(double) * M * N));

    copy_1x16_F64_CM2RM_kernel(dst, src);

    free(src);
    free(dst);
}

test_kupl_za
void copy_16x2_BF16_RM2ZZ_kernel(bfloat16_t *dst, bfloat16_t *src) test_kupl_streaming
{
    constexpr int M = 16;
    constexpr int N = 576;
    auto shape_d = make_shape(Int<M>{}, make_shape(Int<2>{}, Int<N / 2>{}));
    auto shape_s = make_shape(Int<M>{}, Int<N>{});

    auto stride_d = make_stride(Int<2>{}, make_stride(Int<1>{}, Int<32>{}));
    auto stride_s = make_stride(Int<N>{}, Int<1>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<1>{}, Int<N / 2>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_16x2_BF16_RM2ZZ>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

void copy_16x2_BF16_RM2ZZ_coverage()
{
    constexpr int M = 16;
    constexpr int N = 576;
    bfloat16_t *dst = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * M * N));
    bfloat16_t *src = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * M * N));

    copy_16x2_BF16_RM2ZZ_kernel(dst, src);

    free(src);
    free(dst);
}

test_kupl_za
void copy_2x64_BF16_CM2NN_kernel(bfloat16_t *dst, bfloat16_t *src) test_kupl_streaming
{
    constexpr int M = 576;
    constexpr int N = 64;
    auto shape_d = make_shape(make_shape(Int<2>{}, Int<M / 2>{}), Int<N>{});
    auto shape_s = make_shape(Int<M>{}, Int<N>{});

    auto stride_d = make_stride(make_stride(Int<1>{}, Int<128>{}), Int<2>{});
    auto stride_s = make_stride(Int<1>{}, Int<M>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<M / 2>{}, Int<1>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_2x64_BF16_CM2NN>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

void copy_2x64_BF16_CM2NN_coverage()
{
    constexpr int M = 576;
    constexpr int N = 64;
    bfloat16_t *dst = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * M * N));
    bfloat16_t *src = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * M * N));

    copy_2x64_BF16_CM2NN_kernel(dst, src);

    free(src);
    free(dst);
}

test_kupl_za
void copy_16x1_BF16_RM2CM_kernel(bfloat16_t *dst, bfloat16_t *src) test_kupl_streaming
{
    constexpr int M = 16;
    constexpr int N = 576;
    auto shape_d = make_shape(Int<M>{}, Int<N>{});
    auto shape_s = make_shape(Int<M>{}, Int<N>{});

    auto stride_d = make_stride(Int<1>{}, Int<16>{});
    auto stride_s = make_stride(Int<N>{}, Int<1>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<1>{}, Int<N>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_16x1_BF16_RM2CM>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

void copy_16x1_BF16_RM2CM_coverage()
{
    constexpr int M = 16;
    constexpr int N = 576;
    bfloat16_t *dst = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * M * N));
    bfloat16_t *src = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * M * N));

    copy_16x1_BF16_RM2CM_kernel(dst, src);

    free(src);
    free(dst);
}

test_kupl_za
void copy_1x64_BF16_CM2RM_kernel(bfloat16_t *dst, bfloat16_t *src) test_kupl_streaming
{
    constexpr int M = 576;
    constexpr int N = 64;
    auto shape_d = make_shape(Int<M>{}, Int<N>{});
    auto shape_s = make_shape(Int<M>{}, Int<N>{});

    auto stride_d = make_stride(Int<64>{}, Int<1>{});
    auto stride_s = make_stride(Int<1>{}, Int<M>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<M>{}, Int<1>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_1x64_BF16_CM2RM>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

void copy_1x64_BF16_CM2RM_coverage()
{
    constexpr int M = 576;
    constexpr int N = 64;
    bfloat16_t *dst = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * M * N));
    bfloat16_t *src = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * M * N));

    copy_1x64_BF16_CM2RM_kernel(dst, src);

    free(src);
    free(dst);
}

test_kupl_za
void copy_16x4_INT8_RM2ZZ_kernel(int8_t *dst, int8_t *src) test_kupl_streaming
{
    constexpr int M = 16;
    constexpr int N = 576;
    auto shape_d = make_shape(Int<M>{}, make_shape(Int<4>{}, Int<N / 4>{}));
    auto shape_s = make_shape(Int<M>{}, Int<N>{});

    auto stride_d = make_stride(Int<4>{}, make_stride(Int<1>{}, Int<64>{}));
    auto stride_s = make_stride(Int<N>{}, Int<1>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<1>{}, Int<N / 4>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_16x4_INT8_RM2ZZ>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

void copy_16x4_INT8_RM2ZZ_coverage()
{
    constexpr int M = 16;
    constexpr int N = 576;
    int8_t *dst = static_cast<int8_t *>(malloc(sizeof(int8_t) * M * N));
    int8_t *src = static_cast<int8_t *>(malloc(sizeof(int8_t) * M * N));

    copy_16x4_INT8_RM2ZZ_kernel(dst, src);

    free(src);
    free(dst);
}

test_kupl_za
void copy_4x64_INT8_CM2NN_kernel(int8_t *dst, int8_t *src) test_kupl_streaming
{
    constexpr int M = 576;
    constexpr int N = 64;

    auto shape_d = make_shape(make_shape(Int<4>{}, Int<M / 4>{}), Int<N>{});
    auto shape_s = make_shape(Int<M>{}, Int<N>{});

    auto stride_d = make_stride(make_stride(Int<1>{}, Int<256>{}), Int<4>{});
    auto stride_s = make_stride(Int<1>{}, Int<M>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<M / 4>{}, Int<1>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_4x64_INT8_CM2NN>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

void copy_4x64_INT8_CM2NN_coverage()
{
    constexpr int M = 576;
    constexpr int N = 64;
    int8_t *dst = static_cast<int8_t *>(malloc(sizeof(int8_t) * M * N));
    int8_t *src = static_cast<int8_t *>(malloc(sizeof(int8_t) * M * N));

    copy_4x64_INT8_CM2NN_kernel(dst, src);

    free(src);
    free(dst);
}

test_kupl_za
void copy_32x4_INT8_RM2ZZ_kernel(int8_t *dst, int8_t *src) test_kupl_streaming
{
    constexpr int M = 32;
    constexpr int N = 576;
    auto shape_d = make_shape(Int<M>{}, make_shape(Int<4>{}, Int<N / 4>{}));
    auto shape_s = make_shape(Int<M>{}, Int<N>{});

    auto stride_d = make_stride(Int<4>{}, make_stride(Int<1>{}, Int<128>{}));
    auto stride_s = make_stride(Int<N>{}, Int<1>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<1>{}, Int<N / 4>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_32x4_INT8_RM2ZZ>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

void copy_32x4_INT8_RM2ZZ_coverage()
{
    constexpr int M = 32;
    constexpr int N = 576;
    int8_t *dst = static_cast<int8_t *>(malloc(sizeof(int8_t) * M * N));
    int8_t *src = static_cast<int8_t *>(malloc(sizeof(int8_t) * M * N));

    copy_32x4_INT8_RM2ZZ_kernel(dst, src);

    free(src);
    free(dst);
}

test_kupl_za
void copy_4x32_INT8_CM2NN_kernel(int8_t *dst, int8_t *src) test_kupl_streaming
{
    constexpr int M = 576;
    constexpr int N = 32;
    auto shape_d = make_shape(make_shape(Int<4>{}, Int<M / 4>{}), Int<N>{});
    auto shape_s = make_shape(Int<M>{}, Int<N>{});

    auto stride_d = make_stride(make_stride(Int<1>{}, Int<128>{}), Int<4>{});
    auto stride_s = make_stride(Int<1>{}, Int<M>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<M / 4>{}, Int<1>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_4x32_INT8_CM2NN>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

void copy_4x32_INT8_CM2NN_coverage()
{
    constexpr int M = 576;
    constexpr int N = 32;
    int8_t *dst = static_cast<int8_t *>(malloc(sizeof(int8_t) * M * N));
    int8_t *src = static_cast<int8_t *>(malloc(sizeof(int8_t) * M * N));

    copy_4x32_INT8_CM2NN_kernel(dst, src);

    free(src);
    free(dst);
}

void copy_coverage()
{
    copy_1x16_F64_CM2RM_coverage();
    copy_16x2_BF16_RM2ZZ_coverage();
    copy_2x64_BF16_CM2NN_coverage();
    copy_16x1_BF16_RM2CM_coverage();
    copy_1x64_BF16_CM2RM_coverage();
    copy_16x4_INT8_RM2ZZ_coverage();
    copy_4x64_INT8_CM2NN_coverage();
    copy_32x4_INT8_RM2ZZ_coverage();
    copy_4x32_INT8_CM2NN_coverage();
}

test_kupl_za
void copy_32x1_F64_RM2CM_kernel(double *dst, double *src) test_kupl_streaming
{
    constexpr int M = 32;
    constexpr int N = 512;
    auto shape_d = make_shape(Int<M>{}, Int<N>{});
    auto shape_s = make_shape(Int<M>{}, Int<N>{});

    auto stride_d = make_stride(Int<1>{}, Int<32>{});
    auto stride_s = make_stride(Int<N>{}, Int<1>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<1>{}, Int<N / 1>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_32x1_F64_RM2CM>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

void copy_base_example(int test_count)
{
    copy_coverage();
    constexpr int M = 32;
    constexpr int N = 512;
    double *dst_bak = static_cast<double *>(malloc(sizeof(double) * M * N));
    double *src_bak = static_cast<double *>(malloc(sizeof(double) * M * N));
    for (int m = 0; m < M; m++) {
        for (int n = 0; n < N; n++) {
            dst_bak[m * N + n] = 0.0;
            src_bak[m * N + n] = 1.0 * m * N + n;
        }
    }

    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        double *dst = (double*)DT_SetGetFixBlob(&g_Element[cnt++], sizeof(double) * M * N,
                                                sizeof(double) * M * N, (char*)dst_bak);
        double *src = (double*)DT_SetGetFixBlob(&g_Element[cnt++], sizeof(double) * M * N,
                                                sizeof(double) * M * N, (char*)src_bak);

        copy_32x1_F64_RM2CM_kernel(dst, src);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);

    free(src_bak);
    free(dst_bak);
}
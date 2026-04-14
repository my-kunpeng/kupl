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

#if defined(__clang__)
#define test_kupl_za
#define test_kupl_streaming
#elif defined(__GNUC__)
#define test_kupl_za        __arm_new("za")
#define test_kupl_streaming __arm_streaming
#endif

test_kupl_za
void copy_32x1_F64_RM2CM_kernel(double *dst, double *src) test_kupl_streaming
{
    constexpr int M = 32;
    constexpr int K = 512;
    auto shape_d = make_shape(Int<M>{}, Int<K>{});
    auto shape_s = make_shape(Int<M>{}, Int<K>{});

    auto stride_d = make_stride(Int<1>{}, Int<32>{});
    auto stride_s = make_stride(Int<K>{}, Int<1>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<1>{}, Int<K / 1>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_32x1_F64_RM2CM>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

TEST(test_copy, copy_32x1_F64_RM2CM)
{
    constexpr int M = 32;
    constexpr int K = 512;
    double *dst = static_cast<double *>(malloc(sizeof(double) * M * K));
    double *src = static_cast<double *>(malloc(sizeof(double) * M * K));
    for (int m = 0; m < M; m++) {
        for (int k = 0; k < K; k++) {
            dst[m * K + k] = 0.0;
            src[m * K + k] = 1.0 * m * K + k;
        }
    }

    copy_32x1_F64_RM2CM_kernel(dst, src);

    bool res = true;
    for (int k = 0; k < K; k++) {
        for (int m = 0; m < M; m++) {
            if (dst[k * M + m] != 1.0 * m * K + k) {
                res = false;
            }
        }
    }
    ASSERT_TRUE(res);

    free(src);
    free(dst);
}

test_kupl_za
void copy_1x16_F64_CM2RM_kernel(double *dst, double *src) test_kupl_streaming
{
    constexpr int K = 512;
    constexpr int N = 16;
    auto shape_d = make_shape(Int<K>{}, Int<N>{});
    auto shape_s = make_shape(Int<K>{}, Int<N>{});

    auto stride_d = make_stride(Int<16>{}, Int<1>{});
    auto stride_s = make_stride(Int<1>{}, Int<K>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<K / 1>{}, Int<1>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_1x16_F64_CM2RM>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

TEST(test_copy, copy_1x16_F64_CM2RM)
{
    constexpr int K = 512;
    constexpr int N = 16;
    double *dst = static_cast<double *>(malloc(sizeof(double) * K * N));
    double *src = static_cast<double *>(malloc(sizeof(double) * K * N));
    for (int n = 0; n < N; n++) {
        for (int k = 0; k < K; k++) {
            dst[n * K + k] = 0.0;
            src[n * K + k] = 1.0 * n * K + k;
        }
    }

    copy_1x16_F64_CM2RM_kernel(dst, src);

    bool res = true;
    for (int k = 0; k < K; k++) {
        for (int n = 0; n < N; n++) {
            if (dst[k * N + n] != 1.0 * n * K + k) {
                res = false;
            }
        }
    }
    ASSERT_TRUE(res);

    free(src);
    free(dst);
}

#if defined(__clang__)
bfloat16_t float_to_bf16_arm_copy(float x) {
    return (bfloat16_t)x;
}
float bf16_to_float_arm_copy(bfloat16_t x) {
    return (float)x;
}
#elif defined(__GNUC__)
bfloat16_t float_to_bf16_arm_copy(float x) {
    return vcvth_bf16_f32(x);
}
float bf16_to_float_arm_copy(bfloat16_t x) {
    return vcvtah_f32_bf16(x);
}
#endif

test_kupl_za
void copy_16x2_BF16_RM2ZZ_kernel(bfloat16_t *dst, bfloat16_t *src) test_kupl_streaming
{
    constexpr int M = 16;
    constexpr int K = 576;
    auto shape_d = make_shape(Int<M>{}, make_shape(Int<2>{}, Int<K / 2>{}));
    auto shape_s = make_shape(Int<M>{}, Int<K>{});

    auto stride_d = make_stride(Int<2>{}, make_stride(Int<1>{}, Int<32>{}));
    auto stride_s = make_stride(Int<K>{}, Int<1>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<1>{}, Int<K / 2>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_16x2_BF16_RM2ZZ>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

TEST(test_copy, copy_16x2_BF16_RM2ZZ)
{
    constexpr int M = 16;
    constexpr int K = 576;
    bfloat16_t *dst = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * M * K));
    bfloat16_t *src = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * M * K));
    for (int m = 0; m < M; m++) {
        for (int k = 0; k < K; k++) {
            dst[m * K + k] = float_to_bf16_arm_copy(0.0);
            src[m * K + k] = float_to_bf16_arm_copy(1.0 * ((m * K + k) % 100));
        }
    }

    copy_16x2_BF16_RM2ZZ_kernel(dst, src);

    bool res = true;
    for (int tile_k = 0; tile_k < K; tile_k += 2) {
        for (int m = 0; m < M; m++) {
            for (int k = tile_k; k < tile_k + 2; k++) {
                res = (bf16_to_float_arm_copy(dst[tile_k * M + m * 2 + k - tile_k]) != 1.0 * ((m * K + k) % 100)) ? false : true;
            }
        }
    }
    ASSERT_TRUE(res);

    free(src);
    free(dst);
}

test_kupl_za
void copy_2x64_BF16_CM2NN_kernel(bfloat16_t *dst, bfloat16_t *src) test_kupl_streaming
{
    constexpr int K = 576;
    constexpr int N = 64;
    auto shape_d = make_shape(make_shape(Int<2>{}, Int<K / 2>{}), Int<N>{});
    auto shape_s = make_shape(Int<K>{}, Int<N>{});

    auto stride_d = make_stride(make_stride(Int<1>{}, Int<128>{}), Int<2>{});
    auto stride_s = make_stride(Int<1>{}, Int<K>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<K / 2>{}, Int<1>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_2x64_BF16_CM2NN>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

TEST(test_copy, copy_2x64_BF16_CM2NN)
{
    constexpr int K = 576;
    constexpr int N = 64;
    bfloat16_t *dst = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * K * N));
    bfloat16_t *src = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * K * N));
    for (int n = 0; n < N; n++) {
        for (int k = 0; k < K; k++) {
            dst[n * K + k] = float_to_bf16_arm_copy(0.0);
            src[n * K + k] = float_to_bf16_arm_copy(1.0 * ((n * K + k) % 100));
        }
    }

    copy_2x64_BF16_CM2NN_kernel(dst, src);

    bool res = true;
    for (int tile_k = 0; tile_k < K; tile_k += 2) {
        for (int n = 0; n < N; n++) {
            for (int k = tile_k; k < tile_k + 2; k++) {
                res = (bf16_to_float_arm_copy(dst[tile_k * N + n * 2 + k - tile_k]) != 1.0 * ((n * K + k) % 100)) ? false : true;
            }
        }
    }
    ASSERT_TRUE(res);

    free(src);
    free(dst);
}

test_kupl_za
void copy_16x1_BF16_RM2CM_kernel(bfloat16_t *dst, bfloat16_t *src) test_kupl_streaming
{
    constexpr int M = 16;
    constexpr int K = 576;
    auto shape_d = make_shape(Int<M>{}, Int<K>{});
    auto shape_s = make_shape(Int<M>{}, Int<K>{});

    auto stride_d = make_stride(Int<1>{}, Int<16>{});
    auto stride_s = make_stride(Int<K>{}, Int<1>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<1>{}, Int<K>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_16x1_BF16_RM2CM>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

TEST(test_copy, copy_16x1_BF16_RM2CM)
{
    constexpr int M = 16;
    constexpr int K = 576;
    bfloat16_t *dst = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * M * K));
    bfloat16_t *src = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * M * K));
    for (int m = 0; m < M; m++) {
        for (int k = 0; k < K; k++) {
            dst[m * K + k] = float_to_bf16_arm_copy(0.0);
            src[m * K + k] = float_to_bf16_arm_copy(1.0 * ((m * K + k) % 100));
        }
    }

    copy_16x1_BF16_RM2CM_kernel(dst, src);

    bool res = true;
    for (int k = 0; k < K; k++) {
        for (int m = 0; m < M; m++) {
            if (bf16_to_float_arm_copy(dst[k * M + m]) != 1.0 * ((m * K + k) % 100)) {
                res = false;
            }
        }
    }
    ASSERT_TRUE(res);

    free(src);
    free(dst);
}

test_kupl_za
void copy_1x64_BF16_CM2RM_kernel(bfloat16_t *dst, bfloat16_t *src) test_kupl_streaming
{
    constexpr int K = 576;
    constexpr int N = 64;
    auto shape_d = make_shape(Int<K>{}, Int<N>{});
    auto shape_s = make_shape(Int<K>{}, Int<N>{});

    auto stride_d = make_stride(Int<64>{}, Int<1>{});
    auto stride_s = make_stride(Int<1>{}, Int<K>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<K>{}, Int<1>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_1x64_BF16_CM2RM>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

TEST(test_copy, copy_1x64_BF16_CM2RM)
{
    constexpr int K = 576;
    constexpr int N = 64;
    bfloat16_t *dst = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * K * N));
    bfloat16_t *src = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t) * K * N));
    for (int n = 0; n < N; n++) {
        for (int k = 0; k < K; k++) {
            dst[n * K + k] = float_to_bf16_arm_copy(0.0);
            src[n * K + k] = float_to_bf16_arm_copy(1.0 * ((n * K + k) % 100));
        }
    }

    copy_1x64_BF16_CM2RM_kernel(dst, src);

    bool res = true;
    for (int k = 0; k < K; k++) {
        for (int n = 0; n < N; n++) {
            if (bf16_to_float_arm_copy(dst[k * N + n]) != 1.0 * ((n * K + k) % 100)) {
                res = false;
            }
        }
    }
    ASSERT_TRUE(res);

    free(src);
    free(dst);
}

test_kupl_za
void copy_16x4_INT8_RM2ZZ_kernel(int8_t *dst, int8_t *src) test_kupl_streaming
{
    constexpr int M = 16;
    constexpr int K = 576;
    auto shape_d = make_shape(Int<M>{}, make_shape(Int<4>{}, Int<K / 4>{}));
    auto shape_s = make_shape(Int<M>{}, Int<K>{});

    auto stride_d = make_stride(Int<4>{}, make_stride(Int<1>{}, Int<64>{}));
    auto stride_s = make_stride(Int<K>{}, Int<1>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<1>{}, Int<K / 4>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_16x4_INT8_RM2ZZ>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

TEST(test_copy, copy_16x4_INT8_RM2ZZ)
{
    constexpr int M = 16;
    constexpr int K = 576;
    int8_t *dst = static_cast<int8_t *>(malloc(sizeof(int8_t) * M * K));
    int8_t *src = static_cast<int8_t *>(malloc(sizeof(int8_t) * M * K));
    for (int m = 0; m < M; m++) {
        for (int k = 0; k < K; k++) {
            dst[m * K + k] = 0.0;
            src[m * K + k] = 1.0 * ((m * K + k) % 100);
        }
    }

    copy_16x4_INT8_RM2ZZ_kernel(dst, src);

    bool res = true;
    for (int tile_k = 0; tile_k < K; tile_k += 4) {
        for (int m = 0; m < M; m++) {
            for (int k = tile_k; k < tile_k + 4; k++) {
                res = (dst[tile_k * M + m * 4 + k - tile_k] != 1.0 * ((m * K + k) % 100)) ? false : true;
            }
        }
    }
    ASSERT_TRUE(res);

    free(src);
    free(dst);
}

test_kupl_za
void copy_4x64_INT8_CM2NN_kernel(int8_t *dst, int8_t *src) test_kupl_streaming
{
    constexpr int K = 576;
    constexpr int N = 64;

    auto shape_d = make_shape(make_shape(Int<4>{}, Int<K / 4>{}), Int<N>{});
    auto shape_s = make_shape(Int<K>{}, Int<N>{});

    auto stride_d = make_stride(make_stride(Int<1>{}, Int<256>{}), Int<4>{});
    auto stride_s = make_stride(Int<1>{}, Int<K>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<K / 4>{}, Int<1>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_4x64_INT8_CM2NN>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

TEST(test_copy, copy_4x64_INT8_CM2NN)
{
    constexpr int K = 576;
    constexpr int N = 64;
    int8_t *dst = static_cast<int8_t *>(malloc(sizeof(int8_t) * K * N));
    int8_t *src = static_cast<int8_t *>(malloc(sizeof(int8_t) * K * N));
    for (int n = 0; n < N; n++) {
        for (int k = 0; k < K; k++) {
            dst[n * K + k] = 0.0;
            src[n * K + k] = 1.0 * ((n * K + k) % 100);
        }
    }

    copy_4x64_INT8_CM2NN_kernel(dst, src);

    bool res = true;
    for (int tile_k = 0; tile_k < K; tile_k += 4) {
        for (int n = 0; n < N; n++) {
            for (int k = tile_k; k < tile_k + 4; k++) {
                res = (dst[tile_k * N + n * 4 + k - tile_k] != 1.0 * ((n * K + k) % 100)) ? false : true;
            }
        }
    }
    ASSERT_TRUE(res);

    free(src);
    free(dst);
}

test_kupl_za
void copy_32x4_INT8_RM2ZZ_kernel(int8_t *dst, int8_t *src) test_kupl_streaming
{
    constexpr int M = 32;
    constexpr int K = 576;
    auto shape_d = make_shape(Int<M>{}, make_shape(Int<4>{}, Int<K / 4>{}));
    auto shape_s = make_shape(Int<M>{}, Int<K>{});

    auto stride_d = make_stride(Int<4>{}, make_stride(Int<1>{}, Int<128>{}));
    auto stride_s = make_stride(Int<K>{}, Int<1>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<1>{}, Int<K / 4>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_32x4_INT8_RM2ZZ>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

TEST(test_copy, copy_32x4_INT8_RM2ZZ)
{
    constexpr int M = 32;
    constexpr int K = 576;
    int8_t *dst = static_cast<int8_t *>(malloc(sizeof(int8_t) * M * K));
    int8_t *src = static_cast<int8_t *>(malloc(sizeof(int8_t) * M * K));
    for (int m = 0; m < M; m++) {
        for (int k = 0; k < K; k++) {
            dst[m * K + k] = 0.0;
            src[m * K + k] = 1.0 * ((m * K + k) % 100);
        }
    }

    copy_32x4_INT8_RM2ZZ_kernel(dst, src);

    bool res = true;
    for (int tile_k = 0; tile_k < K; tile_k += 4) {
        for (int m = 0; m < M; m++) {
            for (int k = tile_k; k < tile_k + 4; k++) {
                res = (dst[tile_k * M + m * 4 + k - tile_k] != 1.0 * ((m * K + k) % 100)) ? false : true;
            }
        }
    }
    ASSERT_TRUE(res);

    free(src);
    free(dst);
}

test_kupl_za
void copy_4x32_INT8_CM2NN_kernel(int8_t *dst, int8_t *src) test_kupl_streaming
{
    constexpr int K = 576;
    constexpr int N = 32;
    auto shape_d = make_shape(make_shape(Int<4>{}, Int<K / 4>{}), Int<N>{});
    auto shape_s = make_shape(Int<K>{}, Int<N>{});

    auto stride_d = make_stride(make_stride(Int<1>{}, Int<128>{}), Int<4>{});
    auto stride_s = make_stride(Int<1>{}, Int<K>{});

    auto layout_d = make_layout(shape_d, stride_d);
    auto layout_s = make_layout(shape_s, stride_s);

    auto atom_copy_shape = make_shape(Int<K / 4>{}, Int<1>{});
    auto tiled_copy = make_tiled_copy(Ops<COPY_4x32_INT8_CM2NN>{}, atom_copy_shape);

    auto tensor_d = make_tensor(dst, layout_d);
    auto tensor_s = make_tensor(src, layout_s);
    tensor_tiled_copy(tiled_copy, tensor_d, tensor_s);
}

TEST(test_copy, copy_4x32_INT8_CM2NN)
{
    constexpr int K = 576;
    constexpr int N = 32;
    int8_t *dst = static_cast<int8_t *>(malloc(sizeof(int8_t) * K * N));
    int8_t *src = static_cast<int8_t *>(malloc(sizeof(int8_t) * K * N));
    for (int n = 0; n < N; n++) {
        for (int k = 0; k < K; k++) {
            dst[n * K + k] = 0.0;
            src[n * K + k] = 1.0 * ((n * K + k) % 100);
        }
    }

    copy_4x32_INT8_CM2NN_kernel(dst, src);

    bool res = true;
    for (int tile_k = 0; tile_k < K; tile_k += 4) {
        for (int n = 0; n < N; n++) {
            for (int k = tile_k; k < tile_k + 4; k++) {
                res = (dst[tile_k * N + n * 4 + k - tile_k] != 1.0 * ((n * K + k) % 100)) ? false : true;
            }
        }
    }
    ASSERT_TRUE(res);

    free(src);
    free(dst);
}

TEST(test_copy, KP36_PREFETCH_L1)
{
    auto atom_prefetch_shape = make_shape(Int<1>{});
    auto tiled_prefetch_L1 = make_tiled_copy(Ops<KP36_PREFETCH_L1>{}, atom_prefetch_shape);

    // double
    double *buf_ds = static_cast<double *>(malloc(sizeof(double *) * 8));
    auto shape_ds = make_shape(Int<8>{});
    auto stride_ds = make_stride(Int<1>{});
    auto layout_ds = make_layout(shape_ds, stride_ds);
    auto tensor_ds = make_tensor(buf_ds, layout_ds);
    tensor_tiled_copy(tiled_prefetch_L1, tensor_ds);
    free(buf_ds);

    // bfloat16_t
    bfloat16_t *buf_bs = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t *) * 32));
    auto shape_bs = make_shape(Int<32>{});
    auto stride_bs = make_stride(Int<1>{});
    auto layout_bs = make_layout(shape_bs, stride_bs);
    auto tensor_bs = make_tensor(buf_bs, layout_bs);
    tensor_tiled_copy(tiled_prefetch_L1, tensor_bs);
    free(buf_bs);

    // int8_t
    int8_t *buf_is = static_cast<int8_t *>(malloc(sizeof(int8_t *) * 64));
    auto shape_is = make_shape(Int<64>{});
    auto stride_is = make_stride(Int<1>{});
    auto layout_is = make_layout(shape_is, stride_is);
    auto tensor_is = make_tensor(buf_is, layout_is);
    tensor_tiled_copy(tiled_prefetch_L1, tensor_is);
    free(buf_is);
}

TEST(test_copy, KP36_PREFETCH_L2)
{
    auto atom_prefetch_shape = make_shape(Int<1>{});
    auto tiled_prefetch_L2 = make_tiled_copy(Ops<KP36_PREFETCH_L2>{}, atom_prefetch_shape);

    // double
    double *buf_ds = static_cast<double *>(malloc(sizeof(double *) * 8));
    auto shape_ds = make_shape(Int<8>{});
    auto stride_ds = make_stride(Int<1>{});
    auto layout_ds = make_layout(shape_ds, stride_ds);
    auto tensor_ds = make_tensor(buf_ds, layout_ds);
    tensor_tiled_copy(tiled_prefetch_L2, tensor_ds);
    free(buf_ds);

    // bfloat16_t
    bfloat16_t *buf_bs = static_cast<bfloat16_t *>(malloc(sizeof(bfloat16_t *) * 32));
    auto shape_bs = make_shape(Int<32>{});
    auto stride_bs = make_stride(Int<1>{});
    auto layout_bs = make_layout(shape_bs, stride_bs);
    auto tensor_bs = make_tensor(buf_bs, layout_bs);
    tensor_tiled_copy(tiled_prefetch_L2, tensor_bs);
    free(buf_bs);

    // int8_t
    int8_t *buf_is = static_cast<int8_t *>(malloc(sizeof(int8_t *) * 64));
    auto shape_is = make_shape(Int<64>{});
    auto stride_is = make_stride(Int<1>{});
    auto layout_is = make_layout(shape_is, stride_is);
    auto tensor_is = make_tensor(buf_is, layout_is);
    tensor_tiled_copy(tiled_prefetch_L2, tensor_is);
    free(buf_is);
}

#endif
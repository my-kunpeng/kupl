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

#include <arm_sme.h>
#include <arm_sve.h>
#include <cstdint>
#include "kupl_mma.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/debug/kupl_log.h"
#include "utils/sys/kupl_hardware.h"
using namespace kupl::tensor;

static kupl_arch_type_t arch_type;

__attribute__((constructor(102)))
static void kupl_mma_arch_detect()
{
    arch_type = kupl_arch_detect();
}

#if !defined(__clang__)
/*
 * Due to the conflict between gcc matrix computation and -Wstack-usage,
 * the -Wstack-usage=32768 compilation option was removed
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage=32768"
#endif

static kupl_always_inline
bool mma_check(void *data_a, void *data_b, void *data_c)
{
    if (kupl_unlikely(arch_type != KUPL_CPU_HISILICOM_920F)) {
        kupl_error("The KUPL mma feature cannot be used in environments without Matrix_computation capability");
        return false;
    }
    if (kupl_unlikely(data_a == nullptr || data_b == nullptr || data_c == nullptr)) {
        kupl_error("The original ptr for KUPL mma matrix is nullptr");
        return false;
    }
    return true;
}

#define kupl_export __attribute__((visibility("default")))

#if defined(__clang__)
#define MATRIX_COMP_ON()                                                                                \
do {                                                                                            \
    __asm__ volatile("SMSTART":::"z0", "z1", "z2", "z3", "z4", "z5", "z6", "z7",                \
                                 "z8", "z9", "z10", "z11", "z12", "z13", "z14", "z15",          \
                                 "z16", "z17", "z18", "z19", "z20", "z21", "z22", "z23",        \
                                 "z24", "z25", "z26", "z27", "z28", "z29", "z30", "z31",        \
                                 "p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7",                \
                                 "p8", "p9", "p10", "p11", "p12", "p13", "p14", "p15");         \
    __asm__ volatile("ISB");                                                                    \
} while (0)
#define MATRIX_COMP_OFF()                                                                               \
do {                                                                                            \
    __asm__ volatile("SMSTOP":::"z0", "z1", "z2", "z3", "z4", "z5", "z6", "z7",                 \
                                "z8", "z9", "z10", "z11", "z12", "z13", "z14", "z15",           \
                                "z16", "z17", "z18", "z19", "z20", "z21", "z22", "z23",         \
                                "z24", "z25", "z26", "z27", "z28", "z29", "z30", "z31",         \
                                "p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7",                 \
                                "p8", "p9", "p10", "p11", "p12", "p13", "p14", "p15");          \
    __asm__ volatile("ISB");                                                                    \
} while (0)
#elif defined(__GNUC__)
#define MATRIX_COMP_ON()
#define MATRIX_COMP_OFF()
#endif

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_mma<32, 16,
                                        Stride<Int<1>, Int<32>>, Stride<Int<16>, Int<1>>, Stride<Int<16>, Int<1>>,
                                        double, double, double>
                                        (double *data_a, double *data_b, double *data_c, int size_k) MMA_INOUT
{
    if (kupl_unlikely(mma_check(data_a, data_b, data_c) == false)) {
        return;
    }
    MATRIX_COMP_ON();
    svbool_t p64 = svwhilelt_b64(0, 8);
    svfloat64_t vc0;
    svfloat64_t vc1;
    svfloat64_t vc2;
    svfloat64_t vc3;
    svfloat64_t vc4;
    svfloat64_t vc5;
    svfloat64_t vc6;
    svfloat64_t vc7;
    double *matc0 = data_c;
    double *matc1 = data_c + 128;
    double *matc2 = data_c + 256;
    double *matc3 = data_c + 384;
    for (uint32_t t = 0; t < 8; t++) {
        vc0 = svld1_f64(p64, matc0);
        svwrite_hor_za64_m(0, t, p64, vc0);
        vc1 = svld1_f64(p64, matc0 + 8);
        svwrite_hor_za64_m(4, t, p64, vc1);
        vc2 = svld1_f64(p64, matc1);
        svwrite_hor_za64_m(1, t, p64, vc2);
        vc3 = svld1_f64(p64, matc1 + 8);
        svwrite_hor_za64_m(5, t, p64, vc3);
        vc4 = svld1_f64(p64, matc2);
        svwrite_hor_za64_m(2, t, p64, vc4);
        vc5 = svld1_f64(p64, matc2 + 8);
        svwrite_hor_za64_m(6, t, p64, vc5);
        vc6 = svld1_f64(p64, matc3);
        svwrite_hor_za64_m(3, t, p64, vc6);
        vc7 = svld1_f64(p64, matc3 + 8);
        svwrite_hor_za64_m(7, t, p64, vc7);
        matc0 += 16;        // ldm_c
        matc1 += 16;
        matc2 += 16;
        matc3 += 16;
    }
    double *data_atmp = data_a;
    double *data_btmp = data_b;
    for (int i = 0; i < size_k; ++i) {
        vc0 = svld1_f64(p64, data_atmp);
        vc4 = svld1_f64(p64, data_btmp);
        svmopa_za64_f64_m(0, p64, p64, vc0, vc4);
        vc1 = svld1_f64(p64, data_atmp + 8);
        svmopa_za64_f64_m(1, p64, p64, vc1, vc4);
        vc2 = svld1_f64(p64, data_atmp + 16);
        svmopa_za64_f64_m(2, p64, p64, vc2, vc4);
        vc3 = svld1_f64(p64, data_atmp + 24);
        svmopa_za64_f64_m(3, p64, p64, vc3, vc4);
        vc5 = svld1_f64(p64, data_btmp + 8);
        svmopa_za64_f64_m(4, p64, p64, vc0, vc5);
        svmopa_za64_f64_m(5, p64, p64, vc1, vc5);
        svmopa_za64_f64_m(6, p64, p64, vc2, vc5);
        svmopa_za64_f64_m(7, p64, p64, vc3, vc5);

        data_atmp += 32;    // ldm_a;
        data_btmp += 16;    // ldm_b;
    }
}

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_mma<16, 64,
                                        Stride<Int<2>, Stride<Int<1>, Int<32>>>,
                                        Stride<Stride<Int<1>, Int<128>>, Int<2>>,
                                        Stride<Int<64>, Int<1>>,
                                        bfloat16_t, bfloat16_t, float>
                                        (bfloat16_t *data_a, bfloat16_t *data_b, float *data_c, int size_k) MMA_INOUT
{
    if (kupl_unlikely(mma_check(data_a, data_b, data_c) == false)) {
        return;
    }
    MATRIX_COMP_ON();
    svbool_t p16 = svwhilelt_b16(0, 32);
    svbool_t p32 = svwhilelt_b32(0, 16);
    svfloat32_t vc32_0;
    svfloat32_t vc32_1;
    svfloat32_t vc32_2;
    svfloat32_t vc32_3;
    float *matc0 = data_c;
    float *matc1 = data_c + 16;
    float *matc2 = data_c + 32;
    float *matc3 = data_c + 48;
    for (uint32_t t = 0; t < 16; t++) {
        vc32_0 = svld1_f32(p32, matc0);
        svwrite_hor_za32_m(0, t, p32, vc32_0);
        vc32_1 = svld1_f32(p32, matc1);
        svwrite_hor_za32_m(1, t, p32, vc32_1);
        vc32_2 = svld1_f32(p32, matc2);
        svwrite_hor_za32_m(2, t, p32, vc32_2);
        vc32_3 = svld1_f32(p32, matc3);
        svwrite_hor_za32_m(3, t, p32, vc32_3);

        matc0 += 64;
        matc1 += 64;
        matc2 += 64;
        matc3 += 64;
    }
    bfloat16_t *data_atmp = data_a;
    bfloat16_t *data_btmp = data_b;
    svbfloat16_t va0;
    svbfloat16_t vb0;
    svbfloat16_t vb1;
    svbfloat16_t vb2;
    svbfloat16_t vb3;
    for (int i = 0; i < size_k / 2; i++) {
        va0 = svld1_bf16(p16, data_atmp);
        vb0 = svld1_bf16(p16, data_btmp);
        svmopa_za32_bf16_m(0, p16, p16, va0, vb0);
        vb1 = svld1_bf16(p16, data_btmp + 32);
        svmopa_za32_bf16_m(1, p16, p16, va0, vb1);
        vb2 = svld1_bf16(p16, data_btmp + 64);
        svmopa_za32_bf16_m(2, p16, p16, va0, vb2);
        vb3 = svld1_bf16(p16, data_btmp + 96);
        svmopa_za32_bf16_m(3, p16, p16, va0, vb3);

        data_atmp += 32;
        data_btmp += 128;
    }
}

#if defined(__clang__)
static kupl_always_inline
bfloat16_t float_to_bf16_arm(float x)
{
    return (bfloat16_t)x;
}
#elif defined(__GNUC__)
bfloat16_t float_to_bf16_arm(float x)
{
    return vcvth_bf16_f32(x);
}
#endif

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_mma<16, 64,
                                        Stride<Int<1>, Int<16>>, Stride<Int<64>, Int<1>>, Stride<Int<64>, Int<1>>,
                                        bfloat16_t, bfloat16_t, float>
                                        (bfloat16_t *data_a, bfloat16_t *data_b, float *data_c, int size_k) MMA_INOUT
{
    if (kupl_unlikely(mma_check(data_a, data_b, data_c) == false)) {
        return;
    }
    MATRIX_COMP_ON();
    svbool_t p16_16 = svwhilelt_b16(0, 16);
    svbool_t p16 = svwhilelt_b16(0, 32);
    svbool_t p32 = svwhilelt_b32(0, 16);
    svfloat32_t vc32_0;
    svfloat32_t vc32_1;
    svfloat32_t vc32_2;
    svfloat32_t vc32_3;
    float *matc0 = data_c;
    float *matc1 = data_c + 16;
    float *matc2 = data_c + 32;
    float *matc3 = data_c + 48;
    for (uint32_t t = 0; t < 16; t++) {
        vc32_0 = svld1_f32(p32, matc0);
        svwrite_hor_za32_m(0, t, p32, vc32_0);
        vc32_1 = svld1_f32(p32, matc1);
        svwrite_hor_za32_m(1, t, p32, vc32_1);
        vc32_2 = svld1_f32(p32, matc2);
        svwrite_hor_za32_m(2, t, p32, vc32_2);
        vc32_3 = svld1_f32(p32, matc3);
        svwrite_hor_za32_m(3, t, p32, vc32_3);

        matc0 += 64;
        matc1 += 64;
        matc2 += 64;
        matc3 += 64;
    }
    bfloat16_t zero[16];
    for (int i = 0; i < 16; i++) {
        zero[i] = float_to_bf16_arm(0.0);
    }
    svbfloat16_t vzero = svld1_bf16(p16_16, zero);
    bfloat16_t *data_atmp = data_a;
    bfloat16_t *data_btmp = data_b;
    svbfloat16_t va0_tmp;
    svbfloat16_t vb0_tmp;
    svbfloat16_t vb1_tmp;
    svbfloat16_t vb2_tmp;
    svbfloat16_t vb3_tmp;
    svbfloat16_t va0;
    svbfloat16_t vb0;
    svbfloat16_t vb1;
    svbfloat16_t vb2;
    svbfloat16_t vb3;
    for (int i = 0; i < size_k; i++) {
        va0_tmp = svld1_bf16(p16_16, data_atmp);
        va0 = svzip1_bf16(va0_tmp, vzero);
        vb0_tmp = svld1_bf16(p16_16, data_btmp);
        vb0 = svzip1_bf16(vb0_tmp, vzero);
        svmopa_za32_bf16_m(0, p16, p16, va0, vb0);
        vb1_tmp = svld1_bf16(p16_16, data_btmp + 16);
        vb1 = svzip1_bf16(vb1_tmp, vzero);
        svmopa_za32_bf16_m(1, p16, p16, va0, vb1);
        vb2_tmp = svld1_bf16(p16_16, data_btmp + 32);
        vb2 = svzip1_bf16(vb2_tmp, vzero);
        svmopa_za32_bf16_m(2, p16, p16, va0, vb2);
        vb3_tmp = svld1_bf16(p16_16, data_btmp + 48);
        vb3 = svzip1_bf16(vb3_tmp, vzero);
        svmopa_za32_bf16_m(3, p16, p16, va0, vb3);

        data_atmp += 16;
        data_btmp += 64;
    }
}

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_mma<16, 64,
                                        Stride<Int<4>, Stride<Int<1>, Int<64>>>,
                                        Stride<Stride<Int<1>, Int<256>>, Int<4>>,
                                        Stride<Int<64>, Int<1>>,
                                        int8_t, int8_t, int32_t>
                                        (int8_t *data_a, int8_t *data_b, int32_t *data_c, int size_k) MMA_INOUT
{
    if (kupl_unlikely(mma_check(data_a, data_b, data_c) == false)) {
        return;
    }
    MATRIX_COMP_ON();
    svbool_t p8 = svwhilelt_b8(0, 64);
    svbool_t p32 = svwhilelt_b32(0, 16);
    svint32_t vc32_0;
    svint32_t vc32_1;
    svint32_t vc32_2;
    svint32_t vc32_3;
    int32_t *matc0 = data_c;
    int32_t *matc1 = data_c + 16;
    int32_t *matc2 = data_c + 32;
    int32_t *matc3 = data_c + 48;
    for (uint32_t t = 0; t < 16; t++) {
        vc32_0 = svld1_s32(p32, matc0);
        svwrite_hor_za32_m(0, t, p32, vc32_0);
        vc32_1 = svld1_s32(p32, matc1);
        svwrite_hor_za32_m(1, t, p32, vc32_1);
        vc32_2 = svld1_s32(p32, matc2);
        svwrite_hor_za32_m(2, t, p32, vc32_2);
        vc32_3 = svld1_s32(p32, matc3);
        svwrite_hor_za32_m(3, t, p32, vc32_3);

        matc0 += 64;
        matc1 += 64;
        matc2 += 64;
        matc3 += 64;
    }
    int8_t *data_atmp = data_a;
    int8_t *data_btmp = data_b;
    svint8_t va0;
    svint8_t vb0;
    svint8_t vb1;
    svint8_t vb2;
    svint8_t vb3;
    for (int i = 0; i < size_k / 4; i++) {
        va0 = svld1_s8(p8, data_atmp);
        vb0 = svld1_s8(p8, data_btmp);
        svmopa_za32_s8_m(0, p8, p8, va0, vb0);
        vb1 = svld1_s8(p8, data_btmp + 64);
        svmopa_za32_s8_m(1, p8, p8, va0, vb1);
        vb2 = svld1_s8(p8, data_btmp + 128);
        svmopa_za32_s8_m(2, p8, p8, va0, vb2);
        vb3 = svld1_s8(p8, data_btmp + 192);
        svmopa_za32_s8_m(3, p8, p8, va0, vb3);

        data_atmp += 64;
        data_btmp += 256;
    }
}

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_mma<32, 32,
                                        Stride<Int<4>, Stride<Int<1>, Int<128>>>,
                                        Stride<Stride<Int<1>, Int<128>>, Int<4>>,
                                        Stride<Int<32>, Int<1>>,
                                        int8_t, int8_t, int32_t>
                                        (int8_t *data_a, int8_t *data_b, int32_t *data_c, int size_k) MMA_INOUT
{
    if (kupl_unlikely(mma_check(data_a, data_b, data_c) == false)) {
        return;
    }
    MATRIX_COMP_ON();
    svbool_t p8 = svwhilelt_b8(0, 64);
    svbool_t p32 = svwhilelt_b32(0, 16);
    svint32_t vc32_0;
    svint32_t vc32_1;
    svint32_t vc32_2;
    svint32_t vc32_3;
    int32_t *matc0 = data_c;
    int32_t *matc1 = data_c + 16;
    int32_t *matc2 = data_c + 16 * 32;
    int32_t *matc3 = data_c + 16 * 32 + 16;
    for (uint32_t t = 0; t < 16; t++) {
        vc32_0 = svld1_s32(p32, matc0);
        svwrite_hor_za32_m(0, t, p32, vc32_0);
        vc32_1 = svld1_s32(p32, matc1);
        svwrite_hor_za32_m(1, t, p32, vc32_1);
        vc32_2 = svld1_s32(p32, matc2);
        svwrite_hor_za32_m(2, t, p32, vc32_2);
        vc32_3 = svld1_s32(p32, matc3);
        svwrite_hor_za32_m(3, t, p32, vc32_3);

        matc0 += 32;
        matc1 += 32;
        matc2 += 32;
        matc3 += 32;
    }
    int8_t *data_atmp = data_a;
    int8_t *data_btmp = data_b;
    svint8_t va0;
    svint8_t va1;
    svint8_t vb0;
    svint8_t vb1;
    for (int i = 0; i < size_k / 4; i++) {
        va0 = svld1_s8(p8, data_atmp);
        vb0 = svld1_s8(p8, data_btmp);
        svmopa_za32_s8_m(0, p8, p8, va0, vb0);
        vb1 = svld1_s8(p8, data_btmp + 64);
        svmopa_za32_s8_m(1, p8, p8, va0, vb1);
        va1 = svld1_s8(p8, data_atmp + 64);
        svmopa_za32_s8_m(2, p8, p8, va1, vb0);
        svmopa_za32_s8_m(3, p8, p8, va1, vb1);

        data_atmp += 128;
        data_btmp += 128;
    }
}

static kupl_always_inline
bool store_check(void *data)
{
    if (kupl_unlikely(arch_type != KUPL_CPU_HISILICOM_920F)) {
        kupl_error("The KUPL mma feature cannot be used in environments without Matrix_computation capability");
        return false;
    }
    if (kupl_unlikely(data == nullptr)) {
        kupl_error("The original ptr for KUPL mma store matrix is nullptr");
        return false;
    }
    return true;
}

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_store<32, 16, Stride<Int<16>, Int<1>>, double>(
    double *data) MMA_IN
{
    if (kupl_unlikely(store_check(data) == false)) {
        return;
    }
    svbool_t p64 = svwhilelt_b64(0, 8);
    double *matd0 = data;
    double *matd1 = data + 128;
    double *matd2 = data + 256;
    double *matd3 = data + 384;
    for (uint32_t t = 0; t < 8; t++) {
        svst1_hor_za64(0, t, p64, matd0);
        svst1_hor_za64(4, t, p64, matd0 + 8);
        svst1_hor_za64(1, t, p64, matd1);
        svst1_hor_za64(5, t, p64, matd1 + 8);
        svst1_hor_za64(2, t, p64, matd2);
        svst1_hor_za64(6, t, p64, matd2 + 8);
        svst1_hor_za64(3, t, p64, matd3);
        svst1_hor_za64(7, t, p64, matd3 + 8);
        matd0 += 16;
        matd1 += 16;
        matd2 += 16;
        matd3 += 16;
    }
    MATRIX_COMP_OFF();
}

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_store<16, 64, Stride<Int<64>, Int<1>>, float>(
    float *data) MMA_IN
{
    if (kupl_unlikely(store_check(data) == false)) {
        return;
    }
    svbool_t p32 = svwhilelt_b32(0, 16);
    float *matd0 = data;
    float *matd1 = data + 16;
    float *matd2 = data + 32;
    float *matd3 = data + 48;
    for (uint32_t t = 0; t < 16; t++) {
        svst1_hor_za32(0, t, p32, matd0);
        svst1_hor_za32(1, t, p32, matd1);
        svst1_hor_za32(2, t, p32, matd2);
        svst1_hor_za32(3, t, p32, matd3);
        matd0 += 64;
        matd1 += 64;
        matd2 += 64;
        matd3 += 64;
    }
    MATRIX_COMP_OFF();
}

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_store<16, 64, Stride<Int<64>, Int<1>>, int32_t>(
    int32_t *data) MMA_IN
{
    if (kupl_unlikely(store_check(data) == false)) {
        return;
    }
    svbool_t p32 = svwhilelt_b32(0, 16);
    int32_t *matd0 = data;
    int32_t *matd1 = data + 16;
    int32_t *matd2 = data + 32;
    int32_t *matd3 = data + 48;
    for (uint32_t t = 0; t < 16; t++) {
        svst1_hor_za32(0, t, p32, matd0);
        svst1_hor_za32(1, t, p32, matd1);
        svst1_hor_za32(2, t, p32, matd2);
        svst1_hor_za32(3, t, p32, matd3);
        matd0 += 64;
        matd1 += 64;
        matd2 += 64;
        matd3 += 64;
    }
    MATRIX_COMP_OFF();
}

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_store<32, 32, Stride<Int<32>, Int<1>>, int32_t>(
    int32_t *data) MMA_IN
{
    if (kupl_unlikely(store_check(data) == false)) {
        return;
    }
    svbool_t p32 = svwhilelt_b32(0, 16);
    int32_t *matd0 = data;
    int32_t *matd1 = data + 16;
    int32_t *matd2 = data + 16 * 32;
    int32_t *matd3 = data + 16 * 32 + 16;
    for (uint32_t t = 0; t < 16; t++) {
        svst1_hor_za32(0, t, p32, matd0);
        svst1_hor_za32(1, t, p32, matd1);
        svst1_hor_za32(2, t, p32, matd2);
        svst1_hor_za32(3, t, p32, matd3);
        matd0 += 32;
        matd1 += 32;
        matd2 += 32;
        matd3 += 32;
    }
    MATRIX_COMP_OFF();
}

static kupl_always_inline
bool copy_check(void *data_dst, void *data_src, int size_m, int size_n)
{
    if (kupl_unlikely(arch_type != KUPL_CPU_HISILICOM_920F)) {
        kupl_error("The KUPL mma feature cannot be used in environments without Matrix_computation capability");
        return false;
    }
    if (kupl_unlikely(data_dst == nullptr || data_src == nullptr)) {
        kupl_error("The dst or src ptr for KUPL matrix copy is nullptr");
        return false;
    }
    if (kupl_unlikely(size_m <= 0 || size_n <= 0)) {
        kupl_error("The size of dst or src matrix is illegal");
        return false;
    }
    return true;
}

static constexpr int TILE_0 = 0;
static constexpr int TILE_1 = 1;
static constexpr int TILE_2 = 2;
static constexpr int TILE_3 = 3;
static constexpr int F64_TILE_8 = 8;
static constexpr int F64_TILE_8X2 = 16;
static constexpr int F64_TILE_8X3 = 24;

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_copy<Ops<COPY_32x1_F64_RM2CM>, double, double>(
    double *data_dst, double *data_src, int size_m, int size_n) MMA_IN
{
    if (kupl_unlikely(copy_check(data_dst, data_src, size_m, size_n) == false)) {
        return;
    }
    if (kupl_unlikely(size_n % F64_TILE_8)) {    // 向量寄存器的宽度为512，对于F64精度数据而言一个向量寄存器可以存放8个F64数据
        kupl_error("The KUPL copy atom COPY_32x1_F64_RM2CM now can "
                   "only support scenarios where n is a multiple of 8");
        return;
    }
    MATRIX_COMP_ON();
    svbool_t p = svwhilelt_b64(0, F64_TILE_8);
    for (int tile_m = 0; tile_m < size_m; tile_m += M_32) {
        for (int tile_n = 0; tile_n < size_n ; tile_n += F64_TILE_8) {
            // 横着写入向量寄存器
            double *data_src0 = data_src + tile_m * size_n + tile_n;
            double *data_src1 = data_src + (tile_m + F64_TILE_8) * size_n + tile_n;
            double *data_src2 = data_src + (tile_m + F64_TILE_8X2) * size_n + tile_n;
            double *data_src3 = data_src + (tile_m + F64_TILE_8X3) * size_n + tile_n;
            for (uint32_t t = 0; t < F64_TILE_8; t++) {
                svld1_hor_za64(TILE_0, t, p, data_src0);
                svld1_hor_za64(TILE_1, t, p, data_src1);
                svld1_hor_za64(TILE_2, t, p, data_src2);
                svld1_hor_za64(TILE_3, t, p, data_src3);
                data_src0 += size_n;
                data_src1 += size_n;
                data_src2 += size_n;
                data_src3 += size_n;
            }

            // 竖着写回buffer
            double *data_dst0 = data_dst + tile_m * size_n + tile_n * M_32;
            double *data_dst1 = data_dst + tile_m * size_n + tile_n * M_32 + F64_TILE_8;
            double *data_dst2 = data_dst + tile_m * size_n + tile_n * M_32 + F64_TILE_8X2;
            double *data_dst3 = data_dst + tile_m * size_n + tile_n * M_32 + F64_TILE_8X3;
            for (uint32_t t = 0; t < F64_TILE_8; t++) {
                svst1_ver_za64(TILE_0, t, p, data_dst0);
                svst1_ver_za64(TILE_1, t, p, data_dst1);
                svst1_ver_za64(TILE_2, t, p, data_dst2);
                svst1_ver_za64(TILE_3, t, p, data_dst3);
                data_dst0 += M_32;
                data_dst1 += M_32;
                data_dst2 += M_32;
                data_dst3 += M_32;
            }
        }
    }
    MATRIX_COMP_OFF();
}

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_copy<Ops<COPY_1x16_F64_CM2RM>, double, double>(
    double *data_dst, double *data_src, int size_m, int size_n) MMA_IN
{
    if (kupl_unlikely(copy_check(data_dst, data_src, size_m, size_n) == false)) {
        return;
    }
    if (kupl_unlikely(size_m % F64_TILE_8)) {    // 向量寄存器的宽度为512，对于F64精度数据而言一个向量寄存器可以存放8个F64数据
        kupl_error("The KUPL copy atom COPY_1x16_F64_CM2RM now can "
                   "only support scenarios where m is a multiple of 8");
        return;
    }
    MATRIX_COMP_ON();
    svbool_t p = svwhilelt_b64(0, F64_TILE_8);
    for (int tile_n = 0; tile_n < size_n; tile_n += N_16) {
        for (int tile_m = 0; tile_m < size_m; tile_m += F64_TILE_8) {
            // 横着写入向量寄存器
            double *data_src0 = data_src + tile_n * size_m + tile_m;
            double *data_src1 = data_src + (tile_n + F64_TILE_8) * size_m + tile_m;
            for (uint32_t t = 0; t < F64_TILE_8; t++) {
                svld1_hor_za64(TILE_0, t, p, data_src0);
                svld1_hor_za64(TILE_1, t, p, data_src1);
                data_src0 += size_m;
                data_src1 += size_m;
            }

            // 竖着写回buffer
            double *data_dst0 = data_dst + tile_n * size_m + tile_m * N_16;
            double *data_dst1 = data_dst + tile_n * size_m + tile_m * N_16 + F64_TILE_8;
            for (uint32_t t = 0; t < F64_TILE_8; t++) {
                svst1_ver_za64(TILE_0, t, p, data_dst0);
                svst1_ver_za64(TILE_1, t, p, data_dst1);
                data_dst0 += N_16;
                data_dst1 += N_16;
            }
        }
    }
    MATRIX_COMP_OFF();
}

static constexpr int BF16_TILE_32 = 32;
static constexpr int BF16_TILE_32X2 = 64;
static constexpr int BF16_TILE_32X3 = 96;
static constexpr int F32_TILE_16 = 16;
static constexpr int F32_TILE_16X2 = 32;
static constexpr int F32_TILE_16X3 = 48;

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_copy<Ops<COPY_16x2_BF16_RM2ZZ>, bfloat16_t, bfloat16_t>(
    bfloat16_t *data_dst, bfloat16_t *data_src, int size_m, int size_n) MMA_IN
{
    if (kupl_unlikely(copy_check(data_dst, data_src, size_m, size_n) == false)) {
        return;
    }
    if (kupl_unlikely(size_n % BF16_TILE_32)) {    // 向量寄存器的宽度为512，对于bfloat16精度数据而言一个向量寄存器可以存放32个bfloat16数据
        kupl_error("The KUPL copy atom COPY_16x2_BF16_RM2ZZ now can "
                   "only support scenarios where n is a multiple of 32");
        return;
    }
    MATRIX_COMP_ON();
    svbool_t p = svwhilelt_b32(0, F32_TILE_16);
    for (int tile_m = 0; tile_m < size_m; tile_m += M_16) {
        for (int tile_n = 0; tile_n < size_n; tile_n += BF16_TILE_32) {
            // 横着写入向量寄存器
            bfloat16_t *data_src0 = data_src + tile_m * size_n + tile_n;
            for (uint32_t t = 0; t < F32_TILE_16; t++) {
                svld1_hor_za32(TILE_0, t, p, reinterpret_cast<float *>(data_src0));
                data_src0 += size_n;
            }

            // 竖着写回buffer
            bfloat16_t *data_dst0 = data_dst + tile_m * size_n + tile_n * M_16;
            for (uint32_t t = 0; t < F32_TILE_16; t++) {
                svst1_ver_za32(TILE_0, t, p, reinterpret_cast<float *>(data_dst0));
                data_dst0 += M_16 * N_2;
            }
        }
    }
    MATRIX_COMP_OFF();
}

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_copy<Ops<COPY_2x64_BF16_CM2NN>, bfloat16_t, bfloat16_t>(
    bfloat16_t *data_dst, bfloat16_t *data_src, int size_m, int size_n) MMA_IN
{
    if (kupl_unlikely(copy_check(data_dst, data_src, size_m, size_n) == false)) {
        return;
    }
    if (kupl_unlikely(size_m % BF16_TILE_32)) {    // 向量寄存器的宽度为512，对于bfloat16精度数据而言一个向量寄存器可以存放32个bfloat16数据
        kupl_error("The KUPL copy atom COPY_2x64_BF16_CM2NN now can "
                   "only support scenarios where m is a multiple of 32");
        return;
    }
    MATRIX_COMP_ON();
    svbool_t p = svwhilelt_b32(0, 16);
    for (int tile_n = 0; tile_n < size_n; tile_n += N_64) {
        for (int tile_m = 0; tile_m < size_m; tile_m += BF16_TILE_32) {
            // 横着写入向量寄存器
            bfloat16_t *data_src0 = data_src + tile_n * size_m + tile_m;
            bfloat16_t *data_src1 = data_src + (tile_n + F32_TILE_16) * size_m + tile_m;
            bfloat16_t *data_src2 = data_src + (tile_n + F32_TILE_16X2) * size_m + tile_m;
            bfloat16_t *data_src3 = data_src + (tile_n + F32_TILE_16X3) * size_m + tile_m;
            for (uint32_t t = 0; t < F32_TILE_16; t++) {
                svld1_hor_za32(TILE_0, t, p, reinterpret_cast<float *>(data_src0));
                svld1_hor_za32(TILE_1, t, p, reinterpret_cast<float *>(data_src1));
                svld1_hor_za32(TILE_2, t, p, reinterpret_cast<float *>(data_src2));
                svld1_hor_za32(TILE_3, t, p, reinterpret_cast<float *>(data_src3));
                data_src0 += size_m;
                data_src1 += size_m;
                data_src2 += size_m;
                data_src3 += size_m;
            }

            // 竖着写回buffer
            bfloat16_t *data_dst0 = data_dst + tile_n * size_m + tile_m * N_64;
            bfloat16_t *data_dst1 = data_dst + tile_n * size_m + tile_m * N_64 + BF16_TILE_32;
            bfloat16_t *data_dst2 = data_dst + tile_n * size_m + tile_m * N_64 + BF16_TILE_32X2;
            bfloat16_t *data_dst3 = data_dst + tile_n * size_m + tile_m * N_64 + BF16_TILE_32X3;
            for (uint32_t t = 0; t < 16; t++) {
                svst1_ver_za32(TILE_0, t, p, reinterpret_cast<float *>(data_dst0));
                svst1_ver_za32(TILE_1, t, p, reinterpret_cast<float *>(data_dst1));
                svst1_ver_za32(TILE_2, t, p, reinterpret_cast<float *>(data_dst2));
                svst1_ver_za32(TILE_3, t, p, reinterpret_cast<float *>(data_dst3));
                data_dst0 += N_64 * M_2;
                data_dst1 += N_64 * M_2;
                data_dst2 += N_64 * M_2;
                data_dst3 += N_64 * M_2;
            }
        }
    }
    MATRIX_COMP_OFF();
}

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_copy<Ops<COPY_16x1_BF16_RM2CM>, bfloat16_t, bfloat16_t>(
    bfloat16_t *data_dst, bfloat16_t *data_src, int size_m, int size_n)
{
    if (kupl_unlikely(copy_check(data_dst, data_src, size_m, size_n) == false)) {
        return;
    }
    // 该原子方法直接使用标量方式进行转置
    for (int tile_m = 0; tile_m < size_m; tile_m += M_16) {
        for (int n = 0; n < size_n; n++) {
            for (int m = tile_m; m < tile_m + M_16; m++) {
                data_dst[tile_m * size_n + n * M_16 + m] = data_src[m * size_n + n];
            }
        }
    }
}

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_copy<Ops<COPY_1x64_BF16_CM2RM>, bfloat16_t, bfloat16_t>(
    bfloat16_t *data_dst, bfloat16_t *data_src, int size_m, int size_n)
{
    if (kupl_unlikely(copy_check(data_dst, data_src, size_m, size_n) == false)) {
        return;
    }
    // 该原子方法直接使用标量方式进行转置
    for (int tile_n = 0; tile_n < size_n; tile_n += N_64) {
        for (int m = 0; m < size_m; m++) {
            for (int n = tile_n; n < tile_n + N_64; n++) {
                data_dst[tile_n * size_m + m * N_64 + n] = data_src[n * size_m + m];
            }
        }
    }
}

static constexpr int INT8_TILE_64 = 64;
static constexpr int INT8_TILE_64X2 = 128;
static constexpr int INT8_TILE_64X3 = 192;
static constexpr int INT32_TILE_16 = 16;
static constexpr int INT32_TILE_16X2 = 32;
static constexpr int INT32_TILE_16X3 = 48;

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_copy<Ops<COPY_16x4_INT8_RM2ZZ>, int8_t, int8_t>(
    int8_t *data_dst, int8_t *data_src, int size_m, int size_n) MMA_IN
{
    if (kupl_unlikely(copy_check(data_dst, data_src, size_m, size_n) == false)) {
        return;
    }
    if (kupl_unlikely(size_n % INT8_TILE_64)) {    // 向量寄存器的宽度为512，对于int8_t精度数据而言一个向量寄存器可以存放64个int8_t数据
        kupl_error("The KUPL copy atom COPY_16x4_INT8_RM2ZZ now can "
                   "only support scenarios where n is a multiple of 64");
        return;
    }
    MATRIX_COMP_ON();
    svbool_t p = svwhilelt_b32(0, INT32_TILE_16);
    for (int tile_m = 0; tile_m < size_m; tile_m += M_16) {
        for (int tile_n = 0; tile_n < size_n; tile_n += INT8_TILE_64) {
            // 横着写入向量寄存器
            int8_t *data_src0 = data_src + tile_m * size_n + tile_n;
            for (uint32_t t = 0; t < INT32_TILE_16; t++) {
                svld1_hor_za32(TILE_0, t, p, reinterpret_cast<int32_t *>(data_src0));
                data_src0 += size_n;
            }

            // 竖着写回buffer
            int8_t *data_dst0 = data_dst + tile_m * size_n + tile_n * 16;
            for (uint32_t t = 0; t < INT32_TILE_16; t++) {
                svst1_ver_za32(TILE_0, t, p, reinterpret_cast<int32_t *>(data_dst0));
                data_dst0 += M_16 * N_4;
            }
        }
    }
    MATRIX_COMP_OFF();
}

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_copy<Ops<COPY_4x64_INT8_CM2NN>, int8_t, int8_t>(
    int8_t *data_dst, int8_t *data_src, int size_m, int size_n) MMA_IN
{
    if (kupl_unlikely(copy_check(data_dst, data_src, size_m, size_n) == false)) {
        return;
    }
    if (kupl_unlikely(size_m % INT8_TILE_64)) {    // 向量寄存器的宽度为512，对于int8_t精度数据而言一个向量寄存器可以存放64个int8_t数据
        kupl_error("The KUPL copy atom COPY_4x64_INT8_CM2NN now can "
                   "only support scenarios where m is a multiple of 32");
        return;
    }
    MATRIX_COMP_ON();
    svbool_t p = svwhilelt_b32(0, INT32_TILE_16);
    for (int tile_n = 0; tile_n < size_n; tile_n += N_64) {
        for (int tile_m = 0; tile_m < size_m; tile_m += INT8_TILE_64) {
            // 横着写入向量寄存器
            int8_t *data_src0 = data_src + tile_n * size_m + tile_m;
            int8_t *data_src1 = data_src + (tile_n + INT32_TILE_16) * size_m + tile_m;
            int8_t *data_src2 = data_src + (tile_n + INT32_TILE_16X2) * size_m + tile_m;
            int8_t *data_src3 = data_src + (tile_n + INT32_TILE_16X3) * size_m + tile_m;
            for (uint32_t t = 0; t < INT32_TILE_16; t++) {
                svld1_hor_za32(TILE_0, t, p, reinterpret_cast<int32_t *>(data_src0));
                svld1_hor_za32(TILE_1, t, p, reinterpret_cast<int32_t *>(data_src1));
                svld1_hor_za32(TILE_2, t, p, reinterpret_cast<int32_t *>(data_src2));
                svld1_hor_za32(TILE_3, t, p, reinterpret_cast<int32_t *>(data_src3));
                data_src0 += size_m;
                data_src1 += size_m;
                data_src2 += size_m;
                data_src3 += size_m;
            }

            // 竖着写回buffer
            int8_t *data_dst0 = data_dst + tile_n * size_m + tile_m * N_64;
            int8_t *data_dst1 = data_dst + tile_n * size_m + tile_m * N_64 + INT8_TILE_64;
            int8_t *data_dst2 = data_dst + tile_n * size_m + tile_m * N_64 + INT8_TILE_64X2;
            int8_t *data_dst3 = data_dst + tile_n * size_m + tile_m * N_64 + INT8_TILE_64X3;
            for (uint32_t t = 0; t < INT32_TILE_16; t++) {
                svst1_ver_za32(TILE_0, t, p, reinterpret_cast<int32_t *>(data_dst0));
                svst1_ver_za32(TILE_1, t, p, reinterpret_cast<int32_t *>(data_dst1));
                svst1_ver_za32(TILE_2, t, p, reinterpret_cast<int32_t *>(data_dst2));
                svst1_ver_za32(TILE_3, t, p, reinterpret_cast<int32_t *>(data_dst3));
                data_dst0 += N_64 * M_4;
                data_dst1 += N_64 * M_4;
                data_dst2 += N_64 * M_4;
                data_dst3 += N_64 * M_4;
            }
        }
    }
    MATRIX_COMP_OFF();
}

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_copy<Ops<COPY_32x4_INT8_RM2ZZ>, int8_t, int8_t>(
    int8_t *data_dst, int8_t *data_src, int size_m, int size_n) MMA_IN
{
    if (kupl_unlikely(copy_check(data_dst, data_src, size_m, size_n) == false)) {
        return;
    }
    if (kupl_unlikely(size_n % INT8_TILE_64)) {    // 向量寄存器的宽度为512，对于int8_t精度数据而言一个向量寄存器可以存放64个int8_t数据
        kupl_error("The KUPL copy atom COPY_32x4_INT8_RM2ZZ now can "
                   "only support scenarios where n is a multiple of 64");
        return;
    }
    MATRIX_COMP_ON();
    svbool_t p = svwhilelt_b32(0, 16);
    for (int tile_m = 0; tile_m < size_m; tile_m += M_32) {
        for (int tile_n = 0; tile_n < size_n; tile_n += INT8_TILE_64) {
            // 横着写入向量寄存器
            int8_t *data_src0 = data_src + tile_m * size_n + tile_n;
            int8_t *data_src1 = data_src + (tile_m + INT32_TILE_16) * size_n + tile_n;
            for (uint32_t t = 0; t < INT32_TILE_16; t++) {
                svld1_hor_za32(TILE_0, t, p, reinterpret_cast<int32_t *>(data_src0));
                svld1_hor_za32(TILE_1, t, p, reinterpret_cast<int32_t *>(data_src1));
                data_src0 += size_n;
                data_src1 += size_n;
            }

            // 竖着写回buffer
            int8_t *data_dst0 = data_dst + tile_m * size_n + tile_n * M_32;
            int8_t *data_dst1 = data_dst + tile_m * size_n + tile_n * M_32 + INT8_TILE_64;
            for (uint32_t t = 0; t < INT32_TILE_16; t++) {
                svst1_ver_za32(0, t, p, reinterpret_cast<int32_t *>(data_dst0));
                svst1_ver_za32(1, t, p, reinterpret_cast<int32_t *>(data_dst1));
                data_dst0 += M_32 * N_4;
                data_dst1 += M_32 * N_4;
            }
        }
    }
    MATRIX_COMP_OFF();
}

template <>
kupl_export void kupl::tensor::TiledCallFunc::call_copy<Ops<COPY_4x32_INT8_CM2NN>, int8_t, int8_t>(
    int8_t *data_dst, int8_t *data_src, int size_m, int size_n) MMA_IN
{
    if (kupl_unlikely(copy_check(data_dst, data_src, size_m, size_n) == false)) {
        return;
    }
    if (kupl_unlikely(size_m % INT8_TILE_64)) {    // 向量寄存器的宽度为512，对于int8_t精度数据而言一个向量寄存器可以存放64个int8_t数据
        kupl_error("The KUPL copy atom COPY_4x32_INT8_CM2NN now can "
                   "only support scenarios where m is a multiple of 32");
        return;
    }
    MATRIX_COMP_ON();
    svbool_t p = svwhilelt_b32(0, INT32_TILE_16);
    for (int tile_n = 0; tile_n < size_n; tile_n += N_32) {
        for (int tile_m = 0; tile_m < size_m; tile_m += INT8_TILE_64) {
            // 横着写入向量寄存器
            int8_t *data_src0 = data_src + tile_n * size_m + tile_m;
            int8_t *data_src1 = data_src + (tile_n + INT32_TILE_16) * size_m + tile_m;
            for (uint32_t t = 0; t < INT32_TILE_16; t++) {
                svld1_hor_za32(TILE_0, t, p, reinterpret_cast<int32_t *>(data_src0));
                svld1_hor_za32(TILE_1, t, p, reinterpret_cast<int32_t *>(data_src1));
                data_src0 += size_m;
                data_src1 += size_m;
            }

            // 竖着写回buffer
            int8_t *data_dst0 = data_dst + tile_n * size_m + tile_m * N_32;
            int8_t *data_dst1 = data_dst + tile_n * size_m + tile_m * N_32 + INT8_TILE_64;
            for (uint32_t t = 0; t < INT32_TILE_16; t++) {
                svst1_ver_za32(TILE_0, t, p, reinterpret_cast<int32_t *>(data_dst0));
                svst1_ver_za32(TILE_1, t, p, reinterpret_cast<int32_t *>(data_dst1));
                data_dst0 += N_32 * M_4;
                data_dst1 += N_32 * M_4;
            }
        }
    }
    MATRIX_COMP_OFF();
}

#if !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif
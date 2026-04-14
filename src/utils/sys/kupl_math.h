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
#ifndef KUPL_MATH_H
#define KUPL_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

#define kupl_min(a, b) (((a) < (b)) ? (a) : (b))
#define kupl_max(a, b) (((a) > (b)) ? (a) : (b))
#define kupl_round_up(_x, _factor) (((_x) + (_factor) - 1) / (_factor) * (_factor))
#define kupl_divup(a, b) (((a) + (b) - 1) / (b))

#define kupl_align_down_pow2(_n, _alignment) \
    ((_n) & ~((_alignment) - 1))

#define kupl_align_up_pow2(_n, _alignment) \
    kupl_align_down_pow2((_n) + (_alignment) - 1, _alignment)

#ifdef __cplusplus
}
#endif

#endif
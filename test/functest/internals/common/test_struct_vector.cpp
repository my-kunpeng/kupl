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
#include "gtest/gtest.h"
#include "kupl.h"
#include "tools/struct/kupl_vector.h"

TEST(test_struct_vector_inner, kupl_vector_create_destroy)
{
    printf("test_struct_vector_inner\n");
    kupl_vector_create(0, 0, nullptr);
    kupl_vector_create(1, SIZE_MAX, nullptr);
    kupl_vector_cleanup(nullptr);
}
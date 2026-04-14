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
#include <exception>
#include <omp.h>
#include "common/fuzz_common.h"

static const int RANGE_HBW_SIZE_MIN = 1;
static const int RANGE_HBW_SIZE_MAX = 2000000;
static const int FUZZ_ERROR = 1;
static const int FUZZ_OK = 0;
static const size_t ALLOC_MAX = (1uLL << 32);
static const size_t REGULAR_PAGE_SIZE = 4096;

static int invalid_example()
{
    if (kupl_hbw_check_available() == 0) {
        printf("HBW not detected, Skipping hbw fuzz function...\n");
        printf("end -- %s\n", __func__);
        return FUZZ_ERROR;
    }

    kupl_hbw_set_policy(KUPL_HBW_POLICY_BIND);
    if (kupl_hbw_get_policy() != KUPL_HBW_POLICY_BIND) {
        printf("Policy setting unsuccessful, Error in fuzz test...\n");
        printf("end -- %s\n", __func__);
        return FUZZ_ERROR;
    }

    void *invalid_p = nullptr;
    void *ddr_p = nullptr;
    invalid_p = kupl_hbw_malloc(0);
    invalid_p = kupl_hbw_malloc(ALLOC_MAX + 1);
    ddr_p = kupl_malloc(KUPL_MEM_LARGE_CAP, REGULAR_PAGE_SIZE);
    kupl_hbw_verify(nullptr, REGULAR_PAGE_SIZE, FLAG_MIN);
    kupl_hbw_verify(ddr_p, ALLOC_MAX + 1, FLAG_MIN);
    kupl_hbw_verify(ddr_p, REGULAR_PAGE_SIZE, FLAG_MIN);
    kupl_free(KUPL_MEM_LARGE_CAP, ddr_p);
    kupl_hbw_free(invalid_p);
    return FUZZ_OK;
}

void hbw_example(int test_count)
{
    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);

    int invalid_fuzz = invalid_example();
    if (invalid_fuzz == FUZZ_ERROR) {
        return;
    }

    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        char *alloc_p;
        int alloc_size = *(int *)DT_SetGetNumberRange(&g_Element[cnt++], RANGE_HBW_SIZE_MIN,
                                                      RANGE_HBW_SIZE_MIN, RANGE_HBW_SIZE_MAX);

        alloc_p = (char *) kupl_hbw_malloc(alloc_size);
        if (alloc_p != nullptr) {
            alloc_p[0] = 'a';
            int flag = *(int *)DT_SetGetNumberRange(&g_Element[cnt++], FLAG_MIN,
                                                    FLAG_MIN, FLAG_MAX);
            kupl_hbw_verify(alloc_p, alloc_size, flag);
        }
        kupl_hbw_free(alloc_p);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}
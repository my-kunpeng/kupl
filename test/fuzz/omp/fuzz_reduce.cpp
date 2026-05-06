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
#include <omp.h>
#include "common/fuzz_common.h"

static const int NUM_THREADS_DFT = 4;
static const int NUM_THREADS_MIN = 0;
static const int NUM_THREADS_MAX = 128;
static const int64_t RANGE_LOWER_DFT = 0;
static const int64_t RANGE_UPPER_DFT = 64;
static const int64_t RANGE_STEP_DFT = 1;
static const int64_t RANGE_BLOCK_DFT = 1;
static int dimTable[] = {1, 2, 3};
static const int dimInit = 1;
static const int dimCount = 3;
static int policyTable[] = {0, 1, 2};
static const int policyInit = 1;
static const int policyCount = 3;
static int rdOpTable[] = {0, 1, 2, 3};
static const int rdOpInit = 0;
static const int rdOpCount = 4;
static int rdTypeTable[] = {0, 1, 2, 3, 4};
static const int rdTypeInit = 0;
static const int rdTypeCount = 5;
static char *lower_names[] = {"lower_0", "lower_1", "lower_2"};
static char *upper_names[] = {"upper_0", "upper_1", "upper_2"};
static char *step_names[] = {"step_0", "step_1", "step_2"};
static char *block_names[] = {"block_0", "block_1", "block_2"};


static inline void reduce_func(kupl_nd_range_t *nd_range, void *args, int tid, int tnum, kupl_reduce_args *rd_args)
{
}

void reduce_omp(int test_count)
{
    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        kupl_nd_range_t range;
        range.dim = 1;
        for (int i = 0; i < range.dim; i++) {
            range.nd_range[i].lower = (*(int64_t*)DT_SetGetNumberRangeS64(&g_Element[cnt++], RANGE_LOWER_DFT, -64, 64, lower_names[i]));
            range.nd_range[i].upper = (*(int64_t*)DT_SetGetNumberRangeS64(&g_Element[cnt++], RANGE_UPPER_DFT, -64, 64, upper_names[i]));
            range.nd_range[i].step = (*(int64_t*)DT_SetGetNumberRangeS64(&g_Element[cnt++], RANGE_STEP_DFT, -16, 16, step_names[i]));
            range.nd_range[i].blocksize = (*(int64_t*)DT_SetGetNumberRangeS64(&g_Element[cnt++], RANGE_BLOCK_DFT, -16, 16, block_names[i]));
        }
        int concurrency = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], NUM_THREADS_DFT, NUM_THREADS_MIN, NUM_THREADS_MAX, (char *)"concurrency");
        int policy = *(int*)DT_SetGetNumberEnum(&g_Element[cnt++], policyInit, policyTable, policyCount, (char *)"policy");
        int rd_op = *(int*)DT_SetGetNumberEnum(&g_Element[cnt++], rdOpInit, rdOpTable, rdOpCount, (char *)"op");
        int rd_type = *(int*)DT_SetGetNumberEnum(&g_Element[cnt++], rdTypeInit, rdTypeTable, rdTypeCount, (char *)"datatype");
        char *buffer[32] = {0};
        kupl_reduce_item_t rd_items[1] = {{ .buffer = buffer, .type = (kupl_datatype_t)rd_type, .op = (kupl_reduce_op_t)rd_op }};
        kupl_reduce_args_t rd_args = { .num = 1, .items = rd_items };

        printf("dim: %d, %d %d %d %d\n", range.dim, range.nd_range[0].lower, range.nd_range[0].upper, range.nd_range[0].step, range.nd_range[0].blocksize);
        printf("concurrency: %d\n", concurrency);
        kupl_parallel_for_desc_t desc = {
            .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
            .range = &range,
            .egroup = nullptr,
            .concurrency = concurrency,
            .policy = (kupl_loop_policy_type_t)policy
        };
        kupl_parallel_for_reduce(&desc, reduce_func, nullptr, &rd_args);

        #pragma omp parallel
        {
            kupl_egroup_barrier(nullptr);
        }
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}

void reduce_coverage()
{
    int sum = 0;
    kupl_reduce_item_t rd_items[1] = {{ .buffer = &sum, .type = KUPL_DATATYPE_INT, .op = KUPL_RD_ADD }};
    kupl_reduce_args_t rd_args = { .num = 1, .items = rd_items };
    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = nullptr,
        .egroup = nullptr,
        .concurrency = KUPL_CONCURRENCY_DEFAULT,
        .policy = KUPL_LOOP_POLICY_STATIC
    };

    // invalid rd_args
    kupl_parallel_for_reduce(&desc, reduce_func, nullptr, nullptr);
    
    // invalid policy
    desc.policy = (kupl_loop_policy_type_t)-1;
    kupl_parallel_for_reduce(&desc, reduce_func, nullptr, &rd_args);
    desc.policy = KUPL_LOOP_POLICY_STATIC;

    // invalid op
    rd_items[0].op = (kupl_reduce_op_t)-1;
    kupl_parallel_for_reduce(&desc, reduce_func, nullptr, &rd_args);
    rd_items[0].op = KUPL_RD_ADD;

    // invalid datatype
    rd_items[0].type = (kupl_datatype_t)-1;
    kupl_parallel_for_reduce(&desc, reduce_func, nullptr, &rd_args);
    rd_items[0].type = KUPL_DATATYPE_INT;
}

void reduce_pthread(int test_count)
{
    printf("start -- %s\n", __func__);
    reduce_coverage();
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        kupl_nd_range_t range;
        range.dim = *(int*)DT_SetGetNumberEnum(&g_Element[cnt++], dimInit, dimTable, dimCount, (char *)"dim");
        for (int i = 0; i < range.dim; i++) {
            range.nd_range[i].lower = (*(int64_t*)DT_SetGetNumberRangeS64(&g_Element[cnt++], RANGE_LOWER_DFT, -64, 64, lower_names[i]));
            range.nd_range[i].upper = (*(int64_t*)DT_SetGetNumberRangeS64(&g_Element[cnt++], RANGE_UPPER_DFT, -64, 64, upper_names[i]));
            range.nd_range[i].step = (*(int64_t*)DT_SetGetNumberRangeS64(&g_Element[cnt++], RANGE_STEP_DFT, -16, 16, step_names[i]));
            range.nd_range[i].blocksize = (*(int64_t*)DT_SetGetNumberRangeS64(&g_Element[cnt++], RANGE_BLOCK_DFT, -16, 16, block_names[i]));
        }
        int concurrency = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], NUM_THREADS_DFT, NUM_THREADS_MIN, NUM_THREADS_MAX, (char *)"concurrency");
        int policy = *(int*)DT_SetGetNumberEnum(&g_Element[cnt++], policyInit, policyTable, policyCount, (char *)"policy");
        int rd_op = *(int*)DT_SetGetNumberEnum(&g_Element[cnt++], rdOpInit, rdOpTable, rdOpCount, (char *)"op");
        int rd_type = *(int*)DT_SetGetNumberEnum(&g_Element[cnt++], rdTypeInit, rdTypeTable, rdTypeCount, (char *)"datatype");
        char *buffer[32] = {0};
        kupl_reduce_item_t rd_items[1] = {{ .buffer = buffer, .type = (kupl_datatype_t)rd_type, .op = (kupl_reduce_op_t)rd_op }};
        kupl_reduce_args_t rd_args = { .num = 1, .items = rd_items };
        kupl_parallel_for_desc_t desc = {
            .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
            .range = &range,
            .egroup = nullptr,
            .concurrency = concurrency,
            .policy = (kupl_loop_policy_type_t)policy
        };
        kupl_parallel_for_reduce(&desc, reduce_func, nullptr, &rd_args);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}
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

static const int RANGE_DIM_MIN = 1;
static const int RANGE_DIM_MAX = 3;
static const int NUM_THREADS_DFT = 4;
static const int NUM_THREADS_MIN = 0;
static const int NUM_THREADS_MAX = 128;
static const int EGROUP_NUM_ABLE_DFT = 4;
static const int EGROUP_NUM_ABLE_MIN = 1;
static const size_t RANGE_MAX = 0xff;
static const size_t RANGE_LOWER_DFT = 0;
static const size_t RANGE_UPPER_DFT = 64;

static inline void task_int_loop_1D(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
}

static inline void task_int_loop_no_range(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
}

void parallel_for_coverage()
{
    kupl_nd_range_t range4;
    int exe8[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    kupl_egroup_h eg8 = kupl_egroup_create(exe8, 8);
    kupl_parallel_for_desc_t desc4 = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = &range4,
        .egroup = eg8,
        .concurrency = 4,
        .policy = KUPL_LOOP_POLICY_STATIC
    };
    KUPL_1D_RANGE_INIT(range4, 1, RANGE_MAX);

    // null egroup condition
    desc4.egroup = nullptr;
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            kupl_parallel_for(&desc4, task_int_loop_1D, nullptr);
        }
        kupl_egroup_barrier(eg8);
    }
    desc4.egroup = eg8;

    desc4.range = nullptr;
    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            kupl_parallel_for(&desc4, task_int_loop_no_range, nullptr);
        }
        kupl_egroup_barrier(eg8);
    }

    // kupl::parallel_for
    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = nullptr,
        .egroup = nullptr,
        .concurrency = KUPL_CONCURRENCY_DEFAULT,
        .policy = KUPL_LOOP_POLICY_STATIC
    };
    kupl::parallel_for(&desc, [](const kupl_nd_range_t *nd_range, const int tid, const int tnum) {});

    kupl_egroup_destroy(eg8);
}

void parallel_for_static_example(int test_count)
{
    printf("start -- %s\n", __func__);
    parallel_for_coverage();
    int e_num = kupl_get_num_executors();
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        kupl_nd_range_t range;
        range.dim = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], RANGE_DIM_MIN, RANGE_DIM_MIN, RANGE_DIM_MAX);
        for (int i = 0; i < RANGE_DIM_MAX; i++) {
            range.nd_range[i].lower = (*(size_t*)DT_SetGetU64(&g_Element[cnt++], RANGE_LOWER_DFT));
            range.nd_range[i].upper = (*(size_t*)DT_SetGetU64(&g_Element[cnt++], RANGE_UPPER_DFT));
            range.nd_range[i].step = (*(size_t*)DT_SetGetU64(&g_Element[cnt++], 1));
            range.nd_range[i].blocksize = (*(size_t*)DT_SetGetU64(&g_Element[cnt++], 1));
        }
        int executor_num = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], EGROUP_NUM_ABLE_DFT,
                                                       EGROUP_NUM_ABLE_MIN, e_num);
        int executors[executor_num];
        for (int i = 0; i < executor_num; i++) {
            executors[i] = i;
        }
        int num_threads = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], NUM_THREADS_DFT,
                                                      NUM_THREADS_MIN, NUM_THREADS_MAX);
        kupl_egroup_h egroup = kupl_egroup_create(executors, executor_num);

        #pragma omp parallel num_threads(executor_num)
        {
            if (omp_get_thread_num() == 0) {
                kupl_parallel_for_desc_t desc = {
                    .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
                    .range = &range,
                    .egroup = egroup,
                    .concurrency = num_threads,
                    .policy = KUPL_LOOP_POLICY_STATIC
                };
                int ret = kupl_parallel_for(&desc, task_int_loop_1D, nullptr);
                if (ret == 0) {
                    printf("fuzz parallel for, executor_num: %d, dim: %d\n", executor_num, range.dim);
                }
            }
            kupl_egroup_barrier(egroup);
        }

        kupl_egroup_destroy(egroup);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}

void parallel_for_dynamic_example(int test_count)
{
    printf("start -- %s\n", __func__);
    int e_num = kupl_get_num_executors();
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        kupl_nd_range_t range;
        range.dim = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], RANGE_DIM_MIN, RANGE_DIM_MIN, RANGE_DIM_MAX);
        for (int i = 0; i < RANGE_DIM_MAX; i++) {
            range.nd_range[i].lower = (*(size_t*)DT_SetGetU64(&g_Element[cnt++], RANGE_LOWER_DFT)) & RANGE_MAX;
            range.nd_range[i].upper = (*(size_t*)DT_SetGetU64(&g_Element[cnt++], RANGE_UPPER_DFT)) & RANGE_MAX;
            range.nd_range[i].step = (*(size_t*)DT_SetGetU64(&g_Element[cnt++], 1)) & RANGE_MAX;
            range.nd_range[i].blocksize = (*(size_t*)DT_SetGetU64(&g_Element[cnt++], 1)) & RANGE_MAX;
        }
        int executor_num = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], EGROUP_NUM_ABLE_DFT,
                                                       EGROUP_NUM_ABLE_MIN, e_num);
        int executors[executor_num];
        for (int i = 0; i < executor_num; i++) {
            executors[i] = i;
        }
        int num_threads = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], NUM_THREADS_DFT,
                                                      NUM_THREADS_MIN, NUM_THREADS_MAX);
        kupl_egroup_h egroup = kupl_egroup_create(executors, executor_num);

        #pragma omp parallel num_threads(executor_num)
        {
            if (omp_get_thread_num() == 0) {
                kupl_parallel_for_desc_t desc = {
                    .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
                    .range = &range,
                    .egroup = egroup,
                    .concurrency = num_threads,
                    .policy = KUPL_LOOP_POLICY_DYNAMIC
                };
                int ret = kupl_parallel_for(&desc, task_int_loop_1D, nullptr);
                if (ret == 0) {
                    printf("fuzz parallel for, executor_num: %d, dim: %d\n", executor_num, range.dim);
                }
            }
            kupl_egroup_barrier(egroup);
        }

        kupl_egroup_destroy(egroup);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}

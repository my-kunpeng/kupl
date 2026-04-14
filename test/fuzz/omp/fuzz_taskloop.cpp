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
static const size_t EGROUP_NUM_ABLE_DFT = 4;
static const size_t EGROUP_NUM_ABLE_MIN = 1;
static const size_t RANGE_LOWER_DFT = 0;
static const size_t RANGE_UPPER_DFT = 64;
static const size_t RANGE_MAX = 0xff;

static void func_taskloop(kupl_nd_range_t *nd_range, void *args)
{
}

void taskloop_coverage()
{
    int executors[1] = {0};
    kupl_egroup_h egroup = kupl_egroup_create(executors, 1);
    kupl_graph_h graph = kupl_graph_create(egroup);
    kupl_nd_range_t range;
    const int range_upper = 10;
    KUPL_1D_RANGE_INIT(range, 0, range_upper);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        if (tid == 0) {
            kupl_taskloop_desc_t desc = {
                .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
                .func   = func_taskloop,
                .args   = nullptr,
                .range  = &range,
                .egroup = egroup,
            };
            kupl_task_info_t info = {
                .type = KUPL_TASK_TYPE_TASKLOOP,
                .desc = &desc,
            };
            kupl_graph_submit(graph, &info);
            kupl_graph_wait(graph);
        }
        kupl_egroup_barrier(egroup);
    }

    kupl_graph_destroy(graph);
    kupl_egroup_destroy(egroup);
}


void taskloop_example(int test_count)
{
    int num_executors = kupl_get_num_executors();
    taskloop_coverage();
    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        int executor_num = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], EGROUP_NUM_ABLE_DFT,
                                                       EGROUP_NUM_ABLE_MIN, num_executors);
        int executors[executor_num];
        for (int i = 0; i < executor_num; i++) {
            executors[i] = i;
        }
        kupl_egroup_h egroup = kupl_egroup_create(executors, executor_num);
        kupl_graph_h graph = kupl_graph_create(egroup);

        kupl_nd_range_t range;
        range.dim = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], RANGE_DIM_MIN,
                                                RANGE_DIM_MIN, RANGE_DIM_MAX);
        for (int i = 0; i < RANGE_DIM_MAX; i++) {
            range.nd_range[i].upper = (*(size_t*)DT_SetGetU16(&g_Element[cnt++], RANGE_LOWER_DFT)) & RANGE_MAX;
            range.nd_range[i].lower = (*(size_t*)DT_SetGetU16(&g_Element[cnt++], RANGE_UPPER_DFT)) & RANGE_MAX;
            range.nd_range[i].step = (*(size_t*)DT_SetGetU16(&g_Element[cnt++], 1)) & RANGE_MAX;
            range.nd_range[i].blocksize = (*(size_t*)DT_SetGetU16(&g_Element[cnt++], 1)) & RANGE_MAX;
        }

        #pragma omp parallel num_threads(executor_num)
        {
            int tid = omp_get_thread_num();
            if (tid == 0) {
                kupl_taskloop_desc_t desc = {
                    .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
                    .func   = func_taskloop,
                    .args   = nullptr,
                    .range  = &range,
                    .egroup = egroup,
                };
                kupl_task_info_t info = {
                    .type = KUPL_TASK_TYPE_TASKLOOP,
                    .desc = &desc,
                };
                kupl_graph_submit(graph, &info);
                kupl_graph_wait(graph);
            }
            kupl_egroup_barrier(egroup);
        }

        #pragma omp parallel num_threads(executor_num)
        {
            int tid = omp_get_thread_num();
            if (tid == 0) {
                kupl_taskloop_desc_t desc = {
                    .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
                    .range  = &range,
                    .egroup = egroup,
                };
                kupl::graph_submit(graph, &desc, [](const kupl_nd_range_t *nd_range) {});
                kupl_graph_wait(graph);
            }
            kupl_egroup_barrier(egroup);
        }

        kupl_graph_destroy(graph);
        kupl_egroup_destroy(egroup);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}
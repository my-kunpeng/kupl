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

static void func_test(void *args)
{
}

void task_wait_example(int test_count)
{
    int executors_bak[EGROUP_NUM_MAX] = {0};
    for (int i = 0; i < EGROUP_NUM_MAX; i++) {
        executors_bak[i] = 1;
    }
    char name_bak[NAME_LEN] = "00000";
    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        int *executors_real = (int*)DT_SetGetBlob(&g_Element[cnt++], EGROUP_NUM_MAX * sizeof(int),
                                                  EGROUP_NUM_MAX * sizeof(int), (char*)executors_bak);
        int executors_num = DT_GET_MutatedValueLen(&g_Element[0]) / sizeof(int);
        executors_num = kupl_get_num_executors() < executors_num ? kupl_get_num_executors() : executors_num;
        int executors[executors_num];
        int real_executors_num = 0;
        for (int i = 0; i < executors_num; i++) {
            if (executors_real[i]) {
                executors[real_executors_num++] = i;
            }
        }
        kupl_egroup_h egroup = kupl_egroup_create(executors, real_executors_num);
        kupl_graph_h graph = kupl_graph_create(egroup);

        int real_cpunum = kupl_get_num_executors();

        const int task_count = 10;

        #pragma omp parallel num_threads(real_cpunum)
        {
            int tid = omp_get_thread_num();
            if (tid == 0) {
                for (int i = 0; i < task_count; i++) {
                    uint32_t mask = *(uint32_t*)DT_SetGetNumberRange(&g_Element[cnt++], MASK_INIT,
                                                                     MASK_MIN, MASK_MAX);
                    int priority = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], PRIORITY_MIN,
                                                               PRIORITY_MIN, PRIORITY_MAX);
                    char *name = DT_SetGetStringNum(&g_Element[cnt++], NAME_LEN,
                                                    NAME_LEN, name_bak);
                    uint32_t flag = *(uint32_t*)DT_SetGetNumberRange(&g_Element[cnt++], FLAG_MIN,
                                                                     FLAG_MIN, FLAG_MAX);
                    kupl_task_desc_t desc = {
                        .field_mask     = mask,
                        .func           = func_test,
                        .args           = nullptr,
                        .name           = name,
                        .priority       = priority,
                        .flag           = flag,
                    };
                    kupl_task_info_t info = {
                        .type = KUPL_TASK_TYPE_SINGLE,
                        .desc = &desc,
                    };
                    kupl_graph_submit(graph, &info);
                }
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
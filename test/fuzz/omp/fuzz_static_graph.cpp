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

static void func_test(void *args)
{
}

void static_graph_create_example(int test_count)
{
    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        kupl_sgraph_h sgraph = kupl_sgraph_create();
        kupl_sgraph_destroy(sgraph);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}

void static_graph_example(int test_count)
{
    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);
    kupl_sgraph_h sgraph = kupl_sgraph_create();
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        char name_bak[NAME_LEN] = "00000";
        uint32_t mask = *(uint32_t*)DT_SetGetNumberRange(&g_Element[cnt++], MASK_INIT,
                                                         MASK_MIN, MASK_MAX);
        int priority = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], PRIORITY_MIN,
                                                   PRIORITY_MIN, PRIORITY_MAX);
        char *name = DT_SetGetStringNum(&g_Element[cnt++], NAME_LEN,
                                        NAME_LEN, name_bak);
        uint32_t flag = *(uint32_t*)DT_SetGetNumberRange(&g_Element[cnt++], FLAG_MIN,
                                                         FLAG_MIN, FLAG_MAX);
        kupl_sgraph_node_desc_t node_1_desc = {
            .field_mask     = mask,
            .func           = func_test,
            .args           = nullptr,
            .name           = name,
            .priority       = priority,
            .flag           = flag,
        };
        mask = *(uint32_t*)DT_SetGetNumberRange(&g_Element[cnt++], MASK_INIT,
                                                MASK_MIN, MASK_MAX);
        priority = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], PRIORITY_MIN,
                                               PRIORITY_MIN, PRIORITY_MAX);
        name = DT_SetGetStringNum(&g_Element[cnt++], NAME_LEN,
                                  NAME_LEN, name_bak);
        flag = *(uint32_t*)DT_SetGetNumberRange(&g_Element[cnt++], FLAG_MIN,
                                                FLAG_MIN, FLAG_MAX);
        kupl_sgraph_node_desc_t node_2_desc = {
            .field_mask     = mask,
            .func           = func_test,
            .args           = nullptr,
            .name           = name,
            .priority       = priority,
            .flag           = flag,
        };
        kupl_sgraph_node_h task_gnode_1 = kupl_sgraph_add_node(sgraph, &node_1_desc);
        kupl_sgraph_node_h task_gnode_2 = kupl_sgraph_add_node(sgraph, &node_2_desc);
        int ret = kupl_sgraph_add_dep(task_gnode_1, task_gnode_2);
        if (ret != KUPL_OK) {
            printf("static graph add norm dep failed in static graph example\n");
        }
    }
    DT_FUZZ_END();
    kupl_sgraph_destroy(sgraph);
    printf("end -- %s\n", __func__);
}

static kupl_sgraph_node_h sgraph_add_node(kupl_sgraph_h sgraph, int &cnt)
{
    char gnode_name[NAME_LEN] = "gnode";
    kupl_sgraph_node_desc_t gnode_desc = {
        .field_mask = *(uint32_t*)DT_SetGetNumberRange(&g_Element[cnt++], MASK_INIT,
                                                       MASK_MIN, MASK_MAX),
        .func       = func_test,
        .args       = nullptr,
        .name       = DT_SetGetStringNum(&g_Element[cnt++], NAME_LEN,
                                         NAME_LEN, gnode_name),
        .priority   = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], PRIORITY_MIN,
                                                  PRIORITY_MIN, PRIORITY_MAX),

        .flag       = *(uint32_t*)DT_SetGetNumberRange(&g_Element[cnt++], FLAG_MIN,
                                                       FLAG_MIN, FLAG_MAX),
    };
    return kupl_sgraph_add_node(sgraph, &gnode_desc);
}

static kupl_sgraph_node_h sgraph_add_node_lambda(kupl_sgraph_h sgraph, int &cnt)
{
    char gnode_name[NAME_LEN] = "gnode";
    kupl_sgraph_node_desc_t node_desc = {
        .field_mask = *(uint32_t*)DT_SetGetNumberRange(&g_Element[cnt++], MASK_INIT,
                                                       MASK_MIN, MASK_MAX),
        .name       = DT_SetGetStringNum(&g_Element[cnt++], NAME_LEN,
                                         NAME_LEN, gnode_name),
        .priority   = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], PRIORITY_MIN,
                                                  PRIORITY_MIN, PRIORITY_MAX),

        .flag       = *(uint32_t*)DT_SetGetNumberRange(&g_Element[cnt++], FLAG_MIN,
                                                       FLAG_MIN, FLAG_MAX),
    };
    return kupl::sgraph_add_node(sgraph, &node_desc, [&]() {});
}

void static_graph_execute(int test_count)
{
    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        kupl_graph_h graph = kupl_graph_create(nullptr);
        kupl_sgraph_h sgraph = kupl_sgraph_create();
        auto gnode_0 = sgraph_add_node(sgraph, cnt);
        auto gnode_1 = sgraph_add_node(sgraph, cnt);
        auto gnode_2 = sgraph_add_node(sgraph, cnt);
        auto gnode_3 = sgraph_add_node(sgraph, cnt);
        auto gnode_4 = sgraph_add_node_lambda(sgraph, cnt);
        kupl_sgraph_add_dep(gnode_0, gnode_1);
        kupl_sgraph_add_dep(gnode_0, gnode_2);
        kupl_sgraph_add_dep(gnode_1, gnode_3);
        kupl_sgraph_add_dep(gnode_2, gnode_3);
        kupl_sgraph_add_dep(gnode_3, gnode_4);

        int total_threads = kupl_get_num_executors();
        int executors[total_threads];
        for (int i = 0; i < total_threads; i++) {
            executors[i] = i;
        }
        kupl_egroup_h egroup = kupl_egroup_create(executors, total_threads);
        const int loop_count = 10;
        for (int i = 0; i < loop_count; i++) {
            #pragma omp parallel num_threads(total_threads)
            {
                if (omp_get_thread_num() == 0) {
                    char task_name[NAME_LEN] = "stask";
                    kupl_sgraph_task_desc_t desc = {
                        .field_mask = *(uint32_t*)DT_SetGetNumberRange(&g_Element[cnt++], MASK_INIT,
                                                                       MASK_MIN, MASK_MAX),
                        .sgraph     = sgraph,
                        .name       = DT_SetGetStringNum(&g_Element[cnt++], NAME_LEN,
                                                         NAME_LEN, task_name),
                        .priority   = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], PRIORITY_MIN,
                                                                  PRIORITY_MIN, PRIORITY_MAX),
                        .flag       = *(uint32_t*)DT_SetGetNumberRange(&g_Element[cnt++], FLAG_MIN,
                                                                       FLAG_MIN, FLAG_MAX),
                    };
                    kupl_task_info_t info = {
                        .type = KUPL_TASK_TYPE_SGRAPH,
                        .desc = &desc,
                    };
                    kupl_graph_submit(graph, &info);
                    kupl_graph_wait(graph);
                }
                kupl_egroup_barrier(egroup);
            }
        }
        kupl_egroup_destroy(egroup);
        kupl_sgraph_destroy(sgraph);
        kupl_graph_destroy(graph);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}
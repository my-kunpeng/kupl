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
#include "common/fuzz_common.h"

static const int ONE = 1;
static const int TWO = 2;

void egroup_example(int test_count)
{
    int executors_bak_0[EGROUP_NUM_MAX] = {0};
    int executors_bak_1[EGROUP_NUM_MAX] = {0};
    int executors_one[ONE] = {0};
    int executors_two[TWO] = {0, 1};
    kupl_egroup_create(nullptr, 1);
    kupl_egroup_create(executors_one, -1);
    kupl_egroup_h egroup_one = kupl_egroup_create(executors_one, sizeof(executors_one) / sizeof(int));
    kupl_egroup_h egroup_two = kupl_egroup_create(executors_two, sizeof(executors_two) / sizeof(int));
    kupl_egroup_barrier(nullptr);
    kupl_egroup_barrier(egroup_one);
    #pragma omp parallel num_threads(TWO)
    {
        kupl_egroup_barrier(egroup_two);
    }
    kupl_egroup_join_barrier(nullptr);
    kupl_egroup_fork_barrier(nullptr);
    kupl_egroup_join_barrier(egroup_one);
    kupl_egroup_fork_barrier(egroup_one);
    #pragma omp parallel num_threads(TWO)
    {
        kupl_egroup_join_barrier(egroup_two);
        kupl_egroup_fork_barrier(egroup_two);
    }
    kupl_egroup_borrow(egroup_one, egroup_two);
    kupl_egroup_return(egroup_one, egroup_two);
    kupl_egroup_destroy(egroup_one);
    kupl_egroup_destroy(egroup_two);

    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int *executors_0 = (int*)DT_SetGetBlob(&g_Element[0], EGROUP_NUM_MAX * sizeof(int),
                                               EGROUP_NUM_MAX * sizeof(int), (char *)executors_bak_0);
        int *executors_1 = (int*)DT_SetGetBlob(&g_Element[1], EGROUP_NUM_MAX * sizeof(int),
                                               EGROUP_NUM_MAX * sizeof(int), (char *)executors_bak_1);
        int executor_num_0 = DT_GET_MutatedValueLen(&g_Element[0]);
        int executor_num_1 = DT_GET_MutatedValueLen(&g_Element[1]);
        kupl_egroup_h egroup_0 = kupl_egroup_create(executors_0, executor_num_0 / sizeof(int));
        kupl_egroup_h egroup_1 = kupl_egroup_create(executors_1, executor_num_1 / sizeof(int));
        kupl_egroup_borrow(egroup_0, egroup_1);
        kupl_egroup_return(egroup_0, egroup_1);
        kupl_egroup_reset(egroup_0);
        kupl_egroup_destroy(egroup_0);
        kupl_egroup_destroy(egroup_1);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}

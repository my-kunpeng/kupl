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

static int copyinFlagInit = 0;
static int copyinFlagTable[] = {0, 1, 2};
static const int copyinFlagCount = 3;
static int copyoutFlagInit = 0;
static int copyoutFlagTable[] = {0, 1, 2, 3, 4};
static const int copyoutFlagCount = 5;
static int a[10] = {0};

void mem_coverage()
{
    // invalid ddr_addr
    kupl_mem_copyin(nullptr, sizeof(a), KUPL_MEM_CREATE, nullptr);
    kupl_mem_copyout(nullptr, sizeof(a), KUPL_MEM_DELETE, nullptr);

    // invalid flag
    kupl_mem_copyin(a, sizeof(a), (kupl_mem_copyin_flag_t)10, nullptr);
    kupl_mem_copyout(a, sizeof(a), (kupl_mem_copyout_flag_t)10, nullptr);

    // sync queue
    kupl_mem_copyin(a, sizeof(a), KUPL_MEM_CREATE, nullptr);
    kupl_mem_copyin(a+1, 1, KUPL_MEM_IN, nullptr);
    kupl_mem_copyout(a+1, 1, KUPL_MEM_OUT, nullptr);
    kupl_mem_copyout(a, sizeof(a), KUPL_MEM_DELETE, nullptr);

    kupl_mem_copyin(a, sizeof(a), KUPL_MEM_CREATE, nullptr);
}

void mem_pthread(int test_count)
{
    printf("start -- %s\n", __func__);
    mem_coverage();
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        auto copyinFlag = *(kupl_mem_copyin_flag_t*)DT_SetGetNumberEnum(&g_Element[cnt++], copyinFlagInit, copyinFlagTable, copyinFlagCount, (char *)"copyinFlag");
        auto copyoutFlag = *(kupl_mem_copyout_flag_t*)DT_SetGetNumberEnum(&g_Element[cnt++], copyoutFlagInit, copyoutFlagTable, copyoutFlagCount, (char *)"copyoutFlag");
        auto queue = kupl_queue_acquire(1);
        kupl_mem_copyin(a, sizeof(a), copyinFlag, queue);
        kupl_mem_is_present(a);
        kupl_queue_wait(queue);
        kupl_mem_query(a);
        kupl_mem_copyout(a, sizeof(a), copyoutFlag, queue);
        kupl_queue_wait(queue);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}

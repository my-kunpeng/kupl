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

static const size_t RANGE_LOWER_DFT = 0;
static const size_t RANGE_UPPER_DFT = 64;
static const size_t RANGE_MAX = 0xfff;
static const size_t EGROUP_NUM_ABLE_DFT = 4;
static const size_t EGROUP_NUM_ABLE_MIN = 1;

static void func_test(void *args)
{
}

static void invalid_example()
{
    auto queue = kupl_queue_create();
    int num_executors = kupl_get_num_executors();
    int executors[num_executors];
    const char *name = "auto";
    for (int i = 0; i < num_executors; i++) {
        executors[i] = i;
    }
    kupl_egroup_h egroup = kupl_egroup_create(executors, num_executors);

    kupl_queue_kernel_desc_t kernel_desc = {
        .field_mask = KUPL_QUEUE_KERNEL_DESC_FIELD_NAME,
        .range = nullptr,
        .egroup = egroup,
        .name = name,
    };
    kupl::queue_submit(queue, &kernel_desc,  [&](const kupl_nd_range_t *nd_range) {});

    kupl_nd_range_t range;
    KUPL_2D_RANGE_INIT(range, 0, num_executors, 0, num_executors);
    kernel_desc.range = &range;
    kupl::queue_submit(queue, &kernel_desc,  [&](const kupl_nd_range_t *nd_range) {});

    KUPL_1D_RANGE_INIT(range, 0, num_executors);
    kernel_desc.range = &range;
    kernel_desc.egroup = nullptr;
    kupl::queue_submit(queue, &kernel_desc,  [&](const kupl_nd_range_t *nd_range) {});

    kernel_desc.egroup = egroup;
    kernel_desc.name = nullptr;
    kupl::queue_submit(queue, &kernel_desc,  [&](const kupl_nd_range_t *nd_range) {});

    kernel_desc.range->nd_range[0].upper = UINT64_MAX;
    kupl::queue_submit(queue, &kernel_desc,  [&](const kupl_nd_range_t *nd_range) {});
}

void queue_event_example(int test_count)
{
    char name_bak[NAME_LEN] = "00000";
    int num_executors = kupl_get_num_executors();
    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);
    invalid_example();
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        auto queue = kupl_queue_create();
        auto queue2 = kupl_queue_create();
        auto event = kupl_event_create();

        kupl_event_record(event, queue);
        kupl_event_wait(event);

        kupl_event_record(event, queue);
        kupl_queue_wait_event(queue2, event);
        kupl_queue_wait(queue2);

        kupl_event_query(event);

        char *name = DT_SetGetStringNum(&g_Element[cnt++], NAME_LEN,
                                        NAME_LEN, name_bak);

        kupl_queue_item_desc_t desc = {
            .field_mask     = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
            .func           = func_test,
            .args           = nullptr,
            .name           = name,
        };
        kupl_queue_submit(queue, &desc);
        kupl_queue_wait(queue);
        size_t lower = (*(size_t*)DT_SetGetU16(&g_Element[cnt++], RANGE_LOWER_DFT)) & RANGE_MAX;
        size_t upper = (*(size_t*)DT_SetGetU16(&g_Element[cnt++], RANGE_UPPER_DFT)) & RANGE_MAX;
        size_t step = (*(size_t*)DT_SetGetU16(&g_Element[cnt++], 1)) & RANGE_MAX;
        size_t blocksize = (*(size_t*)DT_SetGetU16(&g_Element[cnt++], 1)) & RANGE_MAX;

        kupl_nd_range_t range;
        KUPL_STRIDE_1D_RANGE_INIT(range, lower, upper, step, blocksize);

        int executor_num = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], EGROUP_NUM_ABLE_DFT,
                                                       EGROUP_NUM_ABLE_MIN, num_executors);
        int executors[executor_num];
        for (int i = 0; i < executor_num; i++) {
            executors[i] = i;
        }
        kupl_egroup_h egroup = kupl_egroup_create(executors, executor_num);

        #pragma omp parallel num_threads(executor_num)
        {
            int tid = omp_get_thread_num();
            if (tid == 0) {
                kupl_queue_kernel_desc_t kernel_desc = {
                    .field_mask = KUPL_QUEUE_KERNEL_DESC_FIELD_NAME,
                    .range = &range,
                    .egroup = egroup,
                    .name = name,
                };
                kupl::queue_submit(queue, &kernel_desc, [&](const kupl_nd_range_t *nd_range) {});
                kupl_queue_wait(queue);
            }
            kupl_egroup_barrier(egroup);
        }

        #pragma omp parallel num_threads(num_executors)
        {
            int tid = omp_get_thread_num();
            if (tid == 0) {
                kupl_queue_item_desc_t desc = {
                    .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
                    .name = name,
                };
                kupl::queue_submit(queue, &desc, [&]() {});
                kupl_queue_wait(queue);
            }
            kupl_egroup_barrier(egroup);
        }
        kupl_egroup_destroy(egroup);
        kupl_event_destroy(event);
        kupl_queue_destroy(queue2);
        kupl_queue_destroy(queue);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}

void queue_priority_example(int test_count)
{
    char name_bak[NAME_LEN] = "00000";
    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        int least_priority, greatest_priority;
        kupl_get_queue_priority_range(&least_priority, &greatest_priority);

        auto queue1 = kupl_queue_create_with_priority(least_priority);
        auto queue2 = kupl_queue_create_with_priority(greatest_priority);

        char *name1 = DT_SetGetStringNum(&g_Element[cnt++], NAME_LEN,
                                        NAME_LEN, name_bak);
        char *name2 = DT_SetGetStringNum(&g_Element[cnt++], NAME_LEN,
                                        NAME_LEN, name_bak);

        kupl_queue_item_desc_t desc1 = {
            .field_mask     = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
            .func           = func_test,
            .args           = nullptr,
            .name           = name1,
        };
        kupl_queue_item_desc_t desc2 = {
            .field_mask     = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
            .func           = func_test,
            .args           = nullptr,
            .name           = name2,
        };
        kupl_queue_submit(queue1, &desc1);
        kupl_queue_submit(queue2, &desc2);
        kupl_queue_wait(queue1);
        kupl_queue_wait(queue2);

        kupl_queue_destroy(queue1);
        kupl_queue_destroy(queue2);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}
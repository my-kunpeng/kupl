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

fuzz_cmd_t g_cmdtable[] = {
    {"egroup", egroup_example},
    {"parallel_for", parallel_for_static_example},
    {"parallel_for_dynamic", parallel_for_dynamic_example},
    {"task_wait", task_wait_example},
    {"taskloop", taskloop_example},
    {"graph_base", graph_base_example},
    {"mlock", mlock_example},
    {"memcpy1d", memcpy1d_example},
    {"memcpy1d_async", memcpy1d_async_example},
    {"memcpy2d", memcpy2d_example},
    {"memcpy2d_async", memcpy2d_async_example},
    {"memcpy_priority", memcpy_priority_example},
    {"static_graph_create_example", static_graph_create_example},
    {"static_graph_example", static_graph_example},
    {"static_graph_execute", static_graph_execute},
    {"queue_event", queue_event_example},
    {"queue_priority", queue_priority_example},
    {"hbw", hbw_example},
};

int main(int argc, const char *argv[])
{
    if (argc < MIN_ARGUMENT_NUM) {
        printf("argc num error!\n");
        return 1;
    }

    printf("case = %s, count = %s\n", argv[CASE_INDEX], argv[COUNT_INDEX]);

    if (strcmp(argv[CASE_INDEX], "all") == 0) {
        for (size_t i = 0; i < sizeof(g_cmdtable) / sizeof(g_cmdtable[0]); i++) {
            g_cmdtable[i].func(atoi(argv[COUNT_INDEX]));
            printf("case = %s, count = %s ... finished\n", argv[CASE_INDEX], argv[COUNT_INDEX]);
        }
        return 0;
    }

    for (size_t i = 0; i < sizeof(g_cmdtable) / sizeof(g_cmdtable[0]); i++) {
        if (strcmp(argv[CASE_INDEX], g_cmdtable[i].cmd) == 0) {
            g_cmdtable[i].func(atoi(argv[COUNT_INDEX]));
            printf("case = %s, count = %s ... finished\n", argv[CASE_INDEX], argv[COUNT_INDEX]);
            break;
        }
    }
    return 0;
}

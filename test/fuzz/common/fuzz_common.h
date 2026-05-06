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
#ifndef KUPL_FUZZ_COMM_H
#define KUPL_FUZZ_COMM_H

#include <unistd.h>
#include "secodeFuzz.h"
#include "kupl.h"

#ifdef __cplusplus
extern "C" {
#endif

static const int MIN_ARGUMENT_NUM = 3;
static const int CASE_INDEX = 1;
static const int COUNT_INDEX = 2;

static const int EGROUP_NUM_MIN = 0;
static const int EGROUP_NUM_MAX = 1024;
static const int NAME_LEN = 6;
static const int MASK_INIT = 0;
static const int MASK_MIN = 0;
static const int MASK_MAX = 7;
static const int PRIORITY_MIN = 0;
static const int PRIORITY_MAX = 65536;
static const int FLAG_MIN = 0;
static const int FLAG_MAX = 1;

extern void egroup_example(int test_count);
extern void parallel_for_static_example(int test_count);
extern void parallel_for_dynamic_example(int test_count);
extern void reduce_omp(int test_count);
extern void reduce_pthread(int test_count);
extern void task_wait_example(int test_count);
extern void taskloop_example(int test_count);
extern void graph_base_example(int test_count);
extern void mlock_example(int test_count);
extern void memcpy1d_example(int test_count);
extern void memcpy1d_async_example(int test_count);
extern void memcpy2d_example(int test_count);
extern void memcpy2d_async_example(int test_count);
extern void memcpy_priority_example(int test_count);
extern void win_alloc_example(int test_count);
extern void win_query_example(int test_count);
extern void win_barrier_example(int test_count);
extern void allreduce_example(int test_count);
extern void alltoall_example(int test_count);
extern void bcast_example(int test_count);
extern void static_graph_create_example(int test_count);
extern void static_graph_example(int test_count);
extern void static_graph_execute(int test_count);
extern void queue_event_example(int test_count);
extern void queue_priority_example(int test_count);
extern void hbw_example(int test_count);
extern void mma_base_example(int test_count);
extern void copy_base_example(int test_count);

typedef struct fuzz_cmd {
    const char *cmd;
    void (*func)(int test_count);
} fuzz_cmd_t;


#ifdef __cplusplus
}
#endif

#endif
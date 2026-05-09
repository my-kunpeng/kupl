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
#ifndef KUPL_PARALLEL_FOR_H
#define KUPL_PARALLEL_FOR_H

#include <complex>
#include "kupl.h"
#include "executor/kupl_executor_group.h"
#include "mt/kupl_ult.h"
#include "mt/kupl_graph.h"
#include "mt/barrier/kupl_barrier.h"

#ifdef __cplusplus
extern "C" {
#endif

using kupl_pf_t = struct kupl_pf;

using kupl_pf_policy_func_t = void(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum);
using kupl_pf_post_func_t = void(kupl_pf_t &pf, int tid, int tnum);

typedef struct range_chunk {
    int64_t chunksize;
    int64_t chunks;
} range_chunk_t;

typedef union kupl_reduce_data {
    int i;
    float f;
    double d;
    std::complex<float> fc;
    std::complex<double> dc;
    void *p;
} kupl_reduce_data_t KUPL_ALIGN(128);

struct kupl_pf {
    int master_eid;
    kupl_pf_reduce_func_t rd_func;
    kupl_reduce_args_t *shared_args;
    kupl_reduce_args_t private_args;
    kupl_reduce_item_t *rd_item;
    kupl_reduce_data_t *rd_data;
    int rd_num_max;
    struct aligned_index {
        KUPL_ATOMIC_INT64 value;
        int64_t target;
    } KUPL_ALIGN(128);
    aligned_index *chunk_index;
    int64_t total_chunks;
    range_chunk_t chunk_info[KUPL_MAX_DIM_SIZE];
    kupl_pf_policy_func_t *policy_func;
    kupl_pf_post_func_t *post_func;
    kupl_pf_func_t func;
    void *args;
    kupl_nd_range_t *range;
    kupl_egroup_h egroup;
    int num_threads;
    kupl_loop_policy_type_t policy;
    int64_t flag;
    kupl_ult_t ult;
};

int kupl_pf_init(void);

void kupl_pf_fini(void);

typedef void (*kupl_parallel_func_t)(void *args, int tid, int tnum);

int kupl_invoke_parallel(kupl_parallel_func_t func, void *args, int num_threads = -1);

#ifdef __cplusplus
}
#endif

#endif
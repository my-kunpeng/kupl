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
#include "kupl_parallel_for.h"
#include <omp.h>
#include <climits>
#include <cfloat>
#include "kupl.h"
#include "kupl_task.h"
#include "core/kupl_core.h"
#include "mt/kupl_check.h"
#include "executor/kupl_executor.h"
#include "executor/backend/kupl_executor_backend.h"
#include "utils/sys/kupl_math.h"
#include "mt/barrier/kupl_barrier.h"
#include "utils/sys/kupl_math.h"
#include "tools/profile/kupl_profile.h"
#include "tools/profile/kupl_profile_trace.h"

static thread_local int call_cnt = 0;
static int g_concurrency_default = 0;
static kupl_egroup_h g_egroup = nullptr;
static kupl_pf_t *g_pf = nullptr;
static kupl_ult_t *g_ult = nullptr;
constexpr int KUPL_RD_NUM_MAX = 128;

static kupl_always_inline kupl_graph_h kupl_pf_graph_get()
{
    KUPL_PTRACE_START(KUPL_PTRACE_PARALLEL_FOR_GRAPH_GET);
    kupl_graph_h graph = kupl_get_current_graph();
    if (graph == nullptr) {
        graph = kupl_get_global_graph();
    }
    KUPL_PTRACE_END(KUPL_PTRACE_PARALLEL_FOR_GRAPH_GET);

    return graph;
}

template <typename T>
static void reduce_buffer(int op, void *buffer, T data)
{
    switch (op) {
        case KUPL_RD_ADD:
        case KUPL_RD_SUB:
            *static_cast<T *>(buffer) += data;
            break;
        case KUPL_RD_MAX:
            *static_cast<T *>(buffer) = kupl_max(*static_cast<T *>(buffer), data);
            break;
        case KUPL_RD_MIN:
            *static_cast<T *>(buffer) = kupl_min(*static_cast<T *>(buffer), data);
            break;
        default:
            kupl_error("option: %d not supported", op);
    }
}

template <>
void reduce_buffer<std::complex<float>>(int op, void *buffer, std::complex<float> data)
{
    switch (op) {
        case KUPL_RD_ADD:
        case KUPL_RD_SUB:
            *static_cast<std::complex<float> *>(buffer) += data;
            break;
        default:
            kupl_error("option: %d not supported", op);
    }
}

template <>
void reduce_buffer<std::complex<double>>(int op, void *buffer, std::complex<double> data)
{
    switch (op) {
        case KUPL_RD_ADD:
        case KUPL_RD_SUB:
            *static_cast<std::complex<double> *>(buffer) += data;
            break;
        default:
            kupl_error("option: %d not supported", op);
    }
}

static void do_reduce(kupl_reduce_args_t *rd_args, kupl_reduce_data_t *rd_data)
{
    int rd_num = rd_args->num;
    for (int i = 0; i < rd_num; i++) {
        auto &item = rd_args->items[i];
        switch (item.type) {
            case KUPL_DATATYPE_INT:
                reduce_buffer<int>(item.op, item.buffer, rd_data[i].i);
                break;
            case KUPL_DATATYPE_FLOAT:
                reduce_buffer<float>(item.op, item.buffer, rd_data[i].f);
                break;
            case KUPL_DATATYPE_DOUBLE:
                reduce_buffer<double>(item.op, item.buffer, rd_data[i].d);
                break;
            case KUPL_DATATYPE_FLOAT_COMPLEX:
                reduce_buffer<std::complex<float>>(item.op, item.buffer, rd_data[i].fc);
                break;
            case KUPL_DATATYPE_DOUBLE_COMPLEX:
                reduce_buffer<std::complex<double>>(item.op, item.buffer, rd_data[i].dc);
                break;
            default:
                kupl_error("datatype: %d not supported", item.type);
        }
    }
}

static kupl_always_inline void kupl_reduce_post_func(kupl_pf_t &pf, int tid, int tnum)
{
    if (tid != 0) {
        return;
    }

    KUPL_FOR_EACH_LIMIT_EGROUP(pf.egroup, tnum, eid, eidx, { do_reduce(pf.shared_args, g_pf[eid].rd_data); });
}

static kupl_always_inline int get_max_range_dim(kupl_nd_range_t &range)
{
    int64_t max_chunks = 0;
    int dim_of_max = 0;
    for (int i = 0; i < range.dim; i++) {
        int64_t chunksize = range.nd_range[i].step;
        int64_t current_chunks = kupl_divup(range.nd_range[i].upper - range.nd_range[i].lower, chunksize);
        if (current_chunks > max_chunks) {
            max_chunks = current_chunks;
            dim_of_max = i;
        }
    }
    return dim_of_max;
}

typedef struct kupl_task_args {
    int64_t task_id;
    int master_eid;
} kupl_task_args_t;

static void task_policy_loop_func(void *args)
{
    call_cnt++;
    auto task_args = (kupl_task_args_t *)args;
    if (args == nullptr) {
        kupl_warn("task_policy_loop_func input nullptr");
        return;
    }

    int geid = kupl_get_executor_num();
    int master_eid = task_args->master_eid;
    auto &pf = g_pf[master_eid];

    kupl_nd_range_t task_range;
    task_range.dim = 1;

    int64_t task_id = task_args->task_id;
    int64_t chunksize = pf.chunk_info[0].chunksize;
    int64_t lower = pf.range->nd_range[0].lower;
    int64_t upper = pf.range->nd_range[0].upper;

    task_range.nd_range[0].lower = lower + task_id * chunksize;
    task_range.nd_range[0].upper = kupl_min(upper, lower + (task_id + 1) * chunksize);
    task_range.nd_range[0].step = pf.range->nd_range[0].step;
    task_range.nd_range[0].blocksize = pf.range->nd_range[0].blocksize;

    int tid = (int)pf.egroup->cur.eid2lid[geid];
    int tnum = pf.num_threads;

    if (pf.rd_func != nullptr) {
        kupl_reduce_args_t *private_args = &g_pf[geid].private_args;
        pf.rd_func(&task_range, pf.args, tid, tnum, private_args);
    } else {
        pf.func(&task_range, pf.args, tid, tnum);
    }

    call_cnt--;
}

int kupl_invoke_parallel(kupl_parallel_func_t func, void *args, int num_threads)
{
    kupl_parallel_for_desc_t desc = {.field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
                                     .range = nullptr,
                                     .egroup = nullptr,
                                     .concurrency = num_threads,
                                     .policy = KUPL_LOOP_POLICY_STATIC};
    return kupl::parallel_for(&desc, [&](const kupl_nd_range_t *nd_range, const int tid, const int tnum) {
        (void)nd_range;
        func(args, tid, tnum);
    });
}

static kupl_always_inline kupl_reduce_args_t *kupl_reduce_args_dup_impl(kupl_reduce_args_t *rd_args, kupl_pf_t &own_pf,
                                                                        int geid)
{
    static constexpr int INIT_INT[4] = {0, 0, INT_MIN, INT_MAX};
    static constexpr float INIT_FLOAT[4] = {0.0f, 0.0f, -FLT_MAX, FLT_MAX};
    static constexpr double INIT_DOUBLE[4] = {0.0, 0.0, -DBL_MAX, DBL_MAX};

    auto rd_num = rd_args->num;
    if (rd_num > own_pf.rd_num_max) {
        kupl_memory_free(own_pf.rd_item, geid);
        own_pf.rd_item = (kupl_reduce_item_t *)kupl_memory_alloc((size_t)rd_num * sizeof(kupl_reduce_item_t), geid);
        kupl_memory_free(own_pf.rd_data, geid);
        own_pf.rd_data = (kupl_reduce_data_t *)kupl_memory_alloc((size_t)rd_num * sizeof(kupl_reduce_data_t), geid);
        own_pf.rd_num_max = rd_num;
        if (kupl_unlikely((own_pf.rd_item == nullptr) || (own_pf.rd_data == nullptr))) {
            kupl_memory_free(own_pf.rd_item, geid);
            kupl_memory_free(own_pf.rd_data, geid);
            own_pf.rd_item = nullptr;
            own_pf.rd_data = nullptr;
            return nullptr;
        }
    }
    own_pf.private_args.num = rd_num;
    own_pf.private_args.items = own_pf.rd_item;
    for (int i = 0; i < rd_num; i++) {
        own_pf.rd_item[i] = rd_args->items[i];
        own_pf.rd_item[i].buffer = &own_pf.rd_data[i];

        int op_idx = rd_args->items[i].op;
        switch (rd_args->items[i].type) {
            case KUPL_DATATYPE_INT:
                own_pf.rd_data[i].i = INIT_INT[op_idx];
                break;
            case KUPL_DATATYPE_FLOAT:
                own_pf.rd_data[i].f = INIT_FLOAT[op_idx];
                break;
            case KUPL_DATATYPE_DOUBLE:
                own_pf.rd_data[i].d = INIT_DOUBLE[op_idx];
                break;
            case KUPL_DATATYPE_FLOAT_COMPLEX:
                own_pf.rd_data[i].fc = std::complex<float>(0.0f, 0.0f);
                break;
            case KUPL_DATATYPE_DOUBLE_COMPLEX:
                own_pf.rd_data[i].dc = std::complex<double>(0.0, 0.0);
                break;
            default:
                break;
        }
    }
    return &own_pf.private_args;
}

static kupl_reduce_args_t *kupl_reduce_args_dup(kupl_reduce_args_t *rd_args)
{
    int geid = kupl_get_executor_num();
    return kupl_reduce_args_dup_impl(rd_args, g_pf[geid], geid);
}

static void kupl_reduce_no_range_policy_func(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum)
{
    (void)range;
    auto private_args = kupl_reduce_args_dup(pf.shared_args);
    if (kupl_likely(private_args != nullptr)) {
        pf.rd_func(nullptr, pf.args, tid, tnum, private_args);
    }
}

static void kupl_no_range_policy_func(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum)
{
    (void)range;
    pf.func(nullptr, pf.args, tid, tnum);
}

template <typename CallFunc>
static kupl_always_inline void kupl_static_policy_impl(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum,
                                                       CallFunc call)
{
    range.dim = pf.range->dim;
    auto &nd_range = range.nd_range[0];
    auto chunksize = pf.chunk_info[0].chunksize;

    int64_t num_threads = (int64_t)tnum;
    int64_t base_chunks = pf.total_chunks / num_threads;
    int64_t lower = pf.range->nd_range[0].lower;
    int64_t upper = pf.range->nd_range[0].upper;

    int64_t remain_chunks = pf.chunk_info[0].chunks % num_threads;
    int64_t lower_tmp = lower + (int64_t)tid * chunksize * base_chunks;

    nd_range.lower = lower_tmp + kupl_min(remain_chunks, (int64_t)tid) * chunksize;
    nd_range.upper = lower_tmp + chunksize * base_chunks + kupl_min(remain_chunks, ((int64_t)tid + 1)) * chunksize;
    if (chunksize > 0) {
        nd_range.upper = kupl_min(nd_range.upper, upper);
    } else {
        nd_range.upper = kupl_max(nd_range.upper, upper);
    }
    nd_range.step = pf.range->nd_range[0].step;
    nd_range.blocksize = pf.range->nd_range[0].blocksize;
    call();
}

static void kupl_static_policy_func(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum)
{
    kupl_static_policy_impl(range, pf, tid, tnum, [&]() { pf.func(&range, pf.args, tid, tnum); });
}

static void kupl_reduce_static_policy_func(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum)
{
    auto private_args = kupl_reduce_args_dup(pf.shared_args);
    if (kupl_unlikely(private_args == nullptr)) {
        return;
    }
    kupl_static_policy_impl(range, pf, tid, tnum, [&]() { pf.rd_func(&range, pf.args, tid, tnum, private_args); });
}

static void divide_range(kupl_nd_range_t &range, kupl_pf_t &pf, int64_t target_tid, int64_t head_tid,
                         int64_t num_threads)
{
    // the end condition, divide to only one num_threads
    if (num_threads == 1 && target_tid == head_tid) {
        return;
    }
    // save nd_range infomation to recover changes by the recursive
    int divide_dim = get_max_range_dim(range);
    int64_t left_threads = num_threads >> 1;
    int64_t right_threads = num_threads - left_threads;

    int64_t lower = pf.range->nd_range[divide_dim].lower;
    int64_t upper = pf.range->nd_range[divide_dim].upper;
    int64_t chunksize = pf.chunk_info[divide_dim].chunksize;
    int64_t total_chunks = kupl_divup(upper - lower, chunksize);
    int64_t left_chunks = total_chunks / (int64_t)num_threads * (int64_t)left_threads;
    int64_t mid = lower + left_chunks * chunksize;

    if (head_tid + left_threads > target_tid) {
        range.nd_range[divide_dim].upper = mid;
        divide_range(range, pf, target_tid, head_tid, left_threads);
    } else {
        range.nd_range[divide_dim].lower = mid;
        divide_range(range, pf, target_tid, head_tid + left_threads, right_threads);
    }
}

template <typename CallFunc>
static kupl_always_inline void kupl_nd_static_policy_impl(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum,
                                                          CallFunc call)
{
    range = *pf.range;
    divide_range(range, pf, (int64_t)tid, 0, (int64_t)tnum);
    call();
}

static void kupl_nd_static_policy_func(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum)
{
    kupl_nd_static_policy_impl(range, pf, tid, tnum, [&]() { pf.func(&range, pf.args, tid, tnum); });
}

static void kupl_reduce_nd_static_policy_func(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum)
{
    auto private_args = kupl_reduce_args_dup(pf.shared_args);
    if (kupl_unlikely(private_args == nullptr)) {
        return;
    }
    kupl_nd_static_policy_impl(range, pf, tid, tnum, [&]() { pf.rd_func(&range, pf.args, tid, tnum, private_args); });
}

static kupl_always_inline void kupl_compute_chunk_range(kupl_nd_range_t &range, kupl_pf_t &pf, int64_t chunk_idx)
{
    int64_t ci = chunk_idx;
    for (int d = range.dim - 1; d >= 0; d--) {
        auto nd_index = ci % pf.chunk_info[d].chunks;
        ci = ci / pf.chunk_info[d].chunks;
        int64_t chunksize = pf.chunk_info[d].chunksize;
        range.nd_range[d].lower = pf.range->nd_range[d].lower + nd_index * chunksize;
        if (chunksize > 0) {
            range.nd_range[d].upper = kupl_min(range.nd_range[d].lower + chunksize, pf.range->nd_range[d].upper);
        } else {
            range.nd_range[d].upper = kupl_max(range.nd_range[d].lower + chunksize, pf.range->nd_range[d].upper);
        }
    }
}

template <typename CallFunc>
static kupl_always_inline void kupl_dynamic_policy_impl(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum,
                                                        CallFunc call)
{
    range = *pf.range;
    int target_tid = tid;
    for (int64_t loop_count = 0; loop_count < (int64_t)tnum; loop_count++) {
        if (target_tid >= tnum) {
            target_tid = 0;
        }
        int64_t chunk_index = KUPL_ATOMIC_LD(&pf.chunk_index[target_tid].value);
        int64_t target_index = pf.chunk_index[target_tid].target;
        while (kupl_likely(chunk_index < target_index)) {
            chunk_index = (int64_t)KUPL_ARCH_ATOMIC_ADD_RLX(&pf.chunk_index[target_tid].value, 1);
            if (chunk_index >= target_index) {
                break;
            }
            kupl_compute_chunk_range(range, pf, chunk_index);
            call();
        }
        target_tid++;
    }
}

static void kupl_dynamic_policy_func(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum)
{
    kupl_dynamic_policy_impl(range, pf, tid, tnum, [&]() { pf.func(&range, pf.args, tid, tnum); });
}

static void kupl_reduce_dynamic_policy_func(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum)
{
    auto private_args = kupl_reduce_args_dup(pf.shared_args);
    if (kupl_unlikely(private_args == nullptr)) {
        return;
    }
    kupl_dynamic_policy_impl(range, pf, tid, tnum, [&]() { pf.rd_func(&range, pf.args, tid, tnum, private_args); });
}

static kupl_always_inline void kupl_task_reduce_buffers_init(kupl_pf_t &pf)
{
    KUPL_FOR_EACH_LIMIT_EGROUP(pf.egroup, pf.num_threads, eid, eidx, {
        g_pf[eid].master_eid = pf.master_eid;
        kupl_reduce_args_dup_impl(pf.shared_args, g_pf[eid], (int)eid);
    });
}

static kupl_always_inline void kupl_calc_chunk_info(kupl_pf_t &pf)
{
    int64_t total_chunks = 1;
    auto &nd_range = pf.range->nd_range;
    for (int d = 0; d < pf.range->dim; d++) {
        pf.chunk_info[d].chunksize = nd_range[d].step * nd_range[d].blocksize;
        if (pf.chunk_info[d].chunksize > 0) {
            pf.chunk_info[d].chunks = kupl_divup(nd_range[d].upper - nd_range[d].lower, pf.chunk_info[d].chunksize);
        } else {
            pf.chunk_info[d].chunks = kupl_divup(nd_range[d].lower - nd_range[d].upper, -pf.chunk_info[d].chunksize);
        }
        total_chunks *= pf.chunk_info[d].chunks;
    }
    pf.total_chunks = total_chunks;
}

template <typename SetupFunc, typename FinalFunc>
static kupl_always_inline int kupl_task_policy_impl(kupl_pf_t &pf, SetupFunc setup, FinalFunc final)
{
    auto graph = kupl_pf_graph_get();
    if (kupl_unlikely(graph == nullptr)) {
        return KUPL_ERROR;
    }

    kupl_calc_chunk_info(pf);

    int64_t total_chunks = pf.total_chunks;
    int64_t num_threads = (int64_t)pf.num_threads;
    int64_t base_chunks = total_chunks / num_threads;
    int64_t remain_chunks = total_chunks % num_threads;

    KUPL_ATOMIC_UINT32 task_cnt_ref = {0};
    int geid = kupl_get_executor_num();

    setup();

    int64_t task_id = 0;
    KUPL_FOR_EACH_LIMIT_EGROUP(pf.egroup, pf.num_threads, eid, eidx, {
        int64_t num_tasks = base_chunks + (eidx < remain_chunks);
        for (int64_t j = 0; j < num_tasks; j++) {
            kupl_tb_desc_t user_desc = {.field_mask = KUPL_TB_DESC_FIELD_EXECUTOR_ID,
                                        .func = task_policy_loop_func,
                                        .args = nullptr,
                                        .executor_id = (int)eid};
            kupl_task_param_t task_param = {
                .super = {.type = KUPL_TB_TYPE_TASK, .user_desc = &user_desc, .graph = graph, .count = &task_cnt_ref},
                .kind = KUPL_TASK_KIND_COMM_DYNAMIC,
                .inplace = nullptr,
                .udata_size = sizeof(kupl_task_args_t)};
            kupl_task_h task = kupl_task_init(&task_param, geid);
            if (kupl_unlikely(task == nullptr)) {
                kupl_error("task create failed");
                task_id++;
                continue;
            }

            task->tb.args = task->udata;
            auto args = reinterpret_cast<kupl_task_args_t *>(task->udata);
            args->task_id = task_id;
            args->master_eid = pf.master_eid;

            kupl_sched_add_tb(graph->sched, &task->tb);
            task_id++;
        }
    });

    while (task_cnt_ref.load() != 0) {
        if (kupl_sched_execute_tb(graph->sched) == KUPL_FINISHED) {
            break;
        }
    }

    final();
    pf.post_func = nullptr;
    return KUPL_OK;
}

static int kupl_task_policy_func(kupl_pf_t &pf)
{
    return kupl_task_policy_impl(pf, []() {}, []() {});
}

static int kupl_reduce_task_policy_func(kupl_pf_t &pf)
{
    return kupl_task_policy_impl(
        pf, [&]() { kupl_task_reduce_buffers_init(pf); }, [&]() { kupl_reduce_post_func(pf, 0, pf.num_threads); });
}

static void kupl_loop_func(void *args)
{
    (void)args;
    static thread_local kupl_nd_range_t range;
    call_cnt++;
    static thread_local int geid = kupl_get_executor_num();
    int master_eid = g_pf[geid].master_eid;
    auto &pf = g_pf[master_eid];
    g_ult[geid].tb.egroup = pf.egroup;

    int tid = static_cast<int>(pf.egroup->cur.eid2lid[geid]);
    pf.policy_func(range, pf, tid, pf.num_threads);

    g_ult[geid].tb.egroup = nullptr;
    auto &barrier = kupl::FlagBarrier::getInstance();
    barrier.leave(pf.egroup, pf.num_threads, master_eid);
    if (pf.post_func) {
        pf.post_func(pf, tid, pf.num_threads);
    }

    call_cnt--;
}

static kupl_always_inline void kupl_static_policy_prepare(kupl_pf_t &pf)
{
    kupl_calc_chunk_info(pf);
    if (pf.range != nullptr && pf.range->dim == 1) {
        pf.policy_func = kupl_static_policy_func;
    } else {
        pf.policy_func = kupl_nd_static_policy_func;
    }
}

static kupl_always_inline void kupl_dynamic_policy_prepare_common(kupl_pf_t &pf)
{
    kupl_calc_chunk_info(pf);
    auto chunks_per_thread = pf.total_chunks / (int64_t)pf.num_threads;
    auto chunks_remain = pf.total_chunks % (int64_t)pf.num_threads;
    for (int64_t i = 0; i < (int64_t)pf.num_threads; i++) {
        auto tmp_chunk = i * chunks_per_thread;
        pf.chunk_index[i].value.store(tmp_chunk + kupl_min(tmp_chunk, chunks_remain), std::memory_order_release);
        pf.chunk_index[i].target = (i + 1) * chunks_per_thread + kupl_min((i + 1) * chunks_per_thread, chunks_remain);
    }
}

static kupl_always_inline void kupl_dynamic_policy_prepare(kupl_pf_t &pf)
{
    kupl_dynamic_policy_prepare_common(pf);
    pf.policy_func = kupl_dynamic_policy_func;
}

static kupl_always_inline void kupl_reduce_static_policy_prepare(kupl_pf_t &pf)
{
    kupl_calc_chunk_info(pf);
    if (pf.range->dim == 1) {
        pf.policy_func = kupl_reduce_static_policy_func;
    } else {
        pf.policy_func = kupl_reduce_nd_static_policy_func;
    }
    pf.post_func = kupl_reduce_post_func;
}

static kupl_always_inline void kupl_reduce_dynamic_policy_prepare(kupl_pf_t &pf)
{
    kupl_dynamic_policy_prepare_common(pf);
    pf.policy_func = kupl_reduce_dynamic_policy_func;
    pf.post_func = kupl_reduce_post_func;
}

static kupl_always_inline void kupl_reduce_policy_prepare(kupl_pf_t &pf)
{
    if (pf.range == nullptr) {
        pf.policy_func = kupl_reduce_no_range_policy_func;
    } else {
        switch (pf.policy) {
            case KUPL_LOOP_POLICY_STATIC:
                kupl_reduce_static_policy_prepare(pf);
                break;
            case KUPL_LOOP_POLICY_DYNAMIC:
                kupl_reduce_dynamic_policy_prepare(pf);
                break;
            case KUPL_LOOP_POLICY_TASK:
                pf.policy_func = nullptr;
                pf.post_func = nullptr;
                break;
            default:
                kupl_error("Unsupported policy for reduce: %d", pf.policy);
        }
    }
}

static int kupl_omp_parallel(kupl_pf_t &pf)
{
    kupl_nd_range_t local_range;
    if (pf.range != nullptr) {
        if (kupl_unlikely(pf.range->dim != 1 || pf.policy != KUPL_LOOP_POLICY_STATIC)) {
            return kupl_log_error_return(ERROR, "omp backend only supports static policy with 1D range");
        }
        local_range = *pf.range;
    }

#pragma omp parallel for num_threads(pf.num_threads) private(local_range)
    for (int i = 0; i < pf.num_threads; ++i) {
        call_cnt++;
        int geid = kupl_get_executor_num();
        g_ult[geid].tb.egroup = pf.egroup;
        pf.policy_func(local_range, pf, i, pf.num_threads);
        g_ult[geid].tb.egroup = nullptr;
        call_cnt--;
    }

    if (pf.post_func) {
        pf.post_func(pf, 0, pf.num_threads);
        pf.post_func = nullptr;
    }
    return KUPL_OK;
}

static kupl_always_inline int kupl_parallel_for_check(kupl_parallel_for_desc_t *desc, void *func)
{
    KUPL_PTRACE_START(KUPL_PTRACE_PARALLEL_FOR_CHECK);
    if (kupl_unlikely(func == nullptr || desc == nullptr ||
                      (desc->field_mask & KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT) !=
                          KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT)) {
        return kupl_log_error_return(ERROR, "Invalid parameter");
    }
    auto range = desc->range;
    auto policy = desc->policy;
    if (range != nullptr) {
        if ((policy == KUPL_LOOP_POLICY_TASK && range->dim != 1) ||
            (policy != KUPL_LOOP_POLICY_STATIC && policy != KUPL_LOOP_POLICY_DYNAMIC &&
             policy != KUPL_LOOP_POLICY_TASK)) {
            return kupl_log_error_return(ERROR, "kupl not support current policy or dim yet!");
        }
        if (kupl_check_range(range, policy) != KUPL_OK) {
            return kupl_log_error_return(WARN, "kupl parallel for range invalid.");
        }
    }

    // set egroup if egroup is null
    if (desc->egroup == nullptr) {
        auto current_egroup = kupl_get_current_egroup();
        if (current_egroup == nullptr) {
            desc->egroup = g_egroup;
        } else {
            desc->egroup = current_egroup;
        }
    } else {
        auto egroup = desc->egroup;
        uint32_t egroup_size = egroup->cur.size;
        if (egroup_size == 0) {
            return kupl_log_error_return(ERROR, "Invalid egroup size: %u", egroup_size);
        }
    }
    if (desc->concurrency == KUPL_CONCURRENCY_DEFAULT) {
        desc->concurrency = g_concurrency_default;
    } else {
        int num_threads = desc->concurrency;
        if (num_threads <= 0 || num_threads > kupl_get_num_executors()) {
            return kupl_log_error_return(ERROR, "Invalid num_threads: %d", num_threads);
        }
    }
    KUPL_PTRACE_END(KUPL_PTRACE_PARALLEL_FOR_CHECK);
    return KUPL_OK;
}

static kupl_always_inline int kupl_parallel_for_num_threads(kupl_parallel_for_desc_t *desc, kupl_egroup_h egroup)
{
    KUPL_PTRACE_START(KUPL_PTRACE_PARALLEL_FOR_NUM_THREADS);
    int num_threads = desc->concurrency;
    auto range = desc->range;
    if (range != nullptr) { // include no range condition
        int64_t total_chunks = 1;
        for (int i = 0; i < range->dim; i++) {
            int64_t lower = range->nd_range[i].lower;
            int64_t upper = range->nd_range[i].upper;
            int64_t step = range->nd_range[i].step;
            int64_t blocksize = range->nd_range[i].blocksize;
            // there has been checked outside overflow and int64_t to int
            if (lower < upper) {
                total_chunks *= (int64_t)kupl_divup(upper - lower, blocksize * step);
            } else {
                total_chunks *= (int64_t)kupl_divup(lower - upper, -blocksize * step);
            }
        }
        num_threads = (int)kupl_min((int64_t)num_threads, total_chunks);
    }
    num_threads = kupl_min(num_threads, (int)kupl_egroup_get_cur_size(egroup));
    num_threads = kupl_min(num_threads, kupl_get_kernel_concurrency_inner());
    KUPL_PTRACE_END(KUPL_PTRACE_PARALLEL_FOR_NUM_THREADS);
    return num_threads;
}

int kupl_parallel_for(kupl_parallel_for_desc_t *desc, kupl_pf_func_t func, void *args)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(kupl_parallel_for_check(desc, (void *)func) == KUPL_ERROR)) {
        return KUPL_ERROR;
    }
    auto egroup = desc->egroup;
    int num_threads = kupl_parallel_for_num_threads(desc, egroup);
    if (kupl_unlikely(call_cnt >= 1 || num_threads == 1 || kupl_is_expand_executor())) {
        auto ult = kupl_executor_get_pf_ult();
        ult->tb.egroup = egroup;
        func(desc->range, args, 0, 1);
        ult->tb.egroup = nullptr;
        return KUPL_OK;
    }
    static thread_local int geid = kupl_get_executor_num();
    int master_eid;
    if (CPU_ISSET(geid, &egroup->cur.eid_set)) {
        master_eid = geid;
    } else {
        master_eid = (int)egroup->cur.min_eid;
    }
    kupl_pf_t &pf = g_pf[master_eid];
    // 1. set pf value
    pf.func = func;
    pf.args = args;
    pf.range = desc->range;
    pf.egroup = egroup;
    pf.num_threads = num_threads;
    pf.policy = desc->policy;

    if (pf.range != nullptr) {
        switch (desc->policy) {
            case KUPL_LOOP_POLICY_STATIC:
                kupl_static_policy_prepare(pf);
                break;
            case KUPL_LOOP_POLICY_DYNAMIC:
                kupl_dynamic_policy_prepare(pf);
                break;
            case KUPL_LOOP_POLICY_TASK:
                pf.master_eid = master_eid;
                return kupl_task_policy_func(pf);
            default:
                return kupl_log_error_return(ERROR, "invalid loop policy");
        }
    } else {
        pf.policy_func = kupl_no_range_policy_func;
    }

    // omp backend and not in parallel
    if (kupl_unlikely(kupl_backend_type_get() == KUPL_BACKEND_OMP && omp_in_parallel() == 0)) {
        return kupl_omp_parallel(pf);
    }

    auto &barrier = kupl::FlagBarrier::getInstance();
    if (kupl_executor_get_current_tb() != nullptr) {
        barrier.wait(pf.egroup, pf.num_threads);
    }

    KUPL_FOR_EACH_LIMIT_EGROUP(pf.egroup, pf.num_threads, eid, eidx, { g_pf[eid].master_eid = master_eid; });

    // 2. notify target threads to execute ult
    barrier.notify(pf.egroup, pf.num_threads);
    if (master_eid == geid) {
        barrier.arrive(master_eid);
        kupl_loop_func(nullptr);
    } else {
        barrier.wait(pf.egroup, pf.num_threads);
    }
    return KUPL_OK;
}

static kupl_always_inline int kupl_reduce_check(kupl_parallel_for_desc_t *desc, void *func, kupl_reduce_args_t *rd_args)
{
    if (kupl_unlikely(kupl_parallel_for_check(desc, func) == KUPL_ERROR)) {
        return KUPL_ERROR;
    }

    if (kupl_unlikely((rd_args == nullptr) || (rd_args->num <= 0) || (rd_args->num > KUPL_RD_NUM_MAX) ||
                      (rd_args->items == nullptr))) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(desc->policy != KUPL_LOOP_POLICY_STATIC && desc->policy != KUPL_LOOP_POLICY_DYNAMIC &&
                      desc->policy != KUPL_LOOP_POLICY_TASK)) {
        kupl_error("Invalid policy: %d, only STATIC, DYNAMIC and TASK supported for reduce", desc->policy);
        return KUPL_ERROR;
    }
    for (int i = 0; i < rd_args->num; i++) {
        int op = rd_args->items[i].op;
        int type = rd_args->items[i].type;
        if (kupl_unlikely((op < KUPL_RD_ADD) || (op > KUPL_RD_MIN))) {
            kupl_error("Invalid reduce option: %d", op);
            return KUPL_ERROR;
        }
        if (kupl_unlikely((type < KUPL_DATATYPE_INT) || (type > KUPL_DATATYPE_DOUBLE_COMPLEX))) {
            kupl_error("Invalid reduce datatype: %d", type);
            return KUPL_ERROR;
        }
        if (kupl_unlikely((op == KUPL_RD_MAX && type == KUPL_DATATYPE_FLOAT_COMPLEX) ||
                          (op == KUPL_RD_MIN && type == KUPL_DATATYPE_FLOAT_COMPLEX) ||
                          (op == KUPL_RD_MAX && type == KUPL_DATATYPE_DOUBLE_COMPLEX) ||
                          (op == KUPL_RD_MIN && type == KUPL_DATATYPE_DOUBLE_COMPLEX))) {
            kupl_error("complex type do not support min max op.");
            return KUPL_ERROR;
        }
    }
    return KUPL_OK;
}

int kupl_parallel_for_reduce(kupl_parallel_for_desc_t *desc, kupl_pf_reduce_func_t func, void *args,
                             kupl_reduce_args_t *rd_args)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(kupl_reduce_check(desc, (void *)func, rd_args) == KUPL_ERROR)) {
        return KUPL_ERROR;
    }
    auto egroup = desc->egroup;
    int num_threads = kupl_parallel_for_num_threads(desc, egroup);
    static thread_local int geid = kupl_get_executor_num();

    // quick path
    if (kupl_unlikely(call_cnt >= 1 || num_threads == 1)) {
        // prepare
        auto private_args = kupl_reduce_args_dup(rd_args);
        if (kupl_unlikely(private_args == nullptr)) {
            return KUPL_ERROR;
        }
        func(desc->range, args, 0, 1, private_args);

        // post
        do_reduce(rd_args, g_pf[geid].rd_data);
        return KUPL_OK;
    }

    int master_eid;
    if (CPU_ISSET(geid, &egroup->cur.eid_set)) {
        master_eid = geid;
    } else {
        master_eid = (int)egroup->cur.min_eid;
    }
    kupl_pf_t &pf = g_pf[master_eid];
    // 1. set pf value
    pf.rd_func = func;
    pf.shared_args = rd_args;
    pf.args = args;
    pf.range = desc->range;
    pf.egroup = egroup;
    pf.policy = desc->policy;
    pf.num_threads = num_threads;
    pf.post_func = kupl_reduce_post_func;

    kupl_reduce_policy_prepare(pf);

    // omp backend and not in parallel
    if (kupl_unlikely(kupl_backend_type_get() == KUPL_BACKEND_OMP && omp_in_parallel() == 0)) {
        return kupl_omp_parallel(pf);
    }

    // task policy has its own execution path
    if (pf.policy == KUPL_LOOP_POLICY_TASK && pf.range != nullptr) {
        pf.master_eid = master_eid;
        return kupl_reduce_task_policy_func(pf);
    }

    KUPL_FOR_EACH_LIMIT_EGROUP(pf.egroup, pf.num_threads, eid, eidx, { g_pf[eid].master_eid = master_eid; });

    // 2. notify target threads to execute ult
    auto &barrier = kupl::FlagBarrier::getInstance();
    barrier.notify(pf.egroup, pf.num_threads);

    if (master_eid == geid) {
        barrier.arrive(master_eid);
        kupl_loop_func(nullptr);
    } else {
        barrier.wait(pf.egroup, pf.num_threads);
    }
    pf.post_func = nullptr;
    return KUPL_OK;
}

bool kupl_in_parallel()
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return false;
    }
    return call_cnt != 0;
}

int kupl_get_kernel_concurrency()
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return 1;
    }
    if (call_cnt == 1) {
        static thread_local int geid = kupl_get_executor_num();
        int master_eid = g_pf[geid].master_eid;
        auto &pf = g_pf[master_eid];
        return pf.num_threads;
    } else {
        return 1;
    }
}

static int kupl_global_egroup_create(int num_threads)
{
    // create global egroup
    static int executor[KUPL_EXECUTOR_ID_MAX];
    for (int i = 0; i < num_threads; i++) {
        executor[i] = i;
    }
    g_egroup = kupl_egroup_create(executor, num_threads);
    if (kupl_unlikely(g_egroup == nullptr)) {
        return kupl_log_error_return(ERROR, "parallel for egroup create failed");
    }
    return KUPL_OK;
}

static void kupl_global_egroup_destroy()
{
    kupl_egroup_destroy(g_egroup);
    g_egroup = nullptr;
}

static void kupl_global_ult_fini()
{
    kupl_memory_free(g_ult, 0);
}

static int kupl_global_ult_init(int num_threads)
{
    g_ult = (kupl_ult_t *)kupl_memory_calloc((size_t)num_threads * sizeof(kupl_ult_t), 0);
    if (kupl_unlikely(g_ult == nullptr)) {
        kupl_error("kupl_global_ult_init malloc failed.");
        return KUPL_ERROR;
    }
    auto sched = kupl_get_global_sched();
    if (kupl_unlikely(sched == nullptr)) {
        kupl_error("kupl_global_ult_init get sched failed.");
        goto err;
    }

    for (int i = 0; i < num_threads; i++) {
        kupl_ult_desc_t ult_desc = {
            .field_mask = KUPL_TB_DESC_FIELD_FLAG | KUPL_TB_DESC_FIELD_EXECUTOR_ID,
            .func = kupl_loop_func,
            .args = nullptr,
            .flag = KUPL_TB_FLAG_HYBRID_OUTER,
            .executor_id = i,
        };
        kupl_ult_param_t param = {
            .super =
                {
                    .type = KUPL_TB_TYPE_ULT,
                    .user_desc = &ult_desc,
                    .graph = nullptr,
                    .count = nullptr,
                },
            .kind = KUPL_ULT_KIND_COMM_DYNAMIC,
            .inplace = &g_ult[i],
        };
        kupl_ult_init(&param, 0);

        if (kupl_unlikely(kupl_sched_add_ult(sched, &g_ult[i]) != KUPL_OK)) {
            kupl_error("kupl_global_ult_init add ult failed.");
            goto err;
        }
        kupl_executor_set_pf_ult(&g_ult[i], i);
    }
    return KUPL_OK;
err:
    kupl_global_ult_fini();
    return KUPL_ERROR;
}

int kupl_pf_init()
{
    auto host_info = kupl_get_host_info();
    int num_threads = host_info->avail_pu_cnt;
    g_concurrency_default = num_threads;
    g_pf = (kupl_pf_t *)kupl_calloc(sizeof(kupl_pf_t), (size_t)num_threads);
    if (kupl_unlikely(g_pf == nullptr)) {
        goto err;
    }
    for (int i = 0; i < num_threads; i++) {
        g_pf[i].chunk_index =
            (kupl_pf::aligned_index *)kupl_calloc(sizeof(kupl_pf::aligned_index), (size_t)num_threads);
        if (kupl_unlikely(g_pf[i].chunk_index == nullptr)) {
            goto err;
        }
    }
    if (kupl_unlikely(kupl_global_ult_init(num_threads) != KUPL_OK)) {
        goto err;
    }
    if (kupl_unlikely(kupl_global_egroup_create(num_threads) != KUPL_OK)) {
        goto err;
    }
    return KUPL_OK;
err:
    kupl_pf_fini();
    return KUPL_ERROR;
}

void kupl_pf_fini()
{
    kupl_global_egroup_destroy();
    kupl_global_ult_fini();
    if (g_pf != nullptr) {
        for (int i = 0; i < g_concurrency_default; i++) {
            if (g_pf[i].chunk_index != nullptr) {
                kupl_free_inner(g_pf[i].chunk_index);
            }
            if (g_pf[i].rd_item != nullptr) {
                kupl_memory_free(g_pf[i].rd_item, i);
            }
            if (g_pf[i].rd_data != nullptr) {
                kupl_memory_free(g_pf[i].rd_data, i);
            }
        }
        kupl_free_inner(g_pf);
        g_pf = nullptr;
    }
}

namespace kupl {
static void lambda_pf_func(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
    pf_lambda *func = (pf_lambda *)args;
    (*func)(nd_range, tid, tnum);
}

int parallel_for(kupl_parallel_for_desc_t *desc, const pf_lambda &func)
{
    pf_lambda *func_ptr = const_cast<pf_lambda *>(static_cast<const pf_lambda *>(&func));
    return kupl_parallel_for(desc, lambda_pf_func, func_ptr);
}
} // namespace kupl
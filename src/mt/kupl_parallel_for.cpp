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

static kupl_always_inline
kupl_graph_h kupl_pf_graph_get()
{
    KUPL_PTRACE_START(KUPL_PTRACE_PARALLEL_FOR_GRAPH_GET);
    kupl_graph_h graph = kupl_get_current_graph();
    if (graph == nullptr) {
        graph = kupl_get_global_graph();
    }
    KUPL_PTRACE_END(KUPL_PTRACE_PARALLEL_FOR_GRAPH_GET);

    return graph;
}

static kupl_always_inline
int get_max_range_dim(kupl_nd_range_t& range) {
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

typedef struct kupl_taskloop_pf_args {
    int                     range_num;      // current support range_num == 1
    kupl_nd_range_t         range_list;     // current support range_num == 1
    kupl_pf_func_t          func;
    void                    *args;
    kupl_barrier_h          barrier;
    int                     local_tid;
    int                     local_tnum;
} kupl_taskloop_pf_args_t;

static void task_policy_loop_func(void *args)
{
    call_cnt++;
    auto loop_args = (kupl_taskloop_pf_args_t *)args;
    if (args == nullptr) {
        kupl_warn("task_policy_loop_func input nullptr");
        return;
    }
    for (int i = 0; i < loop_args->range_num; i++) {
        loop_args->func(&(loop_args->range_list), loop_args->args, loop_args->local_tid, loop_args->local_tnum);
    }
    call_cnt--;
}

int kupl_invoke_parallel(kupl_parallel_func_t func, void *args, int num_threads)
{
    kupl_parallel_for_desc_t desc = {
        .field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT,
        .range = nullptr,
        .egroup = nullptr,
        .concurrency = num_threads,
        .policy = KUPL_LOOP_POLICY_STATIC
    };
    return kupl::parallel_for(&desc, [&](const kupl_nd_range_t *nd_range, const int tid, const int tnum) {
        (void)nd_range;
        func(args, tid, tnum);
    });
}

static void kupl_static_policy_func(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum)
{
    range.dim = pf.range->dim;
    auto &nd_range = range.nd_range[0];
    auto chunksize = pf.chunk_info[0].chunksize;

    int64_t num_threads = pf.num_threads;
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
    pf.func(&range, pf.args, tid, tnum);
}

static void divide_range(kupl_nd_range_t &range, kupl_pf_t &pf, size_t target_tid, size_t head_tid, size_t num_threads)
{
    // the end condition, divide to only one num_threads
    if (num_threads == 1 && target_tid == head_tid) {
        return;
    }
    // save nd_range infomation to recover changes by the recursive
    int divide_dim = get_max_range_dim(range);
    size_t left_threads = num_threads >> 1;
    size_t right_threads = num_threads - left_threads;

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

static void kupl_nd_static_policy_func(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum)
{
    range = *pf.range;
    divide_range(range, pf, (size_t)tid, 0, (size_t)tnum);
    pf.func(&range, pf.args, tid, tnum);
}

static void kupl_dynamic_policy_func(kupl_nd_range_t &range, kupl_pf_t &pf, int tid, int tnum)
{
    range = *pf.range;
    int target_tid = tid;
    for (int64_t loop_count = 0; loop_count < (int64_t)tnum; loop_count++) {
        if (target_tid >= tnum) {
            target_tid = 0;
        }
        int64_t chunk_index = (int64_t)KUPL_ATOMIC_LD(&pf.chunk_index[target_tid].value);
        int64_t target_index = pf.chunk_index[target_tid].target;
        while (kupl_likely(chunk_index < target_index)) {
            chunk_index = (int64_t)KUPL_ARCH_ATOMIC_ADD_RLX(&pf.chunk_index[target_tid].value, 1);
            if (chunk_index >= target_index) {
                break;
            }
            for (int d = range.dim - 1; d >= 0; d--) {
                auto nd_index = chunk_index % pf.chunk_info[d].chunks;
                chunk_index = chunk_index / pf.chunk_info[d].chunks;
                int64_t chunksize = pf.chunk_info[d].chunksize;
                range.nd_range[d].lower = pf.range->nd_range[d].lower + nd_index * chunksize;
                if (chunksize > 0) {
                    range.nd_range[d].upper = kupl_min(range.nd_range[d].lower + chunksize,
                                                       pf.range->nd_range[d].upper);
                } else {
                    range.nd_range[d].upper = kupl_max(range.nd_range[d].lower + chunksize,
                                                       pf.range->nd_range[d].upper);
                }
            }
            pf.func(&range, pf.args, tid, tnum);
        }
        target_tid++;
    }
}

static int kupl_task_policy_func(kupl_pf_t &pf)
{
    auto graph = kupl_pf_graph_get();
    if (kupl_unlikely(graph == nullptr)) {
        return KUPL_ERROR;
    }
    int64_t num_threads = (int64_t)pf.num_threads;
    kupl_nd_range_t *range = pf.range;
    int dim = range->dim;
    int64_t lower = range->nd_range[0].lower;
    int64_t upper = range->nd_range[0].upper;
    int64_t step = range->nd_range[0].step;
    int64_t blocksize = range->nd_range[0].blocksize;
    int64_t chunksize = step * blocksize;
    int64_t total_chunks = kupl_divup(upper - lower, chunksize);
    int64_t base_chunks = total_chunks / num_threads;
    int64_t remain_chunks = total_chunks % num_threads;
    int64_t tmp_lower = lower;
    KUPL_ATOMIC_UINT32 task_cnt_ref = {0};

    int geid = kupl_get_executor_num();

    // 5 tasks assigned to 3 threads
    // base_chunks: 1, remain_chunks: 2
    // 0 1 3
    // x 2 4

    int64_t current_thread = 0;
    KUPL_FOR_EACH_EGROUP(pf.egroup, eid, eidx, {
        if (current_thread >= num_threads) {
            break;
        }
        int64_t num_tasks = base_chunks + ((current_thread + remain_chunks) >= num_threads);
        for (int64_t j = 0; j < num_tasks; j++) {
            kupl_tb_desc_t user_desc = {
                .field_mask     = KUPL_TB_DESC_FIELD_EXECUTOR_ID,
                .func           = task_policy_loop_func,
                .args           = nullptr,
                .executor_id    = (int)eid
            };
            kupl_task_param_t task_param = {
                .super = {
                    .type       = KUPL_TB_TYPE_TASK,
                    .user_desc  = &user_desc,
                    .graph      = graph,
                    .count      = &task_cnt_ref
                },
                .kind           = KUPL_TASK_KIND_COMM_DYNAMIC,
                .inplace        = nullptr,
                .udata_size     = sizeof(kupl_taskloop_pf_args_t)
            };
            kupl_task_h task_inner = kupl_task_init(&task_param, geid);
            if (kupl_unlikely(task_inner == nullptr)) {
                kupl_error("task create failed");
                continue;
            }

            task_inner->tb.args = task_inner->udata;
            kupl_taskloop_pf_args_t *loop_args = reinterpret_cast<kupl_taskloop_pf_args_t*>(task_inner->udata);
            loop_args->range_num = 1;
            loop_args->func = pf.func;
            loop_args->args = pf.args;
            loop_args->barrier = nullptr;
            loop_args->local_tid = (int)eidx;
            loop_args->local_tnum = (int)num_threads;
            kupl_nd_range_t &range_list = loop_args->range_list;
            range_list.dim = dim;
            kupl_range_t &inner_range = range_list.nd_range[0];
            inner_range.lower = tmp_lower;
            tmp_lower += chunksize;
            inner_range.upper = kupl_min(upper, tmp_lower);
            inner_range.step = step;
            inner_range.blocksize = blocksize;
            kupl_sched_add_tb(graph->sched, &task_inner->tb);
        }
        current_thread ++;
    });
    while (task_cnt_ref.load() != 0) {
        if (kupl_sched_execute_tb(graph->sched) == KUPL_FINISHED) {
            break;
        }
    }
    return KUPL_OK;
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
    if (pf.range != nullptr) {
        pf.policy_func(range, pf, tid, pf.num_threads);
    } else {
        pf.func(nullptr, pf.args, tid, pf.num_threads);
    }

    g_ult[geid].tb.egroup = nullptr;
    auto &barrier = kupl::FlagBarrier::getInstance();
    barrier.leave(pf.egroup, pf.num_threads, master_eid);

    call_cnt--;
}

static kupl_always_inline
void kupl_calc_chunk_info(kupl_pf_t &pf)
{
    int64_t total_chunks = 1;
    auto &nd_range = pf.range->nd_range;
    for (int d = 0; d < pf.range->dim; d++) {
        pf.chunk_info[d].chunksize = nd_range[d].step * nd_range[d].blocksize;
        pf.chunk_info[d].chunks = kupl_divup(nd_range[d].upper - nd_range[d].lower, pf.chunk_info[d].chunksize);
        total_chunks *= pf.chunk_info[d].chunks;
    }
    pf.total_chunks = total_chunks;
}

static kupl_always_inline
void kupl_static_policy_prepare(kupl_pf_t &pf)
{
    kupl_calc_chunk_info(pf);
    if (pf.range != nullptr && pf.range->dim == 1) {
        pf.policy_func = kupl_static_policy_func;
    } else {
        pf.policy_func = kupl_nd_static_policy_func;
    }
}

static kupl_always_inline
void kupl_dynamic_policy_prepare(kupl_pf_t &pf)
{
    kupl_calc_chunk_info(pf);
    auto chunks_per_thread = pf.total_chunks / (int64_t)pf.num_threads;
    auto chunks_remain = pf.total_chunks % (int64_t)pf.num_threads;
    for (int64_t i = 0; i < (int64_t)pf.num_threads; i++) {
        auto tmp_chunk = i * chunks_per_thread;
        pf.chunk_index[i].value.store(tmp_chunk + kupl_min(tmp_chunk, chunks_remain), std::memory_order_release);
        pf.chunk_index[i].target = (i + 1) * chunks_per_thread + kupl_min((i + 1) * chunks_per_thread, chunks_remain);
    }
    pf.policy_func = kupl_dynamic_policy_func;
}

static int kupl_omp_parallel(kupl_pf_t &pf)
{
    if (pf.range == nullptr) {
        #pragma omp parallel for num_threads(pf.num_threads)
        for (int i = 0; i < pf.num_threads; ++i) {
            int tid = omp_get_thread_num();
            int geid = kupl_get_executor_num();
            g_ult[geid].tb.egroup = pf.egroup;
            pf.func(nullptr, pf.args, tid, pf.num_threads);
            g_ult[geid].tb.egroup = nullptr;
        }
        return KUPL_OK;
    }
    if (kupl_unlikely(pf.range->dim != 1 || pf.policy != KUPL_LOOP_POLICY_STATIC)) {
        return kupl_log_error_return(ERROR, "not support dim != 1 or policy != KUPL_LOOP_POLICY_STATIC");
    }

    kupl_static_policy_prepare(pf);

    kupl_nd_range_t local_range = *pf.range;
    #pragma omp parallel for num_threads(pf.num_threads) private(local_range)
    for (int i = 0; i < pf.num_threads; ++i) {
        call_cnt++;
        int geid = kupl_get_executor_num();
        g_ult[geid].tb.egroup = pf.egroup;
        pf.policy_func(local_range, pf, i, pf.num_threads);
        g_ult[geid].tb.egroup = nullptr;
        call_cnt--;
    }
    return KUPL_OK;
}

static kupl_always_inline
int kupl_parallel_for_check(kupl_parallel_for_desc_t *desc, kupl_pf_func_t func)
{
    KUPL_PTRACE_START(KUPL_PTRACE_PARALLEL_FOR_CHECK);
    if (kupl_unlikely(func == nullptr || desc == nullptr || desc->field_mask != KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT)) {
        return kupl_log_error_return(ERROR, "Invalid parameter");
    }
    auto range = desc->range;
    auto policy = desc->policy;
    if (range != nullptr) {
        if ((policy == KUPL_LOOP_POLICY_TASK && range->dim != 1) ||
            (policy != KUPL_LOOP_POLICY_STATIC &&
             policy != KUPL_LOOP_POLICY_DYNAMIC &&
             policy != KUPL_LOOP_POLICY_TASK)) {
            return kupl_log_error_return(ERROR, "kupl not support current policy or dim yet!");
        }
        if (kupl_check_range(range, policy) != KUPL_OK) {
            return kupl_log_error_return(ERROR, "kupl parallel for range invalid.");
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

static kupl_always_inline
int kupl_parallel_for_num_threads(kupl_parallel_for_desc_t *desc, kupl_egroup_h egroup)
{
    KUPL_PTRACE_START(KUPL_PTRACE_PARALLEL_FOR_NUM_THREADS);
    int num_threads = desc->concurrency;
    auto range = desc->range;
    if (range != nullptr) {   // include no range condition
        int total_chunks = 1;
        for (int i = 0; i < range->dim; i++) {
            int64_t lower = range->nd_range[i].lower;
            int64_t upper = range->nd_range[i].upper;
            int64_t step = range->nd_range[i].step;
            int64_t blocksize = range->nd_range[i].blocksize;
            // there has been checked outside overflow and size_t to int
            if (lower < upper) {
                total_chunks *= (int)kupl_divup(upper - lower, blocksize * step);
            } else {
                total_chunks *= (int)kupl_divup(lower - upper, -blocksize * step);
            }
        }
        num_threads = kupl_min(num_threads, total_chunks);
    }
    num_threads = kupl_min(num_threads, (int)kupl_egroup_get_cur_size(egroup));
    KUPL_PTRACE_END(KUPL_PTRACE_PARALLEL_FOR_NUM_THREADS);
    return num_threads;
}

int kupl_parallel_for(kupl_parallel_for_desc_t *desc, kupl_pf_func_t func, void *args)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(kupl_parallel_for_check(desc, func) == KUPL_ERROR)) {
        return KUPL_ERROR;
    }
    auto egroup = desc->egroup;
    int num_threads = kupl_parallel_for_num_threads(desc, egroup);
    if (kupl_unlikely(call_cnt >= 1 || num_threads == 1 || kupl_is_expand_executor())) {
        func(desc->range, args, 0, 1);
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

    // omp backend and not in parallel
    if (kupl_unlikely(kupl_backend_type_get() == KUPL_BACKEND_OMP && omp_in_parallel() == 0)) {
        pf.policy = desc->policy;
        return kupl_omp_parallel(pf);
    }

    if (pf.range != nullptr) {
        switch (desc->policy) {
            case KUPL_LOOP_POLICY_STATIC:
                kupl_static_policy_prepare(pf);
                break;
            case KUPL_LOOP_POLICY_DYNAMIC:
                kupl_dynamic_policy_prepare(pf);
                break;
            case KUPL_LOOP_POLICY_TASK:
                return kupl_task_policy_func(pf);
            default:
                return kupl_log_error_return(ERROR, "invalid loop policy");
        }
    }
    auto &barrier = kupl::FlagBarrier::getInstance();
    if (kupl_executor_get_current_tb() != nullptr) {
        barrier.wait(pf.egroup, pf.num_threads);
    }

    KUPL_FOR_EACH_LIMIT_EGROUP(pf.egroup, pf.num_threads, eid, eidx, {
        g_pf[eid].master_eid = master_eid;
    });

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

static int kupl_global_egroup_create(int num_threads)
{
    // create global egroup
    static int executor[KUPL_EXECUTOR_ID_MAX];
    for (int i = 0; i < num_threads; i++) {
        executor[i] = i;
    }
    g_egroup = kupl_egroup_create(executor, num_threads);
    if (g_egroup == nullptr) {
        return kupl_log_error_return(ERROR, "parallel for egroup create failed");
    }
    return KUPL_OK;
}

static void kupl_global_egroup_destroy()
{
    kupl_egroup_destroy(g_egroup);
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
            .func       = kupl_loop_func,
            .args       = nullptr,
            .flag       = KUPL_TB_FLAG_HYBRID_OUTER,
            .executor_id = i,
        };
        kupl_ult_param_t param = {
            .super = {
                .type           = KUPL_TB_TYPE_ULT,
                .user_desc      = &ult_desc,
                .graph          = nullptr,
                .count          = nullptr,
            },
            .kind               = KUPL_ULT_KIND_COMM_DYNAMIC,
            .inplace            = &g_ult[i],
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
        kupl_pf_fini();
        return KUPL_ERROR;
    }
    for (int i = 0; i < num_threads; i++) {
        g_pf[i].chunk_index = (kupl_pf::aligned_index *)kupl_calloc(sizeof(kupl_pf::aligned_index),
                                                                    (size_t)num_threads);
        if (kupl_unlikely(g_pf[i].chunk_index == nullptr)) {
            kupl_pf_fini();
            return KUPL_ERROR;
        }
    }
    if (kupl_unlikely(kupl_global_ult_init(num_threads) != KUPL_OK)) {
        kupl_pf_fini();
        return KUPL_ERROR;
    }
    if (kupl_unlikely(kupl_global_egroup_create(num_threads) != KUPL_OK)) {
        kupl_pf_fini();
        return KUPL_ERROR;
    }
    return KUPL_OK;
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
        pf_lambda* func_ptr = const_cast<pf_lambda*>(static_cast<const pf_lambda*>(&func));
        return kupl_parallel_for(desc, lambda_pf_func, func_ptr);
    }
}
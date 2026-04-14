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

#include "kupl_memcpy.h"
#include <memory.h>
#include <omp.h>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include "kupl.h"
#include "core/kupl_core.h"
#include "executor/backend/kupl_executor_backend.h"
#include "utils/config/kupl_config.h"
#include "utils/type/kupl_status.h"
#include "utils/sys/kupl_math.h"
#include "utils/arch/kupl_cache.h"
#include "mt/kupl_parallel_for.h"
#include "memory/mpool/kupl_mpool.h"
#include "utils/sys/kupl_dl_module.h"
#include "utils/sys/kupl_hardware.h"
#include "mt/kupl_queue.h"


static thread_local int core_index = SDMA_CHN_INDEX_INIT;
static bool sdma_memcpy_func_init = false;
static init_chn_sdma kupl_sdma_init_chn = nullptr;
static get_process_id_sdma kupl_sdma_get_process_id = nullptr;
static deinit_chn_sdma kupl_sdma_deinit_chn = nullptr;
static icopy_data_sdma kupl_sdma_icopy_data = nullptr;
static iwait_chn_sdma kupl_sdma_iwait_chn = nullptr;
static iquery_chn_sdma kupl_sdma_iquery_chn = nullptr;

bool kupl_sdma_memcpy_func_init()
{
    if (!g_sdma_func_init) {
        return false;
    }
    sdma_func_list_t func_l = get_sdma_dl_func_l();
    kupl_sdma_init_chn = func_l.kupl_sdma_init_chn;
    kupl_sdma_get_process_id = func_l.kupl_sdma_get_process_id;
    kupl_sdma_deinit_chn = func_l.kupl_sdma_deinit_chn;
    kupl_sdma_icopy_data = func_l.kupl_sdma_icopy_data;
    kupl_sdma_iwait_chn = func_l.kupl_sdma_iwait_chn;
    kupl_sdma_iquery_chn = func_l.kupl_sdma_iquery_chn;
    if (kupl_sdma_init_chn && kupl_sdma_get_process_id && kupl_sdma_deinit_chn
    && kupl_sdma_icopy_data && kupl_sdma_iwait_chn && kupl_sdma_iquery_chn) {
        return true;
    } else {
        return false;
    }
}

void kupl_sdma_memcpy_func_fini()
{
    kupl_sdma_init_chn = nullptr;
    kupl_sdma_get_process_id = nullptr;
    kupl_sdma_deinit_chn = nullptr;
    kupl_sdma_icopy_data = nullptr;
    kupl_sdma_iwait_chn = nullptr;
    kupl_sdma_iquery_chn = nullptr;
}

bool kupl_get_sdma_memcpy_func_init()
{
    return sdma_memcpy_func_init;
}

int kupl_graph_wait_req(void *chn, sdma_request_t *req, kupl_graph_h graph)
{
    int ret = kupl_sdma_iquery_chn(chn, req);
    while (ret == KUPL_SDMA_UNFINISHED) {
        kupl_sched_execute_tb(graph->sched);
        ret = kupl_sdma_iquery_chn(chn, req);
    }
    if (ret != KUPL_SDMA_FINISHED) {
        return kupl_log_error_return(ERROR, "sdma_wait_data failed, ret = %d", ret);
    }
    return KUPL_OK;
}

int kupl_sdma_wait_event(kupl_event_h event)
{
    if (kupl_unlikely(event == nullptr || (event->m_args == nullptr && event->req == nullptr)
        || !sdma_memcpy_func_init)) {
        return kupl_log_error_return(ERROR, "kupl sdma wait event failed.");
    }
    if (kupl_unlikely(KUPL_ATOMIC_LD(&event->status) == KUPL_EVENT_STATUS_COMPLETE)) {
        return KUPL_OK;
    }
    auto graph = kupl_get_global_graph();
    if (kupl_unlikely(graph == nullptr)) {
        return KUPL_ERROR;
    }
    int ret = KUPL_OK;
    if (event->q != nullptr) {
        sdma_request_t* request = &event->req->request;
        event->lock->lock(event->lock);
        if (event->req->flag) {
            event->req->flag = false;
            ret = kupl_graph_wait_req(event->q->sdma_chn, request, graph);
        }
        event->lock->unlock(event->lock);
    } else {
        int chn_index = ((kupl_sdma_async_t *)(event->m_args))->chn_index;
        sdma_request_t request = ((kupl_sdma_async_t *)(event->m_args))->request;
        ret = kupl_graph_wait_req(g_sdma_chns[chn_index], &request, graph);
    }

    return ret;
}

int kupl_sdma_query_req(void *chn, sdma_request_t *req)
{
    int ret = kupl_sdma_iquery_chn(chn, req);
    if (ret == KUPL_SDMA_UNFINISHED) {
        return KUPL_EVENT_STATUS_SUBMITTED;
    }
    if (ret != KUPL_SDMA_FINISHED) {
        kupl_error("kupl sdma memcpy failed, sdma error code: %d.", ret);
    }
    return KUPL_EVENT_STATUS_COMPLETE;
}

int kupl_sdma_wait_req(kupl_queue_h queue, sdma_request_t *request)
{
    auto graph = kupl_get_global_graph();
    if (kupl_unlikely(graph == nullptr)) {
        return KUPL_ERROR;
    }
    int ret = kupl_sdma_iquery_chn(queue->sdma_chn, request);
    while (ret == KUPL_SDMA_UNFINISHED) {
        kupl_sched_execute_tb(graph->sched);
        ret = kupl_sdma_iquery_chn(queue->sdma_chn, request);
    }
    if (ret != KUPL_SDMA_FINISHED) {
        return kupl_log_error_return(ERROR, "sdma_wait_data failed, ret = %d", ret);
    }
    return KUPL_OK;
}

int kupl_sdma_query_event(kupl_event_h event)
{
    if (kupl_unlikely(event == nullptr || (event->m_args == nullptr && event->req == nullptr)
        || !sdma_memcpy_func_init)) {
        kupl_error("kupl sdma query event failed.");
        return KUPL_EVENT_STATUS_COMPLETE;
    }
    if (kupl_unlikely(KUPL_ATOMIC_LD(&event->status) == KUPL_EVENT_STATUS_COMPLETE)) {
        return KUPL_EVENT_STATUS_COMPLETE;
    }
    int ret = KUPL_EVENT_STATUS_COMPLETE;
    if (event->q != nullptr) {
        sdma_request_t* request = &event->req->request;
        event->lock->lock(event->lock);
        if (event->req->flag) {
            ret = kupl_sdma_query_req(event->q->sdma_chn, request);
            if (ret == KUPL_EVENT_STATUS_COMPLETE) {
                event->req->flag = false;
            }
        }
        event->lock->unlock(event->lock);
    } else {
        int chn_index = ((kupl_sdma_async_t *)(event->m_args))->chn_index;
        sdma_request_t request = ((kupl_sdma_async_t *)(event->m_args))->request;
        ret = kupl_sdma_query_req(g_sdma_chns[chn_index], &request);
    }
    return ret;
}

int kupl_set_sdma_event(kupl_event_t *event, int chn_index)
{
    if (KUPL_ATOMIC_LD(&event->status) == KUPL_EVENT_STATUS_SUBMITTED) {
        return kupl_log_error_return(ERROR, "event is submitted, can't be submitted twice");
    }
    kupl_event_set_status(event, KUPL_EVENT_STATUS_SUBMITTED);

    event->type = KUPL_EVENT_TYPE_SDMA;
    kupl_sdma_async_t *args;
    if (kupl_unlikely(event->m_args == nullptr)) {
        int geid = kupl_get_executor_num();
        args = (kupl_sdma_async_t *)kupl_memory_calloc(sizeof(kupl_sdma_async_t), geid);
        if (args == nullptr) {
            kupl_event_set_status(event, KUPL_EVENT_STATUS_COMPLETE);
            return KUPL_ERROR;
        }
    } else {
        args = (kupl_sdma_async_t *)event->m_args;
    }
    args->chn_index = chn_index;
    event->m_args = args;
    event->q = nullptr;

    return KUPL_OK;
}

int kupl_set_sdma_event_req(kupl_event_t *event)
{
    if (KUPL_ATOMIC_LD(&event->status) == KUPL_EVENT_STATUS_SUBMITTED) {
        return kupl_log_error_return(ERROR, "event is submitted, can't be submitted twice");
    }
    kupl_event_set_status(event, KUPL_EVENT_STATUS_SUBMITTED);

    event->type = KUPL_EVENT_TYPE_SDMA;
    return KUPL_OK;
}

int kupl_set_sdma_async_event(kupl_event_t *event)
{
    if (KUPL_ATOMIC_LD(&event->status) == KUPL_EVENT_STATUS_SUBMITTED) {
        return kupl_log_error_return(ERROR, "event is submitted, can't be submitted twice");
    }
    kupl_event_set_status(event, KUPL_EVENT_STATUS_SUBMITTED);

    event->type = KUPL_EVENT_TYPE_SDMA_WAIT;
    return KUPL_OK;
}

static inline void kupl_sdma_wait_async_task(kupl_sdma_chn_h chn, sdma_request_t* request)
{
    auto graph = kupl_get_global_graph();
    if (kupl_unlikely(graph == nullptr)) {
        return;
    }

    int ret = kupl_sdma_iquery_chn(chn, request);
    while (ret == KUPL_SDMA_UNFINISHED) {
        kupl_sched_execute_tb(graph->sched);
        ret = kupl_sdma_iquery_chn(chn, request);
    }

    if (ret != KUPL_SDMA_FINISHED) {
        kupl_error("kupl sdma memcpy failed, sdma error code: %d.", ret);
    }
}

static inline void kupl_sdma_create_task(sdma_sqe_task_t& s_task, uint64_t src_addr, uint32_t src_sl,
                                         uint64_t dst_addr, uint32_t dst_sl, uint32_t stride_num,
                                         uint32_t src_pasid, uint32_t dst_pasid, uint32_t length, uint8_t qos)
{
    s_task.src_addr = src_addr;
    s_task.dst_addr = dst_addr;
    s_task.src_process_id = src_pasid;
    s_task.dst_process_id = dst_pasid;
    s_task.src_stride_len = src_sl;
    s_task.dst_stride_len = dst_sl;
    s_task.stride_num = stride_num;
    s_task.length = length;
    s_task.opcode = 0x0;
    s_task.next_sqe = nullptr;
    s_task.qos = qos & 0xF;
}

static inline int kupl_get_core_index()
{
    if (core_index == SDMA_CHN_INDEX_INIT) {
        core_index = kupl_get_self_affinity();
    }
    return core_index;
}

static int g_kupl_memcpy_threads = 0;
void kupl_set_kernel_concurrency(int num)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return;
    }
    if (num < 1 || num > kupl_get_num_executors()) {
        kupl_warn("number of threads out of the 1..kupl_get_num_executors() range,"
                   " so set the number to kupl_get_num_executors().");
        g_kupl_memcpy_threads = kupl_get_num_executors();
        return;
    }
    g_kupl_memcpy_threads = num;
}

int kupl_get_kernel_concurrency()
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (g_kupl_memcpy_threads == 0) {
        return kupl_get_num_executors();
    }
    return g_kupl_memcpy_threads;
}

static thread_local int g_kupl_memcpy_threads_local = 0;
void kupl_set_kernel_concurrency_local(int num)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return;
    }
    if (num < 1 || num > kupl_get_num_executors()) {
        kupl_warn("number of threads out of the [1..kupl_get_num_executors()] range,"
                   " so set the number to kupl_get_num_executors().");
        g_kupl_memcpy_threads_local = kupl_get_num_executors();
        return;
    }
    g_kupl_memcpy_threads_local = num;
}

int kupl_get_kernel_concurrency_local()
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (g_kupl_memcpy_threads_local == 0) {
        return kupl_get_kernel_concurrency();
    }
    return g_kupl_memcpy_threads_local;
}

static int g_kupl_memcpy_mt_threshold = 0;

int kupl_memcpy_init()
{
    // read memcpy env before main
    g_kupl_memcpy_mt_threshold = kupl_config_get_value(KUPL_MEMCPY_MT_THRESHOLD);
    g_kupl_memcpy_threads = kupl_config_get_value(KUPL_KERNEL_CONCURRENCY);

    if (kupl_arch_detect() != KUPL_CPU_HISILICOM_920F) {
        return KUPL_OK;
    }
    sdma_memcpy_func_init = kupl_sdma_memcpy_func_init();
    if (!sdma_memcpy_func_init) {
        kupl_sdma_memcpy_func_fini();
        kupl_warn("failed to init sdma memcpy func");
    }
    return KUPL_OK;
}

void* kupl_get_sdma_chn()
{
    if (sdma_memcpy_func_init) {
        int core_id = kupl_get_core_index();
        int fd_index = core_id / cores_per_sdma % KUPL_MAX_SDMA_DEVICE_SIZE;
        int id = g_sdma_chn_num[fd_index].fetch_add(1);
        if (kupl_unlikely(id < 0)) {
            kupl_error("create sdma channl failed");
            return nullptr;
        }
        int chn_index = fd_index * cores_per_sdma +
                        id % cores_per_sdma;
        if (kupl_unlikely(g_sdma_chns[chn_index] == nullptr)) {
            g_sdma_chns[chn_index] =
            kupl_sdma_init_chn(g_sdma_fd[fd_index], chn_index % cores_per_sdma);
            if (g_sdma_chns[chn_index] == nullptr) {
                kupl_error("create sdma channl failed");
                return nullptr;
            }
        }
        return g_sdma_chns[chn_index];
    }
    return nullptr;
}

void* kupl_get_sdma_chn_by_cid(int cid)
{
    if (kupl_unlikely(g_sdma_chns[cid] == nullptr)) {
        g_sdma_chns[cid] =
        kupl_sdma_init_chn(g_sdma_fd[cid / cores_per_sdma % KUPL_MAX_SDMA_DEVICE_SIZE], cid % cores_per_sdma);
        if (g_sdma_chns[cid] == nullptr) {
            kupl_error("creat sdma channl failed");
            return nullptr;
        }
    }
    return g_sdma_chns[cid];
}

void kupl_memcpy_fini()
{
    kupl_sdma_memcpy_func_fini();
}

typedef struct kupl_memcpy_task_package {
    void *dst;
    const void *src;
    size_t count;
} kupl_memcpy_task_package_t;

static void kupl_memcpy_pf_task(void* args, int local_tid, int local_tnum)
{
    kupl_memcpy_task_package_t* package = (kupl_memcpy_task_package_t*)args;
    size_t tid = (size_t)local_tid;
    size_t tnum = (size_t)local_tnum;
    void *dst = package->dst;
    const void *src = package->src;
    size_t count = package->count;
    size_t cache_nums = (count + KUPL_CACHE_LINE - 1) / KUPL_CACHE_LINE;
    size_t offset = (tid * (cache_nums / tnum) + kupl_min(tid, cache_nums % tnum)) * KUPL_CACHE_LINE;
    size_t len = (cache_nums / tnum + (tid < cache_nums % tnum)) * KUPL_CACHE_LINE;
    if (count > offset) {
        len = kupl_min(len, count - offset);
    } else {
        len = 0;
    }
    memcpy((char*)dst + offset, (const char*)src + offset, len);
}

static void kupl_memcpy_task(void* args)
{
    kupl_memcpy_task_package_t* package = (kupl_memcpy_task_package_t*)args;
    void *dst = package->dst;
    const void *src = package->src;
    size_t count = package->count;
    kupl_memcpy(dst, src, count);
    int geid = kupl_get_executor_num();
    kupl_memory_free(package, geid);
}

static void kupl_memcpy_sdma_task(void* args)
{
    kupl_sdma_req_t *req = (kupl_sdma_req_t *)args;
    kupl_sdma_chn_h chn = req->chn;

    kupl_sdma_icopy_data(chn, &(req->task), 1, &req->request);
    kupl_sdma_wait_async_task(chn, &req->request);
    int geid = kupl_get_executor_num();
    kupl_memory_free(req, geid);
}

int kupl_memcpy_check(void *dst, const void *src, size_t count)
{
    if (kupl_unlikely(dst == nullptr || src == nullptr)) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(count > UINT32_MAX)) {
        return kupl_log_error_return(ERROR, "sdma_icopy_data can't support data length more than UINT32_MAX");
    }
    return KUPL_OK;
}

int kupl_memcpy(void *dst, const void *src, size_t count)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(kupl_memcpy_check(dst, src, count) == KUPL_ERROR)) {
        return KUPL_ERROR;
    }
    if (sdma_memcpy_func_init) {
        if (count < (size_t)kupl_config_get_value(KUPL_SDMA_MEMCPY_THRESHOLD)
            || !kupl_get_sdma_chn_by_cid(kupl_get_core_index())
            || !kupl_memory_is_pinned(dst, count) || !kupl_memory_is_pinned(const_cast<void *>(src), count)) {
            memcpy(dst, src, count);
        } else {
            kupl_sdma_req_t req;
            int cid = kupl_get_core_index();
            int device_id = cid / cores_per_sdma % KUPL_MAX_SDMA_DEVICE_SIZE;
            kupl_sdma_create_task(req.task, (uint64_t)src, 0, (uint64_t)dst, 0, 0,
                                  g_sdma_process_id[device_id], g_sdma_process_id[device_id],
                                  (uint32_t)count, 0);
            int ret = kupl_sdma_icopy_data(g_sdma_chns[cid], &req.task, 1, &req.request);
            if (ret != 0) {
                return kupl_log_error_return(ERROR, "sdma_icopy_data failed, ret = %d", ret);
            }
            ret = kupl_sdma_iwait_chn(g_sdma_chns[cid], &req.request);
            if (ret != 0) {
                return kupl_log_error_return(ERROR, "sdma_wait_data failed, ret = %d", ret);
            }
        }
    } else if (count < (size_t)g_kupl_memcpy_mt_threshold) {
        memcpy(dst, src, count);
    } else {
        kupl_memcpy_task_package_t package {
            .dst = dst,
            .src = src,
            .count = count
        };
        int threads = g_kupl_memcpy_threads_local > 0 ? g_kupl_memcpy_threads_local : g_kupl_memcpy_threads;
        kupl_invoke_parallel(kupl_memcpy_pf_task, (&package), threads);
    }
    return KUPL_OK;
}

int kupl_submit_memcpy_task(void *dst, const void *src, size_t count, kupl_queue_h queue)
{
    if (queue == nullptr) {
        return KUPL_ERROR;
    }
    kupl_warn("kupl_memcpy_async failed, fallback to kupl_memcpy");
    int geid = kupl_get_executor_num();
    kupl_memcpy_task_package_t *package =
    (kupl_memcpy_task_package_t *)kupl_memory_calloc(sizeof(kupl_memcpy_task_package_t), geid);
    if (package == nullptr) {
        return KUPL_ERROR;
    }
    package->count = count;
    package->dst = dst;
    package->src = src;
    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
        .func = kupl_memcpy_task,
        .args = package,
        .name = "kupl_memcpy"
    };
    kupl_queue_submit(queue, &desc);
    return KUPL_OK;
}

int kupl_submit_memcpy_async_task(kupl_sdma_req_t *req, kupl_queue_h queue, kupl_event_h event)
{
    uint8_t priority = queue->priority < 0 ? 0 : (uint8_t)queue->priority * 4;
    uint32_t field_mask = queue->priority < 0 ? KUPL_TB_DESC_FIELD_NAME :
                          KUPL_TB_DESC_FIELD_NAME | KUPL_TB_DESC_FIELD_PRIORITY;
    req->chn = queue->sdma_chn;
    kupl_tb_desc_t desc = {
        .field_mask = field_mask,
        .func = kupl_memcpy_sdma_task,
        .args = req,
        .name = "sdma_memcpy",
        .priority = priority,
    };
    int ret = kupl_event_init(event, queue, &desc, KUPL_EVENT_TYPE_RECORD);
    if (ret != KUPL_OK) {
        return KUPL_ERROR;
    }
    queue->event_count++;
    kupl_event_ref(event);
    kupl_event_submit(event, queue->last_event);
    queue->last_event = event;
    return KUPL_OK;
}

int kupl_memcpy_async_with_queue(uint64_t src_addr, uint32_t src_sl, uint64_t dst_addr, uint32_t dst_sl,
                                 uint32_t stride_num, uint32_t src_pasid, uint32_t dst_pasid, uint32_t length,
                                 uint8_t priority, kupl_queue_h queue, kupl_event_h event)
{
    queue->lock->lock(queue->lock);
    if (queue->event_count == 0) {
        int ret = kupl_set_sdma_event_req(event);
        if (kupl_unlikely(ret == KUPL_ERROR)) {
            goto error;
        }
        kupl_sdma_req_t req;
        kupl_sdma_create_task(req.task, src_addr, src_sl, dst_addr, dst_sl, stride_num,
                              src_pasid, dst_pasid, length, priority);
        ret = kupl_sdma_icopy_data(queue->sdma_chn, &req.task, 1, &event->req->request);
        if (kupl_unlikely(ret != 0)) {
            kupl_event_set_status(event, KUPL_EVENT_STATUS_COMPLETE);
            queue->lock->unlock(queue->lock);
            return kupl_log_error_return(ERROR, "sdma_icopy_data failed, ret = %d", ret);
        }
        event->req->flag = true;
        event->req->event = event;
        (*(queue->req_set)).push_back(event->req);
        (*(event->q_set)).push_back(queue);
        event->q = queue;
    } else {
        int geid = kupl_get_executor_num();
        int ret = kupl_set_sdma_async_event(event);
        if (kupl_unlikely(ret == KUPL_ERROR)) {
            goto error;
        }
        kupl_sdma_req_t *req = (kupl_sdma_req_t *)kupl_memory_calloc(sizeof(kupl_sdma_req_t), geid);
        if (kupl_unlikely(req == nullptr)) {
            goto error;
        }
        kupl_sdma_create_task(req->task, src_addr, src_sl, dst_addr, dst_sl, stride_num,
                              src_pasid, dst_pasid, length, priority);
        if (kupl_unlikely(kupl_submit_memcpy_async_task(req, queue, event) == KUPL_ERROR)) {
            kupl_memory_free(req, geid);
            goto error;
        }
    }
    queue->lock->unlock(queue->lock);
    return KUPL_OK;
error:
    queue->lock->unlock(queue->lock);
    return KUPL_ERROR;
}

int kupl_memcpy_async(void *dst, const void *src, size_t count, kupl_queue_h queue, kupl_event_h event)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(kupl_memcpy_check(dst, src, count) == KUPL_ERROR || event == nullptr)) {
        return KUPL_ERROR;
    }
    if (count != 0 && sdma_memcpy_func_init) {
        int cid = kupl_get_core_index();
        int device_id = cid / cores_per_sdma % KUPL_MAX_SDMA_DEVICE_SIZE;
        if (queue == nullptr) {
            int ret = kupl_set_sdma_event(event, cid);
            if (kupl_unlikely(!kupl_get_sdma_chn_by_cid(kupl_get_core_index()) || ret == KUPL_ERROR)) {
                return KUPL_ERROR;
            }
            kupl_sdma_req_t req;
            kupl_sdma_create_task(req.task, (uint64_t)src, 0, (uint64_t)dst, 0, 0,
                                  g_sdma_process_id[device_id], g_sdma_process_id[device_id], (uint32_t)count, 0);
            ret = kupl_sdma_icopy_data(g_sdma_chns[cid], &req.task, 1,
                                       &((kupl_sdma_async_t *)(event->m_args))->request);
            if (kupl_unlikely(ret != 0)) {
                kupl_event_set_status(event, KUPL_EVENT_STATUS_COMPLETE);
                return kupl_log_error_return(ERROR, "sdma_icopy_data failed, ret = %d", ret);
            }
            return KUPL_OK;
        } else {
            uint8_t priority = queue->priority < 0 ? 0 : (uint8_t)queue->priority * 4;
            int ret = kupl_memcpy_async_with_queue((uint64_t)src, 0, (uint64_t)dst, 0, 0,
                                                   g_sdma_process_id[device_id], g_sdma_process_id[device_id],
                                                   (uint32_t)count, priority, queue, event);
            if (kupl_unlikely(ret == KUPL_ERROR)) {
                return KUPL_ERROR;
            }
        }
    } else {
        if (kupl_submit_memcpy_task(dst, src, count, queue) == KUPL_ERROR) {
            return KUPL_ERROR;
        }
        kupl_event_record(event, queue);
    }
    return KUPL_OK;
}

typedef struct kupl_memcpy2d_task_package {
    void *dst;
    size_t dpitch;
    const void *src;
    size_t spitch;
    size_t width;
    size_t height;
} kupl_memcpy2d_task_package_t;

static void kupl_memcpy2d_pf_task(void *args, int tid, int tnum)
{
    kupl_memcpy2d_task_package_t* package = (kupl_memcpy2d_task_package_t*)args;
    void *dst = package->dst;
    size_t dpitch = package->dpitch;
    const void *src = package->src;
    size_t spitch = package->spitch;
    size_t width = package->width;
    size_t height = package->height;

    size_t total_bytes = width * height;
    size_t bytes_per_thread = total_bytes / (size_t)tnum;
    size_t remainder = total_bytes % (size_t)tnum;

    // calculate the start and end bytes for the current thread
    size_t start_byte = (size_t)tid * bytes_per_thread + kupl_min((size_t)tid, remainder);
    size_t end_byte = start_byte + bytes_per_thread + ((size_t)tid < remainder ? 1 : 0);

    // calculate the start and end rows and columns with 2d memory
    size_t start_row = start_byte / width;
    size_t start_col = start_byte % width;
    size_t end_row = end_byte / width;
    size_t end_col = end_byte % width;

    // adjust the start address to align with the cache line
    size_t start_addr = ((size_t)(src) + start_row * spitch + start_col) /
                        KUPL_CACHE_LINE * KUPL_CACHE_LINE;
    // if start_col less than left boundary
    if (start_addr < (uintptr_t)src + start_row * spitch) {
        start_byte = start_row * width;
    } else {
        start_byte = start_addr - (uintptr_t)src - start_row * spitch + start_row * width;
    }
    // adjust the end address to align with the cache line
    // only if end_col not equal right boundary
    if (end_col != width - 1) {
        size_t end_addr = ((size_t)(src) + end_row * spitch + end_col) /
                          KUPL_CACHE_LINE * KUPL_CACHE_LINE;
        // if end_col less than left boundary
        if (end_addr < (uintptr_t)src + end_row * spitch) {
            end_byte = end_row * width;
        } else {
            end_byte = end_addr - (uintptr_t)src - end_row * spitch + end_row * width;
        }
    }

    // recalculate the start and end rows and columns
    start_row = start_byte / width;
    start_col = start_byte % width;
    end_row = end_byte / width;
    end_col = end_byte % width;

    // copy the data from the source to the destination
    for (size_t row = start_row; row <= end_row; row++) {
        size_t col_start = (row == start_row) ? start_col : 0;
        size_t col_end = (row == end_row) ? end_col : width;
        if (col_end > col_start) {
            memcpy((char *)dst + row * dpitch + col_start,
                   (const char *)src + row * spitch + col_start,
                   col_end - col_start);
        }
    }
}

static void kupl_memcpy2d_task(void *args)
{
    kupl_memcpy2d_task_package_t* package = (kupl_memcpy2d_task_package_t*)args;
    void *dst = package->dst;
    size_t dpitch = package->dpitch;
    const void *src = package->src;
    size_t spitch = package->spitch;
    size_t width = package->width;
    size_t height = package->height;
    kupl_memcpy2d(dst, dpitch, src, spitch, width, height);
    int geid = kupl_get_executor_num();
    kupl_memory_free(package, geid);
}

void kupl_glibc_memcpy2d(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height)
{
    for (size_t i = 0; i < height; i++) {
        memcpy(((char*)dst + i * dpitch), ((const char *)src + i * spitch), width);
    }
}

int kupl_memcpy2d_check(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height)
{
    if (kupl_unlikely(dst == nullptr || src == nullptr || spitch < width || dpitch < width
        || (height != 0 && (SIZE_MAX / height < spitch || SIZE_MAX / height < dpitch)))) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(spitch - width > UINT32_MAX || dpitch - width > UINT32_MAX
        || width > UINT32_MAX || height > UINT32_MAX)) {
        return kupl_log_error_return(ERROR, "sdma_icopy_data can't support data length more than UINT32_MAX");
    }
    return KUPL_OK;
}

int kupl_memcpy2d(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(kupl_memcpy2d_check(dst, dpitch, src, spitch, width, height) == KUPL_ERROR)) {
        return KUPL_ERROR;
    }
    if (sdma_memcpy_func_init) {
        if (width * height < (size_t)kupl_config_get_value(KUPL_SDMA_MEMCPY_THRESHOLD) ||
            !kupl_get_sdma_chn_by_cid(kupl_get_core_index()) ||
            !kupl_memory_is_pinned(dst, (height - 1) * dpitch + width) ||
            !kupl_memory_is_pinned(const_cast<void *>(src), (height - 1) * spitch + width)) {
            kupl_glibc_memcpy2d(dst, dpitch, src, spitch, width, height);
        } else {
            kupl_sdma_req_t req;
            int cid = kupl_get_core_index();
            int device_id = cid / cores_per_sdma % KUPL_MAX_SDMA_DEVICE_SIZE;
            kupl_sdma_create_task(req.task, (uint64_t)src, (uint32_t)(spitch - width), (uint64_t)dst,
                                  (uint32_t)(dpitch - width), (uint32_t)height, g_sdma_process_id[device_id],
                                  g_sdma_process_id[device_id], (uint32_t)width, 0);
            int ret = kupl_sdma_icopy_data(g_sdma_chns[cid], &req.task, 1, &req.request);
            if (ret != 0) {
                return kupl_log_error_return(ERROR, "sdma_icopy_data failed, ret = %d", ret);
            }
            ret = kupl_sdma_iwait_chn(g_sdma_chns[cid], &req.request);
            if (ret != 0) {
                return kupl_log_error_return(ERROR, "sdma_wait_data failed, ret = %d", ret);
            }
        }
    } else if (width * height < (size_t)g_kupl_memcpy_mt_threshold) {
        kupl_glibc_memcpy2d(dst, dpitch, src, spitch, width, height);
    } else {
        kupl_memcpy2d_task_package_t package {
            .dst = dst,
            .dpitch = dpitch,
            .src = src,
            .spitch = spitch,
            .width = width,
            .height = height
        };
        int threads = g_kupl_memcpy_threads_local > 0 ? g_kupl_memcpy_threads_local : g_kupl_memcpy_threads;
        kupl_invoke_parallel(kupl_memcpy2d_pf_task, (&package), threads);
    }
    return KUPL_OK;
}

int kupl_submit_memcpy2d_task(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width,
                              size_t height, kupl_queue_h queue)
{
    if (queue == nullptr) {
        return KUPL_ERROR;
    }
    kupl_warn("kupl_memcpy2d_async failed, fallback to kupl_memcpy2d");
    int geid = kupl_get_executor_num();
    kupl_memcpy2d_task_package_t *package =
    (kupl_memcpy2d_task_package_t *)kupl_memory_calloc(sizeof(kupl_memcpy2d_task_package_t), geid);
    if (package == nullptr) {
        return KUPL_ERROR;
    }
    package->dst = dst;
    package->dpitch = dpitch;
    package->src = src;
    package->spitch = spitch;
    package->width = width;
    package->height = height;
    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
        .func = kupl_memcpy2d_task,
        .args = package,
        .name = "kupl_memcpy2d"
    };
    kupl_queue_submit(queue, &desc);
    return KUPL_OK;
}

int kupl_memcpy2d_async(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height,
                        kupl_queue_h queue, kupl_event_h event)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(kupl_memcpy2d_check(dst, dpitch, src, spitch, width, height) == KUPL_ERROR
        || event == nullptr)) {
        return KUPL_ERROR;
    }
    if (width * height != 0 && sdma_memcpy_func_init) {
        int cid = kupl_get_core_index();
        int device_id = cid / cores_per_sdma % KUPL_MAX_SDMA_DEVICE_SIZE;
        if (queue == nullptr) {
            int ret = kupl_set_sdma_event(event, cid);
            if (kupl_unlikely(!kupl_get_sdma_chn_by_cid(kupl_get_core_index()) || ret == KUPL_ERROR)) {
                return KUPL_ERROR;
            }
            kupl_sdma_req_t req;
            kupl_sdma_create_task(req.task, (uint64_t)src, (uint32_t)(spitch - width), (uint64_t)dst,
                                  (uint32_t)(dpitch - width), (uint32_t)height, g_sdma_process_id[device_id],
                                  g_sdma_process_id[device_id], (uint32_t)width, 0);
            ret = kupl_sdma_icopy_data(g_sdma_chns[cid], &req.task, 1,
                                       &((kupl_sdma_async_t *)(event->m_args))->request);
            if (kupl_unlikely(ret != 0)) {
                kupl_event_set_status(event, KUPL_EVENT_STATUS_COMPLETE);
                return kupl_log_error_return(ERROR, "sdma_icopy_data failed, ret = %d", ret);
            }
            return KUPL_OK;
        } else {
            uint8_t priority = queue->priority < 0 ? 0 : (uint8_t)queue->priority * 4;
            int ret = kupl_memcpy_async_with_queue((uint64_t)src, (uint32_t)(spitch - width), (uint64_t)dst,
                (uint32_t)(dpitch - width), (uint32_t)height, g_sdma_process_id[device_id],
                g_sdma_process_id[device_id], (uint32_t)width, priority, queue, event);
            if (kupl_unlikely(ret == KUPL_ERROR)) {
                return KUPL_ERROR;
            }
        }
    } else {
        if (kupl_submit_memcpy2d_task(dst, dpitch, src, spitch, width, height, queue) == KUPL_ERROR) {
            return KUPL_ERROR;
        }
        kupl_event_record(event, queue);
    }
    return KUPL_OK;
}

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

#include "kupl_event.h"
#include "kupl_graph.h"
#include "kupl_queue.h"
#include "core/kupl_core.h"
#include "executor/kupl_executor.h"
#include "memory/mpool/kupl_mpool.h"
#include "dm/memcpy/kupl_memcpy.h"
#include "utils/sys/kupl_compiler.h"

kupl_event_h kupl_event_create_with_udata(size_t udata_size)
{
    int geid = kupl_get_executor_num();
    kupl_event_t *event = (kupl_event_t *)kupl_memory_calloc(sizeof(kupl_event_t), geid);
    if (kupl_unlikely(event == nullptr)) {
        return nullptr;
    }
    kupl_event_ref(event);
    // task ref == 1, deref at @event_func
    event->lock = kupl_lock_create(PTHREAD_SPINLOCK);
    if (kupl_unlikely(event->lock == nullptr)) {
        goto err;
    }
    event->task = kupl_task_create_with_udata(udata_size);
    if (kupl_unlikely(event->task == nullptr)) {
        goto err;
    }
    event->req = (kupl_sdma_request_h)kupl_memory_calloc(sizeof(kupl_sdma_request_t), geid);
    if (event->req == nullptr) {
        goto err;
    }
    event->q_set = new (std::nothrow) std::vector<kupl_queue_h>();
    if (event->q_set == nullptr) {
        goto err;
    }
    (event->q_set)->reserve(KUPL_RESERVE_SIZE);
    kupl_event_set_status(event, KUPL_EVENT_STATUS_CREATED);
    return event;
err:
    kupl_event_destroy(event);
    return nullptr;
}

kupl_event_h kupl_event_create(void)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return nullptr;
    }
    return kupl_event_create_with_udata(0);
}

void kupl_remove_event_in_queue(kupl_event_h event)
{
    event->lock->lock(event->lock);
    for (auto it = (*(event->q_set)).begin(); it != (*(event->q_set)).end();) {
        kupl_queue_h q = *it;
        q->lock->lock(q->lock);
        q->req_set->erase(remove(q->req_set->begin(), q->req_set->end(), event->req), q->req_set->end());
        q->lock->unlock(q->lock);
        it = (*(event->q_set)).erase(it);
    }
    event->lock->unlock(event->lock);
}

void kupl_event_destroy(kupl_event_h event)
{
    if (kupl_unlikely(event == nullptr)) {
        return;
    }
    kupl_event_deref(event);
}

void kupl_event_ref(kupl_event_t *event)
{
    KUPL_ATOMIC_ADD(&event->ref, 1);
}

void kupl_event_deref(kupl_event_t *event)
{
    if (KUPL_ATOMIC_SUB(&event->ref, 1) == 1) {
        kupl_remove_event_in_queue(event);
        int geid = kupl_get_executor_num();
        kupl_memory_free(event->m_args, geid);
        delete event->q_set;
        kupl_memory_free(event->req, geid);
        kupl_task_cleanup(event->task);
        kupl_lock_cleanup(event->lock);
        kupl_memory_free(event, geid);
    }
}

static void event_func(void *args)
{
    auto event = (kupl_event_t *)args;
    if (kupl_likely(event->func)) {
        event->func(event->args);
    }
    kupl_event_set_status(event, KUPL_EVENT_STATUS_COMPLETE);
    kupl_dequeue_event(event->q, event);
}

int kupl_event_init(kupl_event_t *event, kupl_queue_t *queue, kupl_tb_desc_t *desc, kupl_event_type_t type)
{
    int geid = kupl_get_executor_num();
    if (kupl_unlikely(geid == KUPL_EXECUTOR_DEFAULT)) {
        return kupl_log_error_return(ERROR, "invoke KUPL functions on threads not managed by KUPL");
    }
    // event always has task
    kupl_gnode_cleanup(event->task->gnode, geid);
    auto graph = kupl_get_global_graph();
    if (kupl_unlikely(graph == nullptr)) {
        return KUPL_ERROR;
    }
    event->func = desc->func;
    event->args = desc->args;
    desc->func = event_func;
    desc->args = event;

    kupl_task_param_t task_param = {
        .super =
            {
                .type = KUPL_TB_TYPE_TASK,
                .user_desc = desc,
                .graph = graph,
                .count = &graph->count,
            },
        .kind = KUPL_TASK_KIND_COMM_DYNAMIC,
        .inplace = event->task,
    };
    event->task = kupl_task_init(&task_param, geid);
    if (kupl_unlikely(event->task == nullptr)) {
        return KUPL_ERROR;
    }
    event->graph = graph;
    event->type = type;
    event->q = queue;
    return KUPL_OK;
}

static void event_func_record(void *args)
{
    (void)args;
}

int kupl_event_record(kupl_event_t *event, kupl_queue_t *queue)
{
    if (kupl_unlikely(event == nullptr || queue == nullptr)) {
        return KUPL_ERROR;
    }
    int status = kupl_event_query(event);
    if (status == KUPL_EVENT_STATUS_SUBMITTED) {
        return KUPL_ERROR;
    }

    kupl_tb_desc_t desc = {
        .func = event_func_record,
        .args = event,
    };
    int ret = kupl_event_init(event, queue, &desc, KUPL_EVENT_TYPE_RECORD);
    if (ret != KUPL_OK) {
        return KUPL_ERROR;
    }
    kupl_enqueue_event(queue, event);
    return KUPL_OK;
}

static void event_func_wait(void *args)
{
    (void)args;
}

int kupl_event_init_wait(kupl_event_t *event, kupl_queue_t *queue, kupl_event_t *wait_event)
{
    kupl_tb_desc_t desc = {
        .func = event_func_wait,
        .args = event,
    };
    event->wait = wait_event;
    return kupl_event_init(event, queue, &desc, KUPL_EVENT_TYPE_WAIT);
}

int kupl_event_wait(kupl_event_t *event)
{
    if (kupl_unlikely(event == nullptr || KUPL_ATOMIC_LD(&event->status) == KUPL_EVENT_STATUS_CREATED)) {
        return KUPL_ERROR;
    }
    if (event->type == KUPL_EVENT_TYPE_SDMA) {
        int ret = kupl_sdma_wait_event(event);
        kupl_event_set_status(event, KUPL_EVENT_STATUS_COMPLETE);
        return ret;
    }
    int ret = kupl_task_wait(event->task);
    int status = kupl_event_query(event);
    if (kupl_unlikely(status != KUPL_EVENT_STATUS_COMPLETE)) {
        kupl_warn("event not complete, event status: %d", status);
        ret = KUPL_ERROR;
    }
    return ret;
}

int kupl_event_query(kupl_event_h event)
{
    if (kupl_unlikely(event == nullptr)) {
        return KUPL_ERROR;
    }
    if (event->type == KUPL_EVENT_TYPE_SDMA) {
        int ret = kupl_sdma_query_event(event);
        if (KUPL_ATOMIC_LD(&event->status) != KUPL_EVENT_STATUS_COMPLETE && ret == KUPL_EVENT_STATUS_COMPLETE) {
            kupl_event_set_status(event, KUPL_EVENT_STATUS_COMPLETE);
        }
        return ret;
    }
    return KUPL_ATOMIC_LD(&event->status);
}

void kupl_event_set_status(kupl_event_t *event, kupl_event_status_t status)
{
    KUPL_ATOMIC_ST(&event->status, status);
}

void kupl_event_submit(kupl_event_t *event, kupl_event_t *last_event)
{
    uint32_t prev_tasks = 0;
    int geid = kupl_get_executor_num();
    kupl_event_set_status(event, KUPL_EVENT_STATUS_SUBMITTED);

    if (last_event != nullptr) {
        prev_tasks += kupl_gnode_precede(&last_event->task->gnode, &event->task->gnode, geid);
    }
    switch (event->type) {
        case KUPL_EVENT_TYPE_RECORD:
            break;
        case KUPL_EVENT_TYPE_WAIT:
            prev_tasks += kupl_gnode_precede(&event->wait->task->gnode, &event->task->gnode, geid);
            break;
        case KUPL_EVENT_TYPE_KERNEL:
            break;
        case KUPL_EVENT_TYPE_SDMA_WAIT:
            break;
        default:
            break;
    }

    if (prev_tasks == 0) {
        kupl_sched_add_tb(event->graph->sched, &event->task->tb);
    }
}
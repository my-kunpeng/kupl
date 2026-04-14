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
#include "kupl_sched_static_mq.h"
#include "mt/scheduler/plugin/kupl_sched_plugin_api.h"
#include "executor/backend/kupl_executor_backend.h"
#include "executor/kupl_executor.h"
#include "utils/lock/kupl_lock.h"
#include "tools/struct/kupl_vector.h"

using queue_t = struct {
    kupl_vector_t    *q;
    kupl_lock_t      *add_lock;
    kupl_lock_t      *get_lock;
};

static kupl_always_inline
void queue_destroy(queue_t *queue);

static kupl_always_inline
queue_t *queue_create(int size)
{
    queue_t *queue = (queue_t *)kupl_calloc(1, sizeof(queue_t));
    if (queue == nullptr) {
        return nullptr;
    }
    queue->q = kupl_vector_create((size_t)size, sizeof(kupl_taskbase_t *), "sched static_mq queue");
    queue->add_lock = kupl_lock_create(PTHREAD_SPINLOCK);
    queue->get_lock = kupl_lock_create(PTHREAD_SPINLOCK);
    if (kupl_unlikely(queue->q == nullptr || queue->add_lock == nullptr || queue->get_lock == nullptr)) {
        goto err;
    }
    return queue;
err:
    queue_destroy(queue);
    return nullptr;
}

static kupl_always_inline
void queue_destroy(queue_t *queue)
{
    if (queue == nullptr) {
        return;
    }
    if (queue->get_lock != nullptr) {
        kupl_lock_cleanup(queue->get_lock);
    }
    if (queue->add_lock != nullptr) {
        kupl_lock_cleanup(queue->add_lock);
    }
    if (queue->q != nullptr) {
        kupl_vector_cleanup(queue->q);
    }
    kupl_safe_free(queue);
}

static kupl_always_inline
int queue_add_tb(queue_t *queue, kupl_taskbase_t *tb)
{
    int err = KUPL_ERROR;
    if (kupl_unlikely(queue == nullptr)) {
        kupl_warn("queue not exist");
        return err;
    }
    auto lock = queue->add_lock;
    lock->lock(lock);
    err = kupl_vector_push_back_macro(queue->q, tb);
    lock->unlock(lock);
    return err;
}

static kupl_always_inline
kupl_taskbase_t *queue_get_tb(queue_t *queue)
{
    kupl_taskbase_t *tb = nullptr;
    if (kupl_unlikely(queue == nullptr)) {
        kupl_warn("queue not exist");
        return tb;
    }
    auto lock = queue->get_lock;
    lock->lock(lock);
    if (!kupl_vector_empty_macro(queue->q)) {
        kupl_vector_front_macro(queue->q, tb);
        kupl_vector_pop_front_macro(queue->q);
    }
    lock->unlock(lock);
    return tb;
}

typedef struct kupl_sched_static_mq {
    int                 num_executors;      // num executors
    int                 queue_size;         // queue size
    priority_queue_t    *priority_queues;
    queue_t             **queues;           // local_queues own by executors
} kupl_sched_static_mq_t;

static int enable_priority;

static int kupl_sched_static_mq_init(kupl_sched_plugin_property_t *property)
{
    enable_priority = kupl_config_get_value(KUPL_ENABLE_PRIORITY);
    property->name = KUPL_SCHED_PLUGIN_STATIC_MQ_NAME;
    property->private_data_len = 0;
    property->score = KUPL_SCHED_PLUGIN_STATIC_MQ_SCORE;

    return KUPL_OK;
}

static void kupl_sched_static_mq_fini()
{
}

static void kupl_sched_static_mq_cleanup(void *_sched);
static void* kupl_sched_static_mq_create()
{
    auto host_info = kupl_get_host_info();
    int num_executors = host_info->avail_pu_cnt;

    kupl_sched_static_mq_t *sched = (kupl_sched_static_mq_t *)kupl_calloc(1, sizeof(kupl_sched_static_mq_t));
    if (kupl_unlikely(sched == nullptr)) {
        return nullptr;
    }
    sched->num_executors = num_executors;
    sched->queue_size = kupl_config_get_value(KUPL_SCHED_QUEUE_LENGTH);
    sched->queues = (queue_t **)kupl_calloc((size_t)num_executors, sizeof(queue_t *));
    sched->priority_queues = priority_queue_create(sched->queue_size);
    if (kupl_unlikely(sched->queues == nullptr || sched->priority_queues == nullptr)) {
        goto err;
    }
    for (int i = 0; i < num_executors; ++i) {
        sched->queues[i] = queue_create(sched->queue_size);
        if (kupl_unlikely(sched->queues[i] == nullptr)) {
            goto err;
        }
    }
    return sched;
err:
    kupl_warn("kupl_sched_static_mq_create failed");
    kupl_sched_static_mq_cleanup(sched);
    return nullptr;
}

static void kupl_sched_static_mq_cleanup(void *_sched)
{
    kupl_sched_static_mq_t *sched = (kupl_sched_static_mq_t *)_sched;
    if (kupl_unlikely(sched == nullptr)) {
        return;
    }
    for (int i = 0; i < sched->num_executors; ++i) {
        queue_destroy(sched->queues[i]);
    }
    priority_queue_cleanup(sched->priority_queues);
    kupl_safe_free(sched->queues);
    kupl_safe_free(sched);
    return;
}

static int kupl_sched_static_mq_add_tb(void *_sched, kupl_taskbase_t *tb)
{
    kupl_sched_static_mq_t *sched = (kupl_sched_static_mq_t *)_sched;
    int geid = kupl_get_executor_num();
    if (kupl_unlikely(geid == KUPL_EIDCID_INIT)) {
        return kupl_log_error_return(ERROR, "invoke KUPL functions on threads not managed by KUPL");
    }
    auto eid = tb->executor_id;
    if (eid <= KUPL_TB_EXECUTOR_ID_DEFAULT) {
        // if eid not set, add to own
        eid = geid;
    }
    // priority_queue
    if (enable_priority && (tb->flag & KUPL_TB_FLAG_PRIORITY)) {
        return priority_queue_add_tb(sched->priority_queues, tb);
    }

    return queue_add_tb(sched->queues[eid], tb);
}

static kupl_taskbase_t *kupl_sched_static_mq_get_tb(void *_sched, kupl_compute_place_t cp)
{
    kupl_sched_static_mq_t *sched = (kupl_sched_static_mq_t *)_sched;
    (void)cp;
    int eid = kupl_get_executor_num();
    if (kupl_unlikely(eid == KUPL_EIDCID_INIT)) {
        kupl_error("invoke KUPL functions on threads not managed by KUPL");
        return nullptr;
    }

    kupl_taskbase_t *tb = nullptr;
    if (enable_priority) {
        tb = priority_queue_get_tb(sched->priority_queues);
        if (tb != nullptr) {
            return tb;
        }
    }

    return queue_get_tb(sched->queues[eid]);
}

static const kupl_sched_plugin_api_t KUPL_SCHED_PLUGIN_GLOBAL_VAR(static_mq) = {
    .name = KUPL_SCHED_PLUGIN_STATIC_MQ_NAME,
    .init = kupl_sched_static_mq_init,
    .fini = kupl_sched_static_mq_fini,
    .create = kupl_sched_static_mq_create,
    .expand = nullptr,
    .cleanup = kupl_sched_static_mq_cleanup,
    .add_tb = kupl_sched_static_mq_add_tb,
    .get_tb = kupl_sched_static_mq_get_tb,
};

const kupl_sched_plugin_api_t* kupl_sched_static_mq_get_instance()
{
    return &KUPL_SCHED_PLUGIN_GLOBAL_VAR(static_mq);
}
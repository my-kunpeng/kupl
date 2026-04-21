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

#include "kupl_queue.h"
#include <map>
#include <vector>
#include "core/kupl_core.h"
#include "memory/mpool/kupl_mpool.h"
#include "dm/memcpy/kupl_memcpy.h"
#include "executor/kupl_executor_group.h"
#include "mt/kupl_event.h"
#include "mt/kupl_graph.h"
#include "mt/kupl_check.h"
#include "executor/kupl_executor_group.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/debug/kupl_log.h"

static std::map<int, kupl_queue_h> *g_queue_table = nullptr;
static std::mutex *g_queue_table_mutex = nullptr;
static std::unordered_map<uint64_t, kupl_egroup_h> *g_egroup_table = nullptr;
static std::mutex *g_egroup_table_mutex = nullptr;
constexpr auto BITS_IN_UINT32 = sizeof(uint32_t) * CHAR_BIT;

int kupl_get_queue_priority_range(int *least_priority, int *greatest_priority)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(least_priority == nullptr || greatest_priority == nullptr)) {
        return KUPL_ERROR;
    }
    *least_priority = KUPL_LEAST_PRIORITY;
    *greatest_priority = KUPL_GREATEST_PRIORITY;
    return KUPL_OK;
}

kupl_queue_t *kupl_queue_create_with_priority(int priority)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return nullptr;
    }
    if (kupl_unlikely(priority < KUPL_LEAST_PRIORITY || priority > KUPL_GREATEST_PRIORITY)) {
        kupl_warn("priority out of range");
        return nullptr;
    }
    kupl_queue_t *queue = kupl_queue_create();
    if (kupl_unlikely(queue == nullptr)) {
        kupl_warn("kupl out of memory");
        return nullptr;
    }

    queue->priority = priority;
    return queue;
}

kupl_queue_t *kupl_queue_create(void)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return nullptr;
    }
    kupl_queue_t *queue = (kupl_queue_t *)kupl_calloc(1, sizeof(kupl_queue_t));
    if (kupl_unlikely(queue == nullptr)) {
        kupl_warn("kupl out of memory");
        return nullptr;
    }
    queue->last_event = nullptr;
    queue->lock = kupl_lock_create(PTHREAD_SPINLOCK);
    if (kupl_unlikely(queue->lock == nullptr)) {
        kupl_free_inner(queue);
        return nullptr;
    }
    queue->sdma_chn = kupl_get_sdma_chn();
    if (kupl_get_sdma_memcpy_func_init() && queue->sdma_chn == nullptr) {
        kupl_lock_cleanup(queue->lock);
        kupl_free_inner(queue);
        return nullptr;
    }
    queue->req_set = new (std::nothrow) std::vector<kupl_sdma_request_h>();
    if (queue->req_set == nullptr) {
        kupl_lock_cleanup(queue->lock);
        kupl_free_inner(queue);
        return nullptr;
    }
    (queue->req_set)->reserve(KUPL_RESERVE_SIZE);
    queue->priority = -1;
    queue->acquire = false;
    queue->sync = false;
    return queue;
}

void kupl_queue_destroy(kupl_queue_t *queue)
{
    if (kupl_unlikely(queue == nullptr)) {
        return;
    }
    if (queue->acquire) {
        (*g_queue_table).erase(queue->index);
    }
    kupl_queue_wait(queue);
    kupl_lock_cleanup(queue->lock);
    delete queue->req_set;
    kupl_free_inner(queue);
}

void kupl_wait_sdma_request_in_queue(void *args)
{
    kupl_queue_h queue = (kupl_queue_h)args;
    for (auto it = (*(queue->req_set)).begin(); it != (*(queue->req_set)).end();) {
        kupl_sdma_request_h req = *it;
        req->event->lock->lock(req->event->lock);
        if (req->flag) {
            req->flag =false;
            sdma_request_t* request = &req->request;
            kupl_sdma_wait_req(queue, request);
            kupl_event_set_status(req->event, KUPL_EVENT_STATUS_COMPLETE);
        }
        req->event->q_set->erase(remove(req->event->q_set->begin(), req->event->q_set->end(), queue),
            req->event->q_set->end());

        req->event->lock->unlock(req->event->lock);
        it = (*(queue->req_set)).erase(it);
    }
}

int kupl_queue_wait(kupl_queue_t *queue)
{
    if (kupl_unlikely(queue == nullptr)) {
        return KUPL_ERROR;
    }
    int ret = KUPL_ERROR;
    kupl_task_h wait_task = nullptr;

    queue->lock->lock(queue->lock);
    if (queue->event_count != 0) {
        if (queue->last_event != nullptr) {
            wait_task = queue->last_event->task;
            kupl_task_ref(&wait_task->tb);
        }
    } else {
        kupl_wait_sdma_request_in_queue(queue);
    }
    queue->lock->unlock(queue->lock);

    if (wait_task != nullptr) {
        ret = kupl_task_wait(wait_task);
        kupl_task_cleanup(wait_task);
        return ret;
    }
    ret = KUPL_OK;
    return ret;
}

void kupl_enqueue_event(kupl_queue_t *queue, kupl_event_t *event)
{
    if (kupl_unlikely(queue == nullptr)) {
        return;
    }
    queue->lock->lock(queue->lock);
    queue->event_count++;
    if (event->type != KUPL_EVENT_TYPE_KERNEL) {
        kupl_event_ref(event);
    }
    kupl_event_submit(event, queue->last_event);
    queue->last_event = event;
    queue->lock->unlock(queue->lock);
}

void kupl_dequeue_event(kupl_queue_t *queue, kupl_event_t *event)
{
    if (kupl_unlikely(queue == nullptr)) {
        return;
    }
    queue->lock->lock(queue->lock);
    queue->event_count--;
    kupl_event_set_status(event, KUPL_EVENT_STATUS_COMPLETE);
    kupl_event_deref(event);
    if (queue->last_event == event) {
        queue->last_event = nullptr;
    }
    queue->lock->unlock(queue->lock);
}

int kupl_queue_wait_event(kupl_queue_h queue, kupl_event_h wait_event)
{
    if (kupl_unlikely(queue == nullptr || wait_event == nullptr ||
                      kupl_event_query(wait_event) == KUPL_EVENT_STATUS_CREATED)) {
        return kupl_log_error_return(WARN, "queue_wait_event invalid params");
    }
    kupl_event_t *event = kupl_event_create();
    if (event == nullptr) {
        return kupl_log_error_return(WARN, "queue_wait_event event create failed");
    }
    int ret = kupl_event_init_wait(event, queue, wait_event);
    if (kupl_unlikely(ret != KUPL_OK)) {
        kupl_error("queue_wait_event event init wait failed");
        goto out;
    }

    kupl_enqueue_event(queue, event);
    ret = kupl_task_wait(event->task);
    if (kupl_unlikely(ret != KUPL_OK)) {
        kupl_error("queue_wait_event task wait failed");
    }
out:
    kupl_event_destroy(event);
    return ret;
}

int kupl_queue_submit_request(kupl_queue_t *queue)
{
    if ((*(queue->req_set)).empty()) {
        return KUPL_OK;
    }
    kupl_event_t *event = kupl_event_create();
    if (kupl_unlikely(event == nullptr)) {
        return KUPL_ERROR;
    }
    uint64_t field_mask = queue->priority < 0 ? KUPL_TB_DESC_FIELD_NAME : KUPL_TB_DESC_FIELD_PRIORITY;
    int priority = queue->priority < 0 ? 0 : queue->priority;
    kupl_tb_desc_t tb_desc = {
        .field_mask = field_mask,
        .func = kupl_wait_sdma_request_in_queue,
        .args = queue,
        .name = "queue_request_wait",
        .priority = priority,
    };

    int ret = kupl_event_init(event, queue, &tb_desc, KUPL_EVENT_TYPE_KERNEL);
    if (kupl_likely(ret == KUPL_OK)) {
        queue->event_count++;
        kupl_event_ref(event);
        kupl_event_submit(event, queue->last_event); // event will destroy itself
        queue->last_event = event;
    } else {
        kupl_event_destroy(event);
    }
    return ret;
}

int kupl_queue_submit(kupl_queue_t *queue, kupl_queue_item_desc_t *desc)
{
    if (kupl_unlikely(queue == nullptr || desc == nullptr ||
        desc->func == nullptr)) {
        return kupl_log_error_return(WARN, "queue submit with invalid params");
    }
    if ((desc->field_mask & KUPL_QUEUE_ITEM_DESC_FIELD_NAME) && desc->name == nullptr) {
        return kupl_log_error_return(WARN, "queue submit with invalid name");
    }
    if (queue->sync) {
        desc->func(desc->args);
        return KUPL_OK;
    }

    queue->lock->lock(queue->lock);
    if (queue->event_count == 0) {
        if (kupl_unlikely(KUPL_OK != kupl_queue_submit_request(queue))) {
            queue->lock->unlock(queue->lock);
            return KUPL_ERROR;
        }
    }
    queue->lock->unlock(queue->lock);

    // event no need to destroy
    size_t udata_size = desc->field_mask & KUPL_QUEUE_ITEM_DESC_FIELD_ARGS_SIZE ? desc->args_size : 0;
    kupl_event_t *event = kupl_event_create_with_udata(udata_size);
    if (kupl_unlikely(event == nullptr)) {
        return KUPL_ERROR;
    }
    const char *name = desc->field_mask & KUPL_QUEUE_ITEM_DESC_FIELD_NAME ? desc->name : "auto";
    uint64_t field_mask = queue->priority < 0 ? KUPL_TB_DESC_FIELD_NAME :
                          KUPL_TB_DESC_FIELD_NAME | KUPL_TB_DESC_FIELD_PRIORITY;
    void *tb_args;
    if (desc->field_mask & KUPL_QUEUE_ITEM_DESC_FIELD_ARGS_SIZE) {
        memcpy(event->task->udata, desc->args, desc->args_size);
        tb_args = event->task->udata;
    } else {
        tb_args = desc->args;
    }

    kupl_tb_desc_t tb_desc = {
        .field_mask = field_mask,
        .func = desc->func,
        .args = tb_args,
        .name = name,
        .priority = queue->priority,
    };
    if ((desc->field_mask & KUPL_QUEUE_ITEM_DESC_FIELD_EGROUP) && (desc->egroup != nullptr)) {
        tb_desc.egroup = desc->egroup;
        tb_desc.executor_id = (int)kupl_egroup_master_eid(desc->egroup);
        tb_desc.field_mask |= KUPL_TB_DESC_FIELD_EGROUP;
        tb_desc.field_mask |= KUPL_TB_DESC_FIELD_EXECUTOR_ID;
    }

    if (queue->acquire) {
        tb_desc.egroup = kupl_queue_acquire_egroup(queue);
        if (kupl_unlikely(tb_desc.egroup == nullptr)) {
            kupl_event_destroy(event);
            kupl_error("kupl_queue_submit egroup set fail");
            return KUPL_ERROR;
        }
        tb_desc.executor_id = (int)kupl_egroup_master_eid(tb_desc.egroup);
        tb_desc.field_mask |= KUPL_TB_DESC_FIELD_EGROUP;
        tb_desc.field_mask |= KUPL_TB_DESC_FIELD_EXECUTOR_ID;
    }

    int ret = kupl_event_init(event, queue, &tb_desc, KUPL_EVENT_TYPE_KERNEL);
    if (kupl_likely(ret == KUPL_OK)) {
        kupl_enqueue_event(queue, event);
    } else {
        kupl_event_destroy(event);
    }
    return ret;
}

namespace kupl {
    using kernel_type = std::function<void(const kupl_nd_range_t *)>;
    struct kernel_data {
        kupl_nd_range_t                 range;
        kupl_egroup_h                   egroup;
        uint64_t                        field_mask;
        kernel_type                     func;
    };

    static void lambda_kernel(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
    {
        (void)tid;
        (void)tnum;
        auto data = (kernel_data *)args;
        data->func(nd_range);
    }

    static void lambda_kernel_wrapper(void *args)
    {
        auto data = (kernel_data *)args;
        kupl_parallel_for_desc_t desc;
        desc.field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT;
        desc.range = &data->range;
        desc.egroup = data->egroup;
        desc.concurrency = KUPL_CONCURRENCY_DEFAULT;
        desc.policy = KUPL_LOOP_POLICY_TASK;
        kupl_parallel_for(&desc, lambda_kernel, data);
    }

    static int queue_submit_check(kupl_queue_kernel_desc_t *desc)
    {
        // range
        auto range = desc->range;
        if (range == nullptr) {
            return KUPL_ERROR;
        }
        if (range->dim != 1) {
            return KUPL_ERROR;
        }
        if (kupl_check_range(range, KUPL_LOOP_POLICY_TASK) != KUPL_OK) {
            return KUPL_ERROR;
        }
        // egroup
        if (desc->egroup == nullptr) {
            return KUPL_ERROR;
        }
        if (kupl_egroup_get_cur_size(desc->egroup) <= 0) {
            return KUPL_ERROR;
        }
        // name
        if (desc->field_mask & KUPL_QUEUE_KERNEL_DESC_FIELD_NAME) {
            if (desc->name == nullptr) {
                return KUPL_ERROR;
            }
        }
        return KUPL_OK;
    }

    int queue_submit(kupl_queue_h queue, kupl_queue_kernel_desc_t *desc,
                     const std::function<void(const kupl_nd_range_t *)>& kernel)
    {
        if (kupl_unlikely(queue == nullptr || desc == nullptr || kernel == nullptr)) {
            return kupl_log_error_return(WARN, "queue submit lambda with invalid params");
        }
        if (queue_submit_check(desc) != KUPL_OK) {
            return kupl_log_error_return(WARN, "queue submit lambda with invalid desc");
        }

        // event no need to destroy
        kupl_event_t *event = kupl_event_create_with_udata(sizeof(kernel_data));
        if (kupl_unlikely(event == nullptr)) {
            return KUPL_ERROR;
        }

        kernel_data *data = reinterpret_cast<kernel_data *>(event->task->udata);
        data->field_mask = desc->field_mask;
        data->range = *desc->range;
        data->egroup = desc->egroup;
        data->func = kernel;

        const char *kernel_name = desc->field_mask & KUPL_QUEUE_KERNEL_DESC_FIELD_NAME ? desc->name : "auto";

        kupl_tb_desc_t tb_desc = {
            .field_mask = KUPL_TB_DESC_FIELD_NAME,
            .func = lambda_kernel_wrapper,
            .args = data,
            .name = kernel_name,
        };

        int ret = kupl_event_init(event, queue, &tb_desc, KUPL_EVENT_TYPE_KERNEL);
        if (kupl_unlikely(ret != KUPL_OK)) {
            kupl_event_destroy(event);
            return ret;
        }
        kupl_enqueue_event(queue, event);
        return KUPL_OK;
    }

    int queue_submit(kupl_queue_h queue, kupl_queue_item_desc_t *desc,
                     const std::function<void(void)> &func)
    {
        if (kupl_unlikely(queue == nullptr || desc == nullptr ||
            func == nullptr)) {
            return kupl_log_error_return(WARN, "queue submit with invalid params");
        }
        if ((desc->field_mask & KUPL_QUEUE_ITEM_DESC_FIELD_NAME) && desc->name == nullptr) {
            return kupl_log_error_return(WARN, "queue submit with invalid name");
        }
        queue->lock->lock(queue->lock);
        if (queue->event_count == 0) {
            if (kupl_unlikely(KUPL_OK != kupl_queue_submit_request(queue))) {
                queue->lock->unlock(queue->lock);
                return KUPL_ERROR;
            }
        }
        queue->lock->unlock(queue->lock);

        // event no need to destroy
        kupl_event_t *event = kupl_event_create_with_udata(sizeof(lambda_func_data));
        if (kupl_unlikely(event == nullptr)) {
            return KUPL_ERROR;
        }
        lambda_func_data *data = reinterpret_cast<lambda_func_data *>(event->task->udata);
        memset((void*)data, 0, sizeof(lambda_func_data));
        data->func = func;
        const char *name = desc->field_mask & KUPL_QUEUE_ITEM_DESC_FIELD_NAME ? desc->name : "auto";
        uint64_t field_mask = queue->priority < 0 ? KUPL_TB_DESC_FIELD_NAME :
                              KUPL_TB_DESC_FIELD_NAME | KUPL_TB_DESC_FIELD_PRIORITY;
        kupl_tb_desc_t tb_desc = {
            .field_mask = field_mask,
            .func = lambda_func,
            .args = data,
            .name = name,
            .priority = queue->priority,
        };

        if ((desc->field_mask & KUPL_QUEUE_ITEM_DESC_FIELD_EGROUP) && (desc->egroup != nullptr)) {
            tb_desc.egroup = desc->egroup;
            tb_desc.executor_id = (int)kupl_egroup_master_eid(desc->egroup);
            tb_desc.field_mask |= KUPL_TB_DESC_FIELD_EGROUP;
            tb_desc.field_mask |= KUPL_TB_DESC_FIELD_EXECUTOR_ID;
        }

        int ret = kupl_event_init(event, queue, &tb_desc, KUPL_EVENT_TYPE_KERNEL);
        if (kupl_likely(ret == KUPL_OK)) {
            kupl_enqueue_event(queue, event);
        } else {
            kupl_event_destroy(event);
        }
        return ret;
    }

}

kupl_queue_h kupl_queue_acquire(int index)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(*g_queue_table_mutex);
    if (g_queue_table->find(index) != g_queue_table->end()) {
        return (*g_queue_table)[index];
    }

    kupl_queue_h queue = kupl_queue_create();
    if (kupl_likely(queue != nullptr)) {
        queue->index = index;
        queue->acquire = true;
        if (index == KUPL_ASYNC_SYNC) {
            queue->sync = true;
        }
        (*g_queue_table)[index] = queue;
    }
    return queue;
}

static uint64_t encode_egroup_key(int eid, int nth)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(eid)) << BITS_IN_UINT32) |
           static_cast<uint64_t>(static_cast<uint32_t>(nth));
}

kupl_egroup_h kupl_queue_acquire_egroup(kupl_queue_h queue)
{
    int nth = kupl_get_kernel_concurrency();
    if (kupl_unlikely(nth <= 0)) {
        kupl_error("nth <= 0");
        return nullptr;
    }
    int groups = kupl_get_num_executors() / nth;
    if (kupl_unlikely(groups <= 0)) {
        kupl_error("groups <= 0");
        return nullptr;
    }
    int eid = (queue->index - 1) % groups * nth;
    std::lock_guard<std::mutex> lock(*g_egroup_table_mutex);
    uint64_t key = encode_egroup_key(eid, nth);
    if (g_egroup_table->find(key) != g_egroup_table->end()) {
        return (*g_egroup_table)[key];
    }
    static int exec[KUPL_EXECUTOR_ID_MAX];
    for (int i = 0; i < nth; i++) {
        exec[i] = i + eid;
    }
    auto egroup = kupl_egroup_create(exec, nth);
    if (kupl_likely(egroup != nullptr)) {
        (*g_egroup_table)[key] = egroup;
    }
    return egroup;
}

int kupl_queue_wait_all()
{
    int status = KUPL_OK;
    for (auto it = g_queue_table->begin(); it != g_queue_table->end(); ++it) {
        const auto& index = it->first;
        if (kupl_queue_wait(it->second) != KUPL_OK) {
            status = KUPL_ERROR;
            kupl_error("kupl_queue_wait failed, index=%d", index);
        }
    }
    return status;
}

int kupl_queue_init()
{
    g_queue_table = new (std::nothrow) std::map<int, kupl_queue_h>();
    g_queue_table_mutex = new (std::nothrow) std::mutex();
    g_egroup_table = new (std::nothrow) std::unordered_map<uint64_t, kupl_egroup_h>();
    g_egroup_table_mutex = new (std::nothrow) std::mutex();
    if (kupl_unlikely((g_queue_table == nullptr) || (g_queue_table_mutex == nullptr) ||
        (g_egroup_table == nullptr) || (g_egroup_table_mutex == nullptr))) {
        goto error;
    }
    return KUPL_OK;
error:
    kupl_queue_fini();
    return KUPL_ERROR;
}

void kupl_queue_fini()
{
    if (g_egroup_table_mutex != nullptr) {
        delete g_egroup_table_mutex;
        g_egroup_table_mutex = nullptr;
    }
    if (g_egroup_table != nullptr) {
        for (auto& it : *g_egroup_table) {
            kupl_egroup_destroy(it.second);
        }
        delete g_egroup_table;
        g_egroup_table = nullptr;
    }
    if (g_queue_table_mutex != nullptr) {
        delete g_queue_table_mutex;
        g_queue_table_mutex = nullptr;
    }
    if (g_queue_table != nullptr) {
        std::vector<kupl_queue_h> queue_vec;
        queue_vec.reserve(g_queue_table->size());
        for (auto& kv : *g_queue_table) {
            queue_vec.push_back(kv.second);
        }
        for (auto& q : queue_vec) {
            kupl_queue_destroy(q);
        }
        delete g_queue_table;
        g_queue_table = nullptr;
    }
}
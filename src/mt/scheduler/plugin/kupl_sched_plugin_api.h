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
#ifndef KUPL_SCHED_PLUGIN_API_H
#define KUPL_SCHED_PLUGIN_API_H

#include <cstddef>
#include <queue>
#include "utils/lock/kupl_lock.h"
#include "tools/struct/kupl_vector.h"
#include "mt/scheduler/kupl_sched.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *           =====  Scheduler plugin develop document  =========
 *
 * There are some things to do to implement one scheduler plugin
 * 1. Plugin must have a name, and this name can't be same with other plugin
 * 2. Some plugin's rulers will use this name:
 *   > 2.0 The plugin's name should keep same with the plugin directory's name
 *   > 2.1 Code must have a kupl_sched_plugin_property_t type global API variable called `g_sched_plugin_name`
 *         which will be read from dlsym() by dynamic load.
 *         We recommend user use @ref KUPL_SCHED_PLUGIN_GLOBAL_VAR() macro to define your global API variable
 *   > 2.2 When compile plugin to dynamic library, the library must be `libsched_plugin_name.so` for auto
 *         detect plugin's name.
 *         We recommend user copy CMakeList.txt from scheduler/plugin/demo/CMakeList.txt and modify it
 *   > 2.3 If plugin's name is demo, it is `g_sched_plugin_demo` and `libsched_plugin_demo.so`
 * 3. Plugin should implement all the APIs in @ref kupl_sched_plugin_api_t
 * 4. Plugin must be thread-safe
 * 5. Plugin can read environment
 */

/**
 * @brief Plugin's property
 */
typedef struct kupl_sched_plugin_property {
    const char *name;
    size_t private_data_len;
    int score;                      /* higher score will be auto selected, when score is same will chose the last */
} kupl_sched_plugin_property_t;

/**
 * @brief Initialize the plugin and get the property of this plugin
 *
 * @param [out] property    the property of this plugin
 * @return                  KUPL_OK for success
 */
typedef int (*kupl_sched_plugin_init_t)(kupl_sched_plugin_property_t *property);

/**
 * @brief Uninitialize the plugin
 */
typedef void (*kupl_sched_plugin_fini_t)(void);

/**
 * @brief Create one sched plugin instance
 */
typedef void* (*kupl_sched_plugin_instance_create_t)(void);

/**
 * @brief Expand one sched plugin instance
 */
typedef void* (*kupl_sched_plugin_instance_expand_t)(void *sched);

/**
 * @brief destroy the sched plugin instance
 */
typedef void (*kupl_sched_plugin_instance_cleanup_t)(void *sched);

/**
 * @brief Add one tb to plugin's scheduler
 *
 * @param [in] tb           the kupl-tb create by kupl
 * @return                  KUPL_OK for success other for failed
 */
typedef int (*kupl_sched_add_ready_tb_t)(void *sched, kupl_taskbase_t *tb);

/**
 * @brief Get one tb from plugin scheduler
 *
 * @param [in] cp           the thread's topology position in CPU
 *
 * @return          get one tb, NULL for no tb or some errors happen
 */
typedef kupl_taskbase_t* (*kupl_sched_get_ready_tb_t)(void *sched, kupl_compute_place_t cp);

/**
 * @brief Plugin's api
 */
typedef struct kupl_scheduler_plugin_api {
    const char *name;
    kupl_sched_plugin_init_t init;
    kupl_sched_plugin_fini_t fini;
    kupl_sched_plugin_instance_create_t create;
    kupl_sched_plugin_instance_expand_t expand;
    kupl_sched_plugin_instance_cleanup_t cleanup;
    kupl_sched_add_ready_tb_t add_tb;
    kupl_sched_get_ready_tb_t get_tb;
} kupl_sched_plugin_api_t;

#define KUPL_SCHED_PLUGIN_LIB_PREFIX                 "libsched_plugin_"
#define KUPL_SCHED_PLUGIN_GLOBAL_VAR_PREFIX          "g_sched_plugin_"
#define KUPL_SCHED_PLUGIN_GLOBAL_VAR(_plugin_name)    g_sched_plugin_ ## _plugin_name

struct kupl_mq_priority_cmp {
    bool operator()(const kupl_taskbase_t *a, const kupl_taskbase_t *b)
    {
        return a->priority < b->priority;
    }
};

using kupl_mq_priority_queue_t = std::priority_queue<kupl_taskbase_t *,
                                                      std::vector<kupl_taskbase_t *>, kupl_mq_priority_cmp>;

typedef struct priority_queue {
    kupl_mq_priority_queue_t   *q;
    kupl_vector_t              *spsc;
    kupl_lock_t                *add_lock;
    kupl_lock_t                *get_lock;
    KUPL_ATOMIC_INT            tb_cnt;
    int                        size;
} priority_queue_t;

static kupl_always_inline
void priority_queue_cleanup(priority_queue_t *queue)
{
    if (kupl_unlikely(queue == nullptr)) {
        return;
    }
    kupl_lock_cleanup(queue->get_lock);
    kupl_lock_cleanup(queue->add_lock);
    kupl_vector_cleanup(queue->spsc);
    delete queue->q;
    kupl_safe_free(queue);
}

static kupl_always_inline
priority_queue_t *priority_queue_create(int size)
{
    priority_queue_t *queue = (priority_queue_t *)kupl_malloc_inner(sizeof(priority_queue_t));
    if (queue == nullptr) {
        return nullptr;
    }
    queue->q = new (std::nothrow) kupl_mq_priority_queue_t;
    queue->spsc = kupl_vector_create((size_t)size, sizeof(kupl_taskbase_t *), "sched mq priority queue");
    queue->add_lock = kupl_lock_create(PTHREAD_SPINLOCK);
    queue->get_lock = kupl_lock_create(PTHREAD_SPINLOCK);
    if (kupl_unlikely((queue->q == nullptr) || (queue->spsc == nullptr) ||
                       (queue->add_lock == nullptr) || (queue->get_lock == nullptr))) {
        goto err;
    }
    queue->tb_cnt = 0;
    queue->size = size;
    return queue;
err:
    priority_queue_cleanup(queue);
    return nullptr;
}

static kupl_always_inline
int priority_queue_add_tb(priority_queue_t *queue, kupl_taskbase_t *tb)
{
    int err = KUPL_ERROR;
    if (kupl_unlikely(queue == nullptr)) {
        kupl_warn("priority queue not exist");
        return err;
    }
    auto lock = queue->add_lock;
    lock->lock(lock);
    err = kupl_vector_push_back_macro(queue->spsc, tb);
    lock->unlock(lock);
    return err;
}

static kupl_always_inline
kupl_taskbase_t *priority_queue_get_tb(priority_queue_t *queue)
{
    kupl_taskbase_t *tb = nullptr;
    if (kupl_unlikely(queue == nullptr)) {
        kupl_warn("priority queue not exist");
        return tb;
    }
    if (KUPL_ATOMIC_LD_RLX(&queue->tb_cnt) <= 0 && kupl_vector_empty_macro(queue->spsc)) {
        return nullptr;
    }
    auto lock = queue->get_lock;
    lock->lock(lock);
    kupl_taskbase_t *spsc_tb = nullptr;
    while (!kupl_vector_empty_macro(queue->spsc)) {
        if ((int)queue->q->size() >= queue->size) {
            break;
        }

        kupl_vector_front_macro(queue->spsc, spsc_tb);
        queue->q->push(spsc_tb);
        kupl_vector_pop_front_macro(queue->spsc);
    }
    if (!queue->q->empty()) {
        tb = queue->q->top();
        queue->q->pop();
    }
    KUPL_ATOMIC_ST(&queue->tb_cnt, (int)queue->q->size());
    lock->unlock(lock);
    return tb;
}

#ifdef __cplusplus
}
#endif

#endif
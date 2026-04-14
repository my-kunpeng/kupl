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
#include "kupl_sched_mq.h"
#include <vector>
#include <queue>
#include <cctype>
#include <cstdlib>
#include <cerrno>
#include "mt/scheduler/plugin/kupl_sched_plugin_api.h"
#include "executor/kupl_executor.h"
#include "executor/backend/kupl_executor_backend.h"
#include "utils/arch/kupl_atomic.h"
#include "utils/config/kupl_config.h"
#include "utils/lock/kupl_lock.h"
#include "tools/struct/kupl_vector.h"
#include "tools/struct/kupl_list.h"

#define KUPL_RANDOM_SHIFT              16
#define KUPL_MQ_DEFAULT                (-1)

static const unsigned kupl_primes[] = {
    0x9e3779b1, 0xffe6cc59, 0x2109f6dd, 0x43977ab5, 0xba5703f5, 0xb495a877,
    0xe1626741, 0x79695e6b, 0xbc98c09f, 0xd5bee2b3, 0x287488f9, 0x3af18231,
    0x9677cd4d, 0xbe3a6929, 0xadc6a877, 0xdcf0674b, 0xbe4d6fe9, 0x5f15e201,
    0x99afc3fd, 0xf3f16801, 0xe222cfff, 0x24ba5fdb, 0x0620452d, 0x79f149e3,
    0xc8b93f49, 0x972702cd, 0xb07dd827, 0x6c97d5ed, 0x085a3d61, 0x46eb5ea7,
    0x3d9910ed, 0x2e687b5b, 0x29609227, 0x6eb081f1, 0x0954c4e1, 0x9d114db9,
    0x542acfa9, 0xb3e6bd7b, 0x0742d917, 0xe9f3ffa7, 0x54581edb, 0xf2480f45,
    0x0bb9288f, 0xef1affc7, 0x85fa0ca7, 0x3ccc14db, 0xe6baf34b, 0x343377f7,
    0x5ca19031, 0xe6d9293b, 0xf0a9f391, 0x5d2e980b, 0xfc411073, 0xc3749363,
    0xb892d829, 0x3549366b, 0x629750ad, 0xb98294e5, 0x892d9483, 0xc235baf3,
    0x3d2402a3, 0x6bdef3c9, 0xbec333cd, 0x40c9520f};


static thread_local unsigned th_x, th_a;
static thread_local bool random_inited = false;
static int enable_priority;

static kupl_always_inline
unsigned short kupl_get_random() {
    unsigned x = th_x;
    unsigned short r = (unsigned short)(x >> KUPL_RANDOM_SHIFT);

    th_x = x * th_a + 1;

    return r;
}

static kupl_always_inline
void kupl_init_random(int executor_id) {
    if (random_inited) {
        return;
    }
    unsigned seed = (unsigned)executor_id;

    th_a =
        kupl_primes[seed % (sizeof(kupl_primes) / sizeof(kupl_primes[0]))];
    th_x = (seed + 1) * th_a + 1;
    random_inited = true;
}

typedef struct queue {
    kupl_vector_t    *q;
    kupl_lock_t      *add_lock;
    kupl_lock_t      *get_lock;
} queue_t;

static kupl_always_inline
void queue_cleanup(queue_t *queue)
{
    if (queue == nullptr) {
        return;
    }
    kupl_lock_cleanup(queue->get_lock);
    kupl_lock_cleanup(queue->add_lock);
    kupl_vector_cleanup(queue->q);
    kupl_safe_free(queue);
}

static kupl_always_inline
queue_t *queue_create(int size)
{
    queue_t *queue = (queue_t *)kupl_malloc_inner(sizeof(queue_t));
    if (queue == nullptr) {
        return nullptr;
    }
    queue->q = kupl_vector_create((size_t)size, sizeof(kupl_taskbase_t *), "sched mq queue");
    queue->add_lock = kupl_lock_create(PTHREAD_SPINLOCK);
    queue->get_lock = kupl_lock_create(PTHREAD_SPINLOCK);
    if (kupl_unlikely(queue->q == nullptr || queue->add_lock == nullptr || queue->get_lock == nullptr)) {
        goto err;
    }
    return queue;
err:
    queue_cleanup(queue);
    return nullptr;
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

typedef struct place_queue {
    queue_t     **qs;
    cpu_set_t   *eid_set;
    int         queue_cnt;
} place_queue_t;

static kupl_always_inline
int parse_eid(char *token) {
    char *end = nullptr;
    long eid = std::strtol(token, &end, KUPL_BASE_DEC);
    if (end == nullptr || end == token || *end != '\0' || errno == ERANGE) {
        return KUPL_EIDCID_INIT;
    }
    if (eid < 0 || eid >= CPU_SETSIZE) {
        return KUPL_EIDCID_INIT;
    }
    return (int)eid;
}

static kupl_always_inline
void parse_place_scatter(char *str, cpu_set_t *eid_set)
{
    char *save;
    char *token = strtok_r(str, ",", &save);
    while (token != nullptr) {
        int eid = parse_eid(token);
        if (kupl_unlikely(eid == KUPL_EIDCID_INIT)) {
            continue;
        }
        CPU_SET(eid, eid_set);
        token = strtok_r(nullptr, ",",  &save);
    }
}

static kupl_always_inline
void parse_place_range(char *str, cpu_set_t *eid_set)
{
    int range_start = 0;
    int range_end = 0;
    char *save;
    char *token = strtok_r(str, "-", &save);
    if (token != nullptr) {
        range_start = parse_eid(token);
        token = strtok_r(nullptr, "-",  &save);
    }
    if (token != nullptr) {
        range_end = parse_eid(token);
    }
    if (range_start < range_end) {
        kupl_debug("mq range place: %d-%d", range_start, range_end);
        for (int eid = range_start; eid <= range_end; eid++) {
            if (kupl_unlikely(eid == KUPL_EIDCID_INIT)) {
                continue;
            }
            CPU_SET(eid, eid_set);
        }
    } else {
        kupl_error("invalid mq range place: %d-%d", range_start, range_end);
    }
}

static kupl_always_inline
void parse_place_single(char *str, cpu_set_t *eid_set)
{
    int eid = parse_eid(str);
    if (kupl_unlikely(eid == KUPL_EIDCID_INIT)) {
        return;
    }
    CPU_SET(eid, eid_set);
}

static kupl_always_inline
bool parse_place(char *_str, cpu_set_t *eid_set)
{
    const size_t max_len = 20;
    size_t len = strlen(_str) + 1;
    if (len > max_len) {
        return false;
    }
    char str[max_len];
    CPU_ZERO(eid_set);

    memcpy(str, _str, len);

    if (strchr(str, ',') != nullptr) {
        parse_place_scatter(str, eid_set);
    } else if (strchr(str, '-') != nullptr) {
        parse_place_range(str, eid_set);
    } else {
        parse_place_single(str, eid_set);
    }

    int num_executors = kupl_get_num_executors();
    cpu_set_t sys_eid_set;
    CPU_ZERO(&sys_eid_set);
    for (int eid = 0; eid < num_executors; eid++) {
        CPU_SET(eid, &sys_eid_set);
    }
    CPU_AND(eid_set, eid_set, &sys_eid_set);
    if (CPU_COUNT(eid_set) > 0) {
        return true;
    }
    return false;
}

static kupl_always_inline
int cal_place_queue_cnt(char* affinity)
{
    // without place queue
    if (*affinity == 0) {
        kupl_info("place queue not used");
        return 0;
    }

    // calculate place queue count
    int place_cnt = 1;
    for (int i = 0; affinity[i]; i++) {
        if (isdigit(affinity[i])) {
            continue;
        }
        switch (affinity[i]) {
        case '|':
            place_cnt++;
            break;
        case ',':
        case '-':
            break;
        default:
            kupl_warn("mq place queue invalid affinity");
            return KUPL_MQ_DEFAULT;
        }
    }
    return place_cnt;
}

static kupl_always_inline
void place_queue_cleanup(place_queue_t *queue)
{
    if (queue == nullptr) {
        return;
    }
    if (queue->qs != nullptr) {
        for (int i = 0; i < queue->queue_cnt; i++) {
            queue_cleanup(queue->qs[i]);
        }
    }
    kupl_safe_free(queue->qs);
    kupl_safe_free(queue->eid_set);
    kupl_safe_free(queue);
}

static kupl_always_inline
place_queue_t *place_queue_create(int size, char* affinity)
{
    place_queue_t *queue = (place_queue_t *)kupl_calloc(sizeof(place_queue_t), 1);
    if (queue == nullptr) {
        return nullptr;
    }
    queue->queue_cnt = cal_place_queue_cnt(affinity);
    if (queue->queue_cnt <= 0) {
        return queue;
    }
    cpu_set_t *eid_set = nullptr;

    // parse affinity
    queue->eid_set = (cpu_set_t *)kupl_malloc_inner((size_t)queue->queue_cnt * sizeof(cpu_set_t));
    if (queue->eid_set == nullptr) {
        goto error;
    }
    char *token;
    char *save;

    token = strtok_r(affinity, "|", &save);
    eid_set = &queue->eid_set[0];
    while (token != nullptr) {
        if (parse_place(token, eid_set)) {
            eid_set++;
        }
        token = strtok_r(nullptr, "|", &save);
    }
    queue->qs = (queue_t **)kupl_calloc((size_t)queue->queue_cnt, sizeof(queue_t *));
    if (kupl_unlikely(queue->qs == nullptr)) {
        goto error;
    }
    for (int i = 0; i < queue->queue_cnt; i++) {
        queue->qs[i] = queue_create(size);
        if (kupl_unlikely(queue->qs[i] == nullptr)) {
            goto error;
        }
    }
    if (queue->queue_cnt == 0) {
        kupl_warn("place queue not used");
    }
    return queue;
error:
    place_queue_cleanup(queue);
    return nullptr;
}

typedef struct kupl_sched_mq {
    int                 queue_size;         // queue size
    int                 num_executors;      // executor num
    priority_queue_t    *priority_queues;  // priority_queues own by executors
    queue_t             **local_queues;     // local_queues own by executors
    kupl_slist_t       **place_queues;     // place_queues own by executors
    place_queue_t       *real_place_queues; // real place_queues
    int                 *last_steal;
} kupl_sched_mq_t;

static int kupl_sched_mq_init(kupl_sched_plugin_property_t *property)
{
    enable_priority = kupl_config_get_value(KUPL_ENABLE_PRIORITY);
    property->name = KUPL_SCHED_PLUGIN_MQ_NAME;
    property->private_data_len = 0;
    property->score = KUPL_SCHED_PLUGIN_MQ_SCORE;

    return KUPL_OK;
}
static void kupl_sched_mq_fini()
{
}

static kupl_always_inline
int kupl_create_place_queue_list(int index, kupl_sched_mq_t *sched)
{
    // create place queues
    auto pq = sched->real_place_queues;
    for (int j = pq->queue_cnt - 1; j >= 0; j--) {
        if (!CPU_ISSET(index, &pq->eid_set[j])) {
            continue;
        }
        auto link = (kupl_slist_t *)kupl_malloc_inner(sizeof(kupl_slist_t));
        if (kupl_unlikely(link == nullptr)) {
            return KUPL_ERROR;
        }
        link->next = sched->place_queues[index];
        link->data = pq->qs[j];
        sched->place_queues[index] = link;
        kupl_info("create place queue: %d on executor: %d", j, index);
    }
    return KUPL_OK;
}

static void kupl_sched_mq_cleanup(void *_sched);
static void* kupl_sched_mq_create()
{
    kupl_sched_mq_t *sched = (kupl_sched_mq_t *)kupl_calloc(1, sizeof(kupl_sched_mq_t));
    if (kupl_unlikely(sched == nullptr)) {
        return nullptr;
    }
    auto host_info = kupl_get_host_info();
    auto num_executors = host_info->avail_pu_cnt;
    sched->num_executors = num_executors;
    sched->queue_size = kupl_config_get_value(KUPL_SCHED_QUEUE_LENGTH);

    char *affinity = strdup(kupl_config_get_value_str(KUPL_SCHED_MQ_PLACEQ_AFFINITY));
    if (kupl_unlikely(affinity == nullptr)) {
        goto err;
    }
    static int place_queue_size_factor = 10;
    sched->real_place_queues = place_queue_create(sched->queue_size * place_queue_size_factor, affinity);
    free(affinity);

    // create priority queues
    sched->priority_queues = priority_queue_create(sched->queue_size);
    sched->local_queues = (queue_t **)kupl_malloc_inner((size_t)sched->num_executors * sizeof(queue_t *));
    sched->place_queues = (kupl_slist_t **)kupl_malloc_inner((size_t)sched->num_executors * sizeof(kupl_slist_t *));
    sched->last_steal = (int *)kupl_malloc_inner((size_t)sched->num_executors * sizeof(int));
    if (sched->real_place_queues == nullptr || sched->priority_queues == nullptr || sched->local_queues == nullptr ||
        sched->place_queues == nullptr || sched->last_steal == nullptr) {
        goto err;
    }
    for (int i = 0; i < sched->num_executors; ++i) {
        sched->local_queues[i] = nullptr;
        sched->place_queues[i] = nullptr;
        sched->last_steal[i] = KUPL_MQ_DEFAULT;

        // create local queues
        sched->local_queues[i] = queue_create(sched->queue_size);
        if (kupl_unlikely(sched->local_queues[i] == nullptr)) {
            goto err;
        }

        // create place queues
        if (kupl_create_place_queue_list(i, sched) == KUPL_ERROR) {
            goto err;
        }
    }
    return sched;

err:
    kupl_warn("kupl_sched_mq_create failed");
    kupl_sched_mq_cleanup(sched);
    return nullptr;
}

static void kupl_sched_mq_cleanup(void *_sched)
{
    kupl_sched_mq_t *sched = (kupl_sched_mq_t *)_sched;
    if (sched == nullptr) {
        return;
    }
    for (int i = 0; i < sched->num_executors; ++i) {
        auto place_queues = sched->place_queues[i];
        while (place_queues != nullptr) {
            kupl_slist_t *tmp = place_queues;
            place_queues = place_queues->next;
            kupl_safe_free(tmp);
        }
        queue_cleanup(sched->local_queues[i]);
    }
    priority_queue_cleanup(sched->priority_queues);
    kupl_safe_free(sched->place_queues);
    kupl_safe_free(sched->local_queues);
    kupl_safe_free(sched->last_steal);
    place_queue_cleanup(sched->real_place_queues);
    kupl_safe_free(sched);
    return;
}

static int kupl_sched_mq_add_tb(void *_sched, kupl_taskbase_t *tb)
{
    kupl_sched_mq_t *sched = (kupl_sched_mq_t *)_sched;
    int err = KUPL_OK;
    // executor id of current executor thread
    int own_eid = kupl_get_executor_num();
    if (kupl_unlikely(own_eid == KUPL_EXECUTOR_DEFAULT)) {
        return kupl_log_error_return(ERROR, "invoke KUPL functions on threads not managed by KUPL");
    }
    // executor id of expect executor thread
    int eid;

    if (tb->executor_id == KUPL_TB_EXECUTOR_ID_DEFAULT) {
        // add to own local queue
        eid = own_eid;
    } else if (tb->executor_id == KUPL_TB_EXECUTOR_ID_PLACE) {
        // add to own place queue
        if (sched->place_queues[own_eid] != nullptr) {
            goto place_queue;
        } else {
            eid = own_eid;
        }
    } else if (tb->executor_id >= 0) {
        // add to other local queue
        eid = tb->executor_id;
    } else {
        kupl_warn("invalid tb executor_id: %d", tb->executor_id);
        eid = own_eid;
    }

    // priority_queue
    if (enable_priority && (tb->flag & KUPL_TB_FLAG_PRIORITY)) {
        return priority_queue_add_tb(sched->priority_queues, tb);
    }

    // local_queue
    err = queue_add_tb(sched->local_queues[eid], tb);
    if (err == KUPL_OK) {
        return KUPL_OK;
    }

place_queue:
    auto place_queues = sched->place_queues[own_eid];
    while (place_queues) {
        err = queue_add_tb((queue_t *)place_queues->data, tb);
        if (err == KUPL_OK) {
            return KUPL_OK;
        }
        place_queues = place_queues->next;
    }
    return err;
}

static kupl_always_inline
kupl_taskbase_t *pop_one_tb(kupl_sched_mq_t *sched, int executor_id)
{
    kupl_taskbase_t *tb = nullptr;

    // priority_queue
    if (enable_priority) {
        tb = priority_queue_get_tb(sched->priority_queues);
        if (tb != nullptr) {
            return tb;
        }
    }

    // local_queue
    tb = queue_get_tb(sched->local_queues[executor_id]);
    if (tb != nullptr) {
        return tb;
    }
    return tb;
}

static kupl_taskbase_t *kupl_sched_mq_get_tb(void *_sched, kupl_compute_place_t cp)
{
    kupl_sched_mq_t *sched = (kupl_sched_mq_t *)_sched;
    (void)cp;
    int executor_id = kupl_get_executor_num();
    if (kupl_unlikely(executor_id == KUPL_EXECUTOR_DEFAULT)) {
        kupl_error("invoke KUPL functions on threads not managed by KUPL");
        return nullptr;
    }

    kupl_taskbase_t *tb = nullptr;
    tb = pop_one_tb(sched, executor_id);
    if (tb != nullptr) {
        return tb;
    }

    // place_queues
    auto place_queues = sched->place_queues[executor_id];
    while (place_queues) {
        tb = queue_get_tb((queue_t *)place_queues->data);
        if (tb != nullptr) {
            return tb;
        }
        place_queues = place_queues->next;
    }

    // steal
    if (sched->num_executors <= 1) {
        return tb;
    }
    // steal from last
    int last_steal = sched->last_steal[executor_id];
    if (sched->last_steal[executor_id] != KUPL_MQ_DEFAULT) {
        tb = pop_one_tb(sched, last_steal);
        if (tb != nullptr) {
            return tb;
        }
    }

    kupl_init_random(executor_id);
    int steal_retry = 1;
    while (steal_retry > 0) {
        int steal_eid = kupl_get_random() % sched->num_executors;
        if (executor_id == steal_eid) {
            continue;
        }
        tb = pop_one_tb(sched, steal_eid);
        if (tb != nullptr) {
            sched->last_steal[executor_id] = steal_eid;
            return tb;
        } else {
            sched->last_steal[executor_id] = KUPL_MQ_DEFAULT;
            steal_retry--;
        }
    }

    return tb;
}

static const kupl_sched_plugin_api_t KUPL_SCHED_PLUGIN_GLOBAL_VAR(mq) = {
    .name = KUPL_SCHED_PLUGIN_MQ_NAME,
    .init = kupl_sched_mq_init,
    .fini = kupl_sched_mq_fini,
    .create = kupl_sched_mq_create,
    .expand = nullptr,
    .cleanup = kupl_sched_mq_cleanup,
    .add_tb = kupl_sched_mq_add_tb,
    .get_tb = kupl_sched_mq_get_tb,
};

const kupl_sched_plugin_api_t* kupl_sched_mq_get_instance()
{
    return &KUPL_SCHED_PLUGIN_GLOBAL_VAR(mq);
}
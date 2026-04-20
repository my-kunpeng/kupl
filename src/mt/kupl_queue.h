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
#ifndef KUPL_QUEUE_H
#define KUPL_QUEUE_H

#include <pthread.h>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "kupl.h"
#include "utils/lock/kupl_lock.h"
#include "mt/kupl_event.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KUPL_LEAST_PRIORITY 0
#define KUPL_GREATEST_PRIORITY 3
#define KUPL_RESERVE_SIZE 128

typedef struct kupl_queue {
    kupl_event_h        event;              /* events of the enqueued commands in enqueue order */
    unsigned long       event_count;        /* counter for unfinished event */
    kupl_event_h        last_event;
    kupl_lock_t         *lock;
    int                 priority;
    void                *sdma_chn;
    std::vector<kupl_sdma_request_h> *req_set;
    int                 index;
    bool                acquire;
    bool                sync;
} kupl_queue_t;

void kupl_enqueue_event(kupl_queue_h queue, kupl_event_h event);

void kupl_dequeue_event(kupl_queue_h queue, kupl_event_h event);

kupl_egroup_h kupl_queue_acquire_egroup(kupl_queue_h queue);

kupl_always_inline
bool kupl_queue_is_sync(kupl_queue_h queue)
{
    return queue->sync;
}

int kupl_queue_init();

void kupl_queue_fini();

#ifdef __cplusplus
}
#endif

#endif
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
#ifndef KUPL_EVENT_H
#define KUPL_EVENT_H

#include <vector>
#include "kupl.h"
#include "kupl_task.h"
#include "utils/thirdpart/sdma/sdma_module.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum kupl_event_type {
    KUPL_EVENT_TYPE_INIT,
    KUPL_EVENT_TYPE_WAIT,
    KUPL_EVENT_TYPE_RECORD,
    KUPL_EVENT_TYPE_KERNEL,
    KUPL_EVENT_TYPE_SDMA,
    KUPL_EVENT_TYPE_SDMA_WAIT,
} kupl_event_type_t;

typedef struct kupl_sdma_request {
    bool flag;
    sdma_request_t request;
    kupl_event_h event;
} kupl_sdma_request_t;

typedef struct kupl_sdma_request* kupl_sdma_request_h;

typedef struct kupl_event {
    kupl_task_h             task;
    kupl_task_func_t        func;
    void                    *args;
    kupl_queue_h            q;
    kupl_graph_h            graph;
    kupl_event_h            wait;
    kupl_event_type_t       type;
    kupl_sdma_request_h     req;
    kupl_lock_t             *lock;
    KUPL_ATOMIC_INT         status;
    KUPL_ATOMIC_INT         ref;
    void                    *m_args;    // args used for memcpy
    std::vector<kupl_queue_h> *q_set;
} kupl_event_t;

kupl_event_h kupl_event_create_with_udata(size_t udata_size);

int kupl_event_init(kupl_event_h event, kupl_queue_h queue, kupl_tb_desc_t *task_desc, kupl_event_type_t type);

int kupl_event_init_wait(kupl_event_h event, kupl_queue_h queue, kupl_event_h wait_event);

void kupl_event_set_status(kupl_event_h event, kupl_event_status_t status);

void kupl_event_ref(kupl_event_h event);

void kupl_event_deref(kupl_event_h event);

void kupl_event_submit(kupl_event_t *event, kupl_event_t *last_event);

#ifdef __cplusplus
}
#endif

#endif
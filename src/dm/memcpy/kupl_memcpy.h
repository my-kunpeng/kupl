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
#ifndef KUPL_MEMCPY_H
#define KUPL_MEMCPY_H

#include <cstddef>
#include "mt/kupl_event.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kupl_sdma_req {
    sdma_sqe_task_t task;
    sdma_request_t request;
    kupl_sdma_chn_h chn;
} kupl_sdma_req_t;

typedef struct kupl_sdma_async {
    sdma_request_t request;
    int chn_index;
} kupl_sdma_async_t;

int kupl_memcpy_init(void);

void kupl_memcpy_fini(void);

void* kupl_get_sdma_chn(void);

int kupl_sdma_wait_req(kupl_queue_h queue, sdma_request_t *request);
/**
 * @brief Check whether sdma memcpy func is inited
 *
 * @return sdma_memcpy_func_init
 */
bool kupl_get_sdma_memcpy_func_init(void);

int kupl_sdma_wait_event(kupl_event_h event);

int kupl_sdma_query_event(kupl_event_h event);

#ifdef __cplusplus
}
#endif

#endif

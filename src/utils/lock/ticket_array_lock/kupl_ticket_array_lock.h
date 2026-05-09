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
#ifndef KUPL_TICKET_ARRAY_LOCK_H
#define KUPL_TICKET_ARRAY_LOCK_H

#include "utils/arch/kupl_atomic.h"
#include "utils/arch/kupl_cache.h"

typedef union KUPL_ALIGN_CACHE kupl_tal_ticket {
    KUPL_ATOMIC_UINT64 value;
    char pad[KUPL_PAD_CACHE(KUPL_ATOMIC_UINT64)];
} kupl_tal_ticket_t;

typedef struct kupl_ticket_array_lock {
    kupl_tal_ticket_t *values;
    KUPL_ATOMIC_UINT64 head;
    uint64_t next;
    uint64_t size;
} kupl_ticket_array_lock_t;

typedef struct kupl_lock kupl_lock_t;
kupl_lock_t *kupl_ticket_array_lock_init(void);
void kupl_ticket_array_lock_fini(kupl_lock_t *lock);

#endif
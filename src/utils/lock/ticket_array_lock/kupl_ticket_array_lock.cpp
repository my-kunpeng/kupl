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
#include "kupl_ticket_array_lock.h"
#include "utils/lock/kupl_lock.h"
#include "utils/debug/kupl_assert.h"
#include "utils/sys/kupl_hardware.h"

#define TWO 2

/**
 * @brief TAL is Ticket Array Lock
 */
static void kupl_tal_lock(kupl_lock_t *lock)
{
    kupl_assert(lock != nullptr);
    kupl_ticket_array_lock_t *raw = (kupl_ticket_array_lock_t *)lock->raw_lock;
    uint64_t head = KUPL_ATOMIC_ADD_RLX(&raw->head, 1);
    auto value = &(raw->values[head % raw->size].value);
    while (KUPL_ATOMIC_LD_RLX(value) != head) {
        kupl_spin_wait();
    }
    kupl_spin_wait_release();

    KUPL_MB_ACQ();
}

static void kupl_tal_unlock(kupl_lock_t *lock)
{
    kupl_assert(lock != nullptr);
    kupl_ticket_array_lock_t *raw = (kupl_ticket_array_lock_t *)lock->raw_lock;
    uint64_t idx = ++raw->next % raw->size;
    KUPL_ATOMIC_ST_RLS(&raw->values[idx].value, raw->next);
}

static int kupl_tal_trylock(kupl_lock_t *lock)
{
    kupl_assert(lock != nullptr);
    kupl_ticket_array_lock_t *raw = (kupl_ticket_array_lock_t *)lock->raw_lock;

    uint64_t head = KUPL_ATOMIC_LD_RLX(&raw->head);
    uint64_t idx = head % raw->size;
    if (KUPL_ATOMIC_LD_RLX(&raw->values[idx].value) != head) {
        return 0;
    }

    /* May multi-threads go there, but only one thread can CAS success */
    return KUPL_ATOMIC_CAS_STR_ACQ2RLX(&raw->head, head, head + 1);
}

kupl_lock_t *kupl_ticket_array_lock_init()
{
    const kupl_host_info_t *info = kupl_get_host_info();
    int count = info->pu_cnt * TWO;
    kupl_lock_t *lock = (kupl_lock_t *)kupl_calloc(1, sizeof(kupl_lock_t));
    if (kupl_unlikely(lock == nullptr)) {
        return nullptr;
    }

    kupl_ticket_array_lock_t *raw = (kupl_ticket_array_lock_t *)kupl_calloc(1, sizeof(kupl_ticket_array_lock_t));
    if (kupl_unlikely(raw == nullptr)) {
        goto err_free;
    }

    raw->values = (kupl_tal_ticket_t *)kupl_calloc((size_t)count, sizeof(kupl_tal_ticket_t));
    if (kupl_unlikely(raw->values == nullptr)) {
        goto err_free;
    }

    raw->next = 0;
    raw->head = 0;
    raw->size = (size_t)count;

    lock->type = TICKET_ARRAY_LOCK;
    lock->raw_lock = raw;
    lock->lock = kupl_tal_lock;
    lock->unlock = kupl_tal_unlock;
    lock->trylock = kupl_tal_trylock;

    return lock;

err_free:
    if (raw != nullptr) {
        kupl_safe_free(raw->values);
    }
    kupl_safe_free(raw);
    kupl_safe_free(lock);
    return nullptr;
}

void kupl_ticket_array_lock_fini(kupl_lock_t *lock)
{
    kupl_assert(lock != nullptr);

    kupl_ticket_array_lock_t *raw = (kupl_ticket_array_lock_t *)lock->raw_lock;
    kupl_safe_free(raw->values);
    kupl_safe_free(lock->raw_lock);
    kupl_free_inner(lock);
}

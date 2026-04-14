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
#include "kupl_lock.h"
#include "pthread_spinlock/kupl_pthread_spinlock.h"
#include "ticket_array_lock/kupl_ticket_array_lock.h"
#include "utils/debug/kupl_assert.h"

typedef kupl_lock_t* (*kupl_lock_init_t)();
typedef void (*kupl_lock_fini_t)(kupl_lock_t *lock);
struct kupl_lock_build_table {
    kupl_lock_type_t type;
    kupl_lock_init_t init;
    kupl_lock_fini_t fini;
};

/** @note the order must be same with @ref kupl_lock_type_t */
static kupl_lock_build_table g_tables[] = {
    { PTHREAD_SPINLOCK, kupl_pthread_spinlock_init, kupl_pthread_spinlock_fini },
    { TICKET_ARRAY_LOCK, kupl_ticket_array_lock_init, kupl_ticket_array_lock_fini },
};

kupl_lock_t* kupl_lock_create(kupl_lock_type_t type)
{
    kupl_assert(g_tables[type].type == type);
    return g_tables[type].init();
}

/**
 * @brief Cleanup a kupl_lock_t
 */
void kupl_lock_cleanup(kupl_lock_t *lock)
{
    if (kupl_unlikely(lock == nullptr)) {
        return;
    }

    kupl_assert(g_tables[lock->type].type == lock->type);
    g_tables[lock->type].fini(lock);
}
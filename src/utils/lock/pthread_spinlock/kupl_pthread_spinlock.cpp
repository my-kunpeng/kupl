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
#include "kupl_pthread_spinlock.h"
#include <pthread.h>
#include "utils/lock/kupl_lock.h"
#include "utils/debug/kupl_assert.h"
#include "utils/sys/kupl_glibc_version.h"

static void kupl_pthread_spinlock_lock(kupl_lock_t *lock)
{
    kupl_assert(lock != nullptr);
    pthread_spinlock_t *raw = (pthread_spinlock_t *)lock->vol_raw_lock;

    pthread_spin_lock(raw);
}

static void kupl_pthread_spinlock_unlock(kupl_lock_t *lock)
{
    kupl_assert(lock != nullptr);
    pthread_spinlock_t *raw = (pthread_spinlock_t *)lock->vol_raw_lock;

    pthread_spin_unlock(raw);
}

static int kupl_pthread_spinlock_trylock(kupl_lock_t *lock)
{
    kupl_assert(lock != nullptr);
    pthread_spinlock_t *raw = (pthread_spinlock_t *)lock->vol_raw_lock;

    return pthread_spin_trylock(raw) == 0;
}

kupl_lock_t *kupl_pthread_spinlock_init()
{
    auto lock = (kupl_lock_t *)kupl_calloc(1, sizeof(kupl_lock_t) + sizeof(pthread_spinlock_t));
    if (kupl_unlikely(lock == nullptr)) {
        return nullptr;
    }

    auto raw = (pthread_spinlock_t *)(lock + 1);

    int ret = pthread_spin_init(raw, 0);
    if (ret != 0) {
        kupl_free_inner(lock);
        return nullptr;
    }
    lock->type = PTHREAD_SPINLOCK;
    lock->vol_raw_lock = raw;
    lock->lock = kupl_pthread_spinlock_lock;
    lock->unlock = kupl_pthread_spinlock_unlock;
    lock->trylock = kupl_pthread_spinlock_trylock;

    return lock;
}

void kupl_pthread_spinlock_fini(kupl_lock_t *lock)
{
    kupl_assert(lock != nullptr);
    pthread_spin_destroy((pthread_spinlock_t *)lock->vol_raw_lock);
    kupl_free_inner(lock);
}
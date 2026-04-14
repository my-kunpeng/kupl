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
#include <omp.h>
#include <unistd.h>
#include "gtest/gtest.h"
#include "kupl.h"
#include "utils/lock/kupl_lock.h"

const static int LOOP_COUNT = 1000;
const static int NUM_THREADS = 2;
static int g_count = 0;

static inline void lock_task(kupl_lock_t *lock, int *cnt)
{
    ASSERT_TRUE(lock != nullptr);
    for (int i = 0; i < LOOP_COUNT; i++) {
        lock->lock(lock);
        *cnt += 1;
        lock->unlock(lock);
    }
}

static inline void trylock_task(kupl_lock_t *lock, int *cnt)
{
    ASSERT_TRUE(lock != nullptr);
    for (int i = 0; i < LOOP_COUNT; i++) {
        while (lock->trylock(lock) == 0) {}
        *cnt += 1;
        lock->unlock(lock);
    }
}

TEST(test_utils_lock_inner, pthread_spinlock)
{
    kupl_lock_t *lock = kupl_lock_create(PTHREAD_SPINLOCK);
    ASSERT_TRUE(lock != nullptr);

    g_count = 0;
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        lock_task(lock, &g_count);
    }
    EXPECT_EQ(g_count, LOOP_COUNT * NUM_THREADS);

    g_count = 0;
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        trylock_task(lock, &g_count);
    }
    EXPECT_EQ(g_count, LOOP_COUNT * NUM_THREADS);

    kupl_lock_cleanup(lock);
    kupl_lock_cleanup(nullptr);    // check the err input
}

TEST(test_utils_lock_inner, ticket_array_lock)
{
    kupl_lock_t *lock = kupl_lock_create(TICKET_ARRAY_LOCK);
    ASSERT_TRUE(lock != nullptr);

    g_count = 0;
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        lock_task(lock, &g_count);
    }
    EXPECT_EQ(g_count, LOOP_COUNT * NUM_THREADS);

    g_count = 0;
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        trylock_task(lock, &g_count);
    }
    EXPECT_EQ(g_count, LOOP_COUNT * NUM_THREADS);

    kupl_lock_cleanup(lock);
}
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
#include <unistd.h>
#include "gtest/gtest.h"
#include "kupl.h"
#include "utils/type/kupl_status.h"
#include "utils/lock/kupl_lock.h"

const static int LOOP_COUNT = 1000;
const static int NUM_THREADS = 2;

static kupl_lock_t *lock = nullptr;
static int g_count = 0;

class test_utils_lock_inner : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        KUPL_1D_RANGE_INIT(range_, 0, NUM_THREADS);
        int exe[2] = {0, 1};
        eg_ = kupl_egroup_create(exe, NUM_THREADS);
        desc_.field_mask = KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT;
        desc_.range = &range_;
        desc_.concurrency = NUM_THREADS;
        desc_.egroup = eg_;
        desc_.policy = KUPL_LOOP_POLICY_STATIC;
    }
    static void TearDownTestCase()
    {
        kupl_egroup_destroy(eg_);
    }
    virtual void SetUp() {}
    virtual void TearDown() {}
    static kupl_nd_range_t range_;
    static kupl_egroup_h eg_;
    static kupl_parallel_for_desc_t desc_;
};

static inline void lock_task(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
    ASSERT_TRUE(lock != nullptr);
    int *cnt = (int *)args;
    for (int i = 0; i < LOOP_COUNT; i++) {
        lock->lock(lock);
        *cnt += 1;
        lock->unlock(lock);
    }
}

static inline void trylock_task(kupl_nd_range_t *nd_range, void *args, int tid, int tnum)
{
    ASSERT_TRUE(lock != nullptr);
    int *cnt = (int *)args;
    for (int i = 0; i < LOOP_COUNT; i++) {
        while (lock->trylock(lock) == 0) {}
        *cnt += 1;
        lock->unlock(lock);
    }
}

kupl_nd_range_t test_utils_lock_inner::range_;
kupl_egroup_h test_utils_lock_inner::eg_;
kupl_parallel_for_desc_t test_utils_lock_inner::desc_;

TEST_F(test_utils_lock_inner, pthread_spinlock)
{
    lock = kupl_lock_create(PTHREAD_SPINLOCK);
    ASSERT_TRUE(lock != nullptr);

    g_count = 0;
    kupl_parallel_for(&desc_, lock_task, &g_count);
    EXPECT_EQ(g_count, LOOP_COUNT * NUM_THREADS);

    g_count = 0;
    kupl_parallel_for(&desc_, trylock_task, &g_count);
    EXPECT_EQ(g_count, LOOP_COUNT * NUM_THREADS);

    kupl_lock_cleanup(lock);
    lock = nullptr;
}

TEST_F(test_utils_lock_inner, ticket_array_lock)
{
    lock = kupl_lock_create(TICKET_ARRAY_LOCK);
    ASSERT_TRUE(lock != nullptr);

    g_count = 0;
    kupl_parallel_for(&desc_, lock_task, &g_count);
    EXPECT_EQ(g_count, LOOP_COUNT * NUM_THREADS);

    g_count = 0;
    kupl_parallel_for(&desc_, trylock_task, &g_count);
    EXPECT_EQ(g_count, LOOP_COUNT * NUM_THREADS);

    kupl_lock_cleanup(lock);
    lock = nullptr;
}
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
#include "gtest/gtest.h"
#include "kupl.h"

static void task_inc(void *args)
{
    int *count = (int *)args;
    *count = (*count) + 1;
}

static void task_void(void *args)
{
}

static void task_delay(void *args)
{
    sleep(1);
}

class test_queue_event : public testing::Test {
public:
    static void SetUpTestCase()
    {
    }
    static void TearDownTestCase()
    {
    }
    void SetUp()
    {
        q1_ = kupl_queue_create();
        q2_ = kupl_queue_create();
        e_ = kupl_event_create();
    }
    void TearDown()
    {
        kupl_event_destroy(e_);
        e_ = nullptr;
        kupl_queue_destroy(q2_);
        q2_ = nullptr;
        kupl_queue_destroy(q1_);
        q1_ = nullptr;
    }
    kupl_queue_h q1_ = nullptr;
    kupl_queue_h q2_ = nullptr;
    kupl_event_h e_ = nullptr;
};

TEST_F(test_queue_event, kupl_queue_create_destroy)
{
    auto queue = kupl_queue_create();
    ASSERT_TRUE(queue != nullptr);
    kupl_queue_destroy(queue);

    // queue destroy invalid
    kupl_queue_destroy(nullptr);
}

TEST_F(test_queue_event, kupl_queue_wait)
{
    int ret = kupl_queue_wait(q1_);
    ASSERT_TRUE(ret == KUPL_OK);

    // queue wait invalid
    ret = kupl_queue_wait(nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_queue_event, kupl_queue_wait_event)
{
    // KUPL_EVENT_STATUS_CREATED
    int ret = kupl_event_query(e_);
    ASSERT_TRUE(ret == KUPL_EVENT_STATUS_CREATED);
    ret = kupl_queue_wait_event(q1_, e_);
    ASSERT_TRUE(ret == KUPL_ERROR);

    // KUPL_EVENT_STATUS_SUBMITTED
    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
        .func = task_delay,
        .args = nullptr,
        .name = "delay",
    };
    ret = kupl_queue_submit(q1_, &desc);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_event_record(e_, q1_);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_event_query(e_);
    ASSERT_TRUE(ret == KUPL_EVENT_STATUS_SUBMITTED);
    ret = kupl_queue_wait_event(q1_, e_);
    ASSERT_TRUE(ret == KUPL_OK);

    // KUPL_EVENT_STATUS_COMPLETE
    ret = kupl_event_query(e_);
    ASSERT_TRUE(ret == KUPL_EVENT_STATUS_COMPLETE);
    ret = kupl_queue_wait_event(q1_, e_);
    ASSERT_TRUE(ret == KUPL_OK);

    // queue wait invalid
    ret = kupl_queue_wait_event(nullptr, e_);
    ASSERT_TRUE(ret == KUPL_ERROR);

    ret = kupl_queue_wait_event(q1_, nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);

    ret = kupl_queue_wait_event(nullptr, nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_queue_event, kupl_queue_submit)
{
    int value = 0;
    // desc.func == nullptr
    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
        .func = nullptr,
        .args = &value,
        .name = "inc",
    };
    int ret = kupl_queue_submit(q1_, &desc);
    ASSERT_TRUE(ret == KUPL_ERROR);

    // desc.name == nullptr
    desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
        .func = task_inc,
        .args = &value,
        .name = nullptr,
    };
    ret = kupl_queue_submit(q1_, &desc);
    ASSERT_TRUE(ret == KUPL_ERROR);

    // desc.args == nullptr
    desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
        .func = task_void,
        .args = nullptr,
        .name = "void",
    };
    ret = kupl_queue_submit(q1_, &desc);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_queue_wait(q1_);
    ASSERT_TRUE(ret == KUPL_OK);

    // valid queue_submit
    desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
        .func = task_inc,
        .args = &value,
        .name = "inc"
    };
    ret = kupl_queue_submit(q1_, &desc);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_queue_wait(q1_);
    ASSERT_TRUE(ret == KUPL_OK);
    ASSERT_TRUE(value == 1);

    // invalid queue_submit
    ret = kupl_queue_submit(nullptr, &desc);
    ASSERT_TRUE(ret == KUPL_ERROR);

    ret = kupl_queue_submit(q1_, nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_queue_event, kupl_event_create_destroy)
{
    auto event = kupl_event_create();
    ASSERT_TRUE(event != nullptr);
    kupl_event_destroy(event);

    // event destroy invalid
    kupl_event_destroy(nullptr);
}

TEST_F(test_queue_event, kupl_event_record)
{
    // KUPL_EVENT_STATUS_CREATED
    int ret = kupl_event_query(e_);
    ASSERT_TRUE(ret == KUPL_EVENT_STATUS_CREATED);
    ret = kupl_event_record(e_, q1_);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_event_wait(e_);
    ASSERT_TRUE(ret == KUPL_OK);

    // KUPL_EVENT_STATUS_COMPLETE
    ret = kupl_event_query(e_);
    ASSERT_TRUE(ret == KUPL_EVENT_STATUS_COMPLETE);
    ret = kupl_event_record(e_, q1_);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_event_wait(e_);
    ASSERT_TRUE(ret == KUPL_OK);

    // KUPL_EVENT_STATUS_SUBMITTED
    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
        .func = task_delay,
        .args = nullptr,
        .name = "delay",
    };
    ret = kupl_queue_submit(q1_, &desc);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_event_record(e_, q1_);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_event_query(e_);
    ASSERT_TRUE(ret == KUPL_EVENT_STATUS_SUBMITTED);
    ret = kupl_event_record(e_, q1_);
    ASSERT_TRUE(ret == KUPL_ERROR);
    ret = kupl_event_wait(e_);
    ASSERT_TRUE(ret == KUPL_OK);

    // event record invalid
    ret = kupl_event_record(e_, nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);

    ret = kupl_event_record(nullptr, q1_);
    ASSERT_TRUE(ret == KUPL_ERROR);

    ret = kupl_event_record(nullptr, nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_queue_event, kupl_event_wait)
{
    // KUPL_EVENT_STATUS_CREATED
    int ret = kupl_event_query(e_);
    ASSERT_TRUE(ret == KUPL_EVENT_STATUS_CREATED);
    ret = kupl_event_wait(e_);
    ASSERT_TRUE(ret == KUPL_ERROR);

    // KUPL_EVENT_STATUS_SUBMITTED
    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
        .func = task_delay,
        .args = nullptr,
        .name = "delay",
    };
    ret = kupl_queue_submit(q1_, &desc);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_event_record(e_, q1_);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_event_query(e_);
    ASSERT_TRUE(ret == KUPL_EVENT_STATUS_SUBMITTED);
    ret = kupl_event_wait(e_);
    ASSERT_TRUE(ret == KUPL_OK);

    // KUPL_EVENT_STATUS_COMPLETE
    ret = kupl_event_query(e_);
    ASSERT_TRUE(ret == KUPL_EVENT_STATUS_COMPLETE);
    ret = kupl_event_wait(e_);
    ASSERT_TRUE(ret == KUPL_OK);

    // event wait invalid
    ret = kupl_event_wait(nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_queue_event, kupl_event_query)
{
    int ret = kupl_event_query(nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_queue_event, kupl_event_record_twice)
{
    int ret = kupl_event_record(e_, q1_);
    ASSERT_TRUE(ret == KUPL_OK);

    ret = kupl_event_record(e_, q1_);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_queue_event, kupl_event_wait_twice)
{
    int ret = kupl_event_record(e_, q1_);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_event_wait(e_);
    ASSERT_TRUE(ret == KUPL_OK);

    ret = kupl_event_record(e_, q2_);
    ASSERT_TRUE(ret == KUPL_OK);
    ret = kupl_event_wait(e_);
    ASSERT_TRUE(ret == KUPL_OK);
}

TEST_F(test_queue_event, kupl_queue_acquire_destroy)
{
    auto queue1 = kupl_queue_acquire(0);
    ASSERT_TRUE(queue1 != nullptr);
    auto queue2 = kupl_queue_acquire(1);
    ASSERT_TRUE(queue2 != nullptr);
    auto queue3 = kupl_queue_acquire(0);
    ASSERT_TRUE(queue3 == queue1);
    kupl_queue_destroy(queue1);
    kupl_queue_destroy(queue2);
}

TEST_F(test_queue_event, kupl_queue_wait_all)
{
    auto queue1 = kupl_queue_acquire(0);
    ASSERT_TRUE(queue1 != nullptr);

    auto queue2 = kupl_queue_acquire(1);
    ASSERT_TRUE(queue2 != nullptr);

    int ret = kupl_queue_wait_all();
    ASSERT_TRUE(ret == KUPL_OK);
}
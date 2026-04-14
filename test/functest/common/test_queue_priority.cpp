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
#include <omp.h>

static size_t len = 1024;

static void task_priority(void *args)
{
    int count = *(int*)args;
    sleep(1);
    printf("priority: %d \n", count);
}

static void task_prt(void *args)
{
    printf("task prt\n");
}

static void task_src_set(void *args)
{
    char *src = (char *)args;
    for (size_t i = 0; i < len / sizeof(char); i++) {
        src[i] = (char)(i+1);
    }
}

static void task_mid_set(void *args)
{
    char *mid = (char *)args;
    for (size_t i = 0; i < len / sizeof(char); i++) {
        mid[i] = mid[i] + 1;
    }
}

static void task_dst_check(void *args)
{
    char *dst = (char *)args;
    for (size_t i = 0; i < len / sizeof(char); i++) {
        if (dst[i] != (char)(i+1)) {
            printf("dst check error\n");
        }
    }
    printf("dst check ok\n");
}

static bool is_dst_eq_src(char *dst, char *src, size_t count)
{
    for (size_t i = 0; i < count / sizeof(char); i++) {
        if (dst[i] != src[i]) {
            return false;
        }
    }
    return true;
}

TEST(test_queue_priority, queue_priority_range)
{
    int least_priority, greatest_priority;
    int ret = kupl_get_queue_priority_range(&least_priority, &greatest_priority);
    ASSERT_TRUE(ret == KUPL_OK);
    printf("kupl queue priority range: [%d, %d]\n", least_priority, greatest_priority);
    ret = kupl_get_queue_priority_range(nullptr, nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST(test_queue_priority, task_priority)
{
    int* q_priority[4];
    for (int i = 0; i < 4; i++) {
        q_priority[i] = (int*)malloc(sizeof(int));
        *q_priority[i] = i;
    }

    kupl_queue_h queue[4];
    kupl_queue_item_desc_t desc[4];
    for (int i = 0; i < 4; i++) {
        queue[i] = kupl_queue_create_with_priority(i);
        desc[i] = {
            .func = task_priority,
            .args = q_priority[i],
            .name = "task_priority"
        };
    }

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            kupl_queue_submit(queue[j], &desc[j]);
        }
    }
    for (int i = 0; i < 4; i++) {
        kupl_queue_wait(queue[i]);
    }

    for (int i = 0; i < 4; i++) {
        free(q_priority[i]);
        kupl_queue_destroy(queue[i]);
    }
}

TEST(test_queue_priority, queue_memcpy_compute_task)
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    for (int i=0; i < len / sizeof(char); i++) {
        src[i] = (char)(i+1);
        dst[i] = 0;
    }

    kupl_queue_h queue;
    kupl_event_h event;
    queue = kupl_queue_create_with_priority(0);
    event = kupl_event_create();

    kupl_memcpy_async(dst, src, len, queue, event);

    kupl_queue_item_desc_t desc = {
        .func = task_dst_check,
        .args = dst,
        .name = "task_dst_check"
    };
    kupl_queue_submit(queue, &desc);
    int ret = kupl_queue_wait(queue);
    ASSERT_TRUE(ret == KUPL_OK);

    kupl_event_destroy(event);
    kupl_queue_destroy(queue);

    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}

TEST(test_queue_priority, queue_compute_memcpy_task)
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    for (int i=0; i < len / sizeof(char); i++) {
        src[i] = 0;
        dst[i] = 0;
    }

    kupl_queue_h queue;
    kupl_event_h event;
    queue = kupl_queue_create_with_priority(0);
    event = kupl_event_create();

    kupl_queue_item_desc_t desc = {
        .func = task_src_set,
        .args = src,
        .name = "task_src_set"
    };
    kupl_queue_submit(queue, &desc);
    kupl_memcpy_async(dst, src, len, queue, event);

    int ret = kupl_queue_wait(queue);
    ASSERT_TRUE(ret == KUPL_OK);

    task_dst_check(dst);

    kupl_event_destroy(event);
    kupl_queue_destroy(queue);

    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}

TEST(test_queue_priority, queue_memcpy_async_order)
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    char *mid = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    for (int i=0; i < len / sizeof(char); i++) {
        src[i] = (char)(i+1);
        mid[i] = 0;
        dst[i] = 0;
    }

    kupl_queue_h queue;
    kupl_event_h event[2];
    queue = kupl_queue_create_with_priority(0);

    for (int i = 0; i < 2; i++) {
        event[i] = kupl_event_create();
    }

    kupl_memcpy_async(mid, src, len, queue, event[0]);
    kupl_memcpy_async(dst, mid, len, queue, event[1]);

    kupl_queue_wait(queue);

    bool res = is_dst_eq_src(dst, src, len);
    ASSERT_TRUE(res);

    for (int i = 0; i < 2; i++) {
        kupl_event_destroy(event[i]);
    }
    kupl_queue_destroy(queue);

    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, mid);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}

TEST(test_queue_priority, queue_compute_memcpy_memcpy_task)
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    char *mid = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    for (int i=0; i < len / sizeof(char); i++) {
        src[i] = 0;
        mid[i] = 0;
        dst[i] = 0;
    }

    kupl_queue_h queue;
    kupl_event_h event[2];
    queue = kupl_queue_create_with_priority(0);

    for (int i = 0; i < 2; i++) {
        event[i] = kupl_event_create();
    }

    kupl_queue_item_desc_t desc = {
        .func = task_src_set,
        .args = src,
        .name = "task_src_set"
    };
    kupl_queue_submit(queue, &desc);

    kupl_memcpy_async(mid, src, len, queue, event[0]);
    kupl_memcpy_async(dst, mid, len, queue, event[1]);

    int ret = kupl_queue_wait(queue);
    ASSERT_TRUE(ret == KUPL_OK);
    task_dst_check(dst);

    for (int i = 0; i < 2; i++) {
        kupl_event_destroy(event[i]);
    }
    kupl_queue_destroy(queue);

    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, mid);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}

TEST(test_queue_priority, queue_memcpy_compute_memcpy_task)
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    char *mid = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    for (int i=0; i < len / sizeof(char); i++) {
        src[i] = (char)(i);
        mid[i] = 0;
        dst[i] = 0;
    }

    kupl_queue_h queue;
    kupl_event_h event[2];
    queue = kupl_queue_create_with_priority(0);

    for (int i = 0; i < 2; i++) {
        event[i] = kupl_event_create();
    }

    kupl_memcpy_async(mid, src, len, queue, event[0]);


    kupl_queue_item_desc_t desc = {
        .func = task_mid_set,
        .args = mid,
        .name = "task_mid_set"
    };
    kupl_queue_submit(queue, &desc);

    kupl_memcpy_async(dst, mid, len, queue, event[1]);
    int ret = kupl_queue_wait(queue);
    ASSERT_TRUE(ret == KUPL_OK);
    task_dst_check(dst);

    for (int i = 0; i < 2; i++) {
        kupl_event_destroy(event[i]);
    }
    kupl_queue_destroy(queue);

    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, mid);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}

TEST(test_queue_priority, queue_memcpy_memcpy_compute_task)
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    char *mid = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    for (int i=0; i < len / sizeof(char); i++) {
        src[i] = (char)(i+1);
        mid[i] = 0;
        dst[i] = 0;
    }

    kupl_queue_h queue;
    kupl_event_h event[2];
    queue = kupl_queue_create_with_priority(0);

    for (int i = 0; i < 2; i++) {
        event[i] = kupl_event_create();
    }

    kupl_memcpy_async(mid, src, len, queue, event[0]);
    kupl_memcpy_async(dst, mid, len, queue, event[1]);

    kupl_queue_item_desc_t desc = {
        .func = task_dst_check,
        .args = dst,
        .name = "task_dst_check"
    };
    kupl_queue_submit(queue, &desc);
    int ret = kupl_queue_wait(queue);
    ASSERT_TRUE(ret == KUPL_OK);

    for (int i = 0; i < 2; i++) {
        kupl_event_destroy(event[i]);
    }
    kupl_queue_destroy(queue);

    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, mid);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}

TEST(test_queue_priority, memcpy_priority)
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, len);
    for (int i=0; i < len / sizeof(char); i++) {
        src[i] = (char)(i+1);
        dst[i] = 0;
    }

    kupl_queue_h queue[4];
    kupl_event_h event[4];
    kupl_queue_item_desc_t desc[4];
    for (int i = 0; i < 4; i++) {
        queue[i] = kupl_queue_create_with_priority(i);
        event[i] = kupl_event_create();
    }

    for (int i = 0; i < 4; i++) {
        kupl_memcpy_async(dst, src, len, queue[i], event[i]);
    }

    for (int i = 0; i < 4; i++) {
        kupl_event_wait(event[i]);
    }

    bool res = is_dst_eq_src(dst, src, len);
    ASSERT_TRUE(res);

    for (int i = 0; i < 4; i++) {
        kupl_queue_destroy(queue[i]);
        kupl_event_destroy(event[i]);
    }

    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}
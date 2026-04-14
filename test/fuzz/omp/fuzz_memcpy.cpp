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
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <exception>
#include "common/fuzz_common.h"

static const int MEMCPY_THREADS_MIN = 0;
static const int MEMCPY_THREADS_MAX = 10240;
static const int MEMCPY_THREADS_LOCAL_MIN = 0;
static const int MEMCPY_THREADS_LOCAL_MAX = 10240;
static const int MLOCK_COUNTS_MIN = 0;
static const int MLOCK_COUNTS_MAX = 5 * 1024 * 1024;
static const int MEMCPY1D_COUNTS_MIN = 0;
static const int MEMCPY1D_COUNTS_MAX = 5 * 1024 * 1024;
static const int MEMCPY2D_HEIGHT_MIN = 0;
static const int MEMCPY2D_HEIGHT_MAX = 5 * 1024;
static const int MEMCPY2D_WIDTH_MIN = 0;
static const int MEMCPY2D_WIDTH_MAX = 5 * 1024;
static const int MEMCPY2D_SPITCH_MIN = 0;
static const int MEMCPY2D_SPITCH_MAX = 5 * 1024;
static const int MEMCPY2D_DPITCH_MIN = 0;
static const int MEMCPY2D_DPITCH_MAX = 5 * 1024;

void mlock_example(int test_count)
{
    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        int sz = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MLOCK_COUNTS_MIN,
                                             MLOCK_COUNTS_MIN, MLOCK_COUNTS_MAX);
        char *buffer = (char*)malloc(sizeof(char) * sz);
        if (buffer == nullptr) {
            std::terminate();
        }
        kupl_mlock(buffer, sz);
        kupl_munlock(buffer, sz);

        free(buffer);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}

void kupl_memcpy_coverage()
{
    kupl_get_kernel_concurrency();
    kupl_get_kernel_concurrency_local();
    kupl_set_kernel_concurrency(1);
    kupl_set_kernel_concurrency_local(1);
    kupl_get_kernel_concurrency();
    kupl_get_kernel_concurrency_local();
}

void memcpy1d_example(int test_count)
{
    printf("start -- %s\n", __func__);
    kupl_memcpy_coverage();
    char *src = (char*)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * MEMCPY1D_COUNTS_MAX);
    char *dst = (char*)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * MEMCPY1D_COUNTS_MAX);
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        int threads = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY_THREADS_MIN,
                                                  MEMCPY_THREADS_MIN, MEMCPY_THREADS_MAX);
        kupl_set_kernel_concurrency(threads);
        int threads_local = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY_THREADS_LOCAL_MIN,
                                                        MEMCPY_THREADS_LOCAL_MIN, MEMCPY_THREADS_LOCAL_MAX);
        kupl_set_kernel_concurrency_local(threads_local);
        int sz = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY1D_COUNTS_MIN,
                                             MEMCPY1D_COUNTS_MIN, MEMCPY1D_COUNTS_MAX);
        int ret = kupl_memcpy(dst, src, sizeof(char) * sz);
        if (ret != KUPL_OK) {
            std::terminate();
        }
    }
    DT_FUZZ_END();
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
    printf("end -- %s\n", __func__);
}

void* memcpy_async_task1(void* arg)
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * 1024);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * 1024);
    kupl_event_h event = kupl_event_create();
    int ret = kupl_memcpy_async(dst, src, sizeof(char) * 1024, nullptr, event);
    if (ret == KUPL_OK) {
        int status = kupl_event_query(event);
        while (status != KUPL_EVENT_STATUS_COMPLETE) {
            status = kupl_event_query(event);
        }
    }
    kupl_event_destroy(event);
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
    return NULL;
}

void* memcpy_async_task2(void* arg)
{
    char *src = (char *)malloc(sizeof(char) * 1024);
    char *dst = (char *)malloc(sizeof(char) * 1024);
    kupl_event_h event = kupl_event_create();
    kupl_queue_h queue = kupl_queue_create();

    int ret = kupl_memcpy_async(dst, src, sizeof(char) * 1024, queue, event);
    if (ret == KUPL_OK) {
        kupl_event_wait(event);
    }

    kupl_event_destroy(event);
    kupl_queue_destroy(queue);
    free(src);
    free(dst);
    return NULL;
}

kupl_event_h event;
kupl_queue_h queue;

void* memcpy_async_task3(void* arg)
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * 1024);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * 1024);
    int ret = kupl_memcpy_async(dst, src, sizeof(char) * 1024, nullptr, event);
    if (ret == KUPL_OK) {
        int status = kupl_event_query(event);
        while (status != KUPL_EVENT_STATUS_COMPLETE) {
            status = kupl_event_query(event);
        }
    }
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
    return NULL;
}

void kupl_memcpy_async_user_created_thread()
{
    event = kupl_event_create();
    pthread_t thread1;
    pthread_create(&thread1, NULL, memcpy_async_task1, NULL);
    pthread_t thread2;
    pthread_create(&thread2, NULL, memcpy_async_task2, NULL);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    pthread_t thread3;
    pthread_create(&thread3, NULL, memcpy_async_task3, NULL);
    pthread_join(thread3, NULL);
    kupl_event_destroy(event);
}

void kupl_memcpy_async_coverage()
{
    kupl_memcpy_async_user_created_thread();
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * MEMCPY1D_COUNTS_MAX);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * MEMCPY1D_COUNTS_MAX);
    kupl_event_h event = kupl_event_create();
    int ret = kupl_memcpy_async(dst, src, sizeof(char) * MEMCPY1D_COUNTS_MAX, nullptr, event);
    if (ret == KUPL_OK) {
        int res = kupl_event_query(event);
        while (res != KUPL_EVENT_STATUS_COMPLETE) {
            res = kupl_event_query(event);
        }
    }
    kupl_event_destroy(event);
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}

void memcpy1d_async_example(int test_count)
{
    printf("start -- %s\n", __func__);
    kupl_memcpy_async_coverage();
    char *src = (char*)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * MEMCPY1D_COUNTS_MAX);
    char *dst = (char*)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * MEMCPY1D_COUNTS_MAX);
    kupl_event_h event = kupl_event_create();
    kupl_queue_h queue = kupl_queue_create();
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        int sz = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY1D_COUNTS_MIN,
                                             MEMCPY1D_COUNTS_MIN, MEMCPY1D_COUNTS_MAX);
        int ret = kupl_memcpy_async(dst, src, sizeof(char) * sz, queue, event);
        if (ret != KUPL_OK) {
            std::terminate();
        }
        kupl_event_wait(event);
    }
    DT_FUZZ_END();
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
    kupl_event_destroy(event);
    kupl_queue_destroy(queue);
    printf("end -- %s\n", __func__);
}

void memcpy2d_example(int test_count)
{
    printf("start -- %s\n", __func__);
    char *src = (char*)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * (size_t)MEMCPY2D_HEIGHT_MAX * MEMCPY2D_SPITCH_MAX);
    char *dst = (char*)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * (size_t)MEMCPY2D_HEIGHT_MAX * MEMCPY2D_DPITCH_MAX);
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        int threads = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY_THREADS_MIN,
                                                  MEMCPY_THREADS_MIN, MEMCPY_THREADS_MAX);
        kupl_set_kernel_concurrency(threads);
        int threads_local = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY_THREADS_LOCAL_MIN,
                                                        MEMCPY_THREADS_LOCAL_MIN, MEMCPY_THREADS_LOCAL_MAX);
        kupl_set_kernel_concurrency_local(threads_local);
        int height = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY2D_HEIGHT_MIN,
                                                 MEMCPY2D_HEIGHT_MIN, MEMCPY2D_HEIGHT_MAX);
        int width = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY2D_WIDTH_MIN,
                                                MEMCPY2D_WIDTH_MIN, MEMCPY2D_WIDTH_MAX);
        int spitch = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY2D_SPITCH_MIN,
                                                 MEMCPY2D_SPITCH_MIN, MEMCPY2D_SPITCH_MAX);
        int dpitch = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY2D_DPITCH_MIN,
                                                 MEMCPY2D_DPITCH_MIN, MEMCPY2D_DPITCH_MAX);
        if (spitch < width || dpitch < width) {
            int ret = kupl_memcpy2d(dst, sizeof(char) * dpitch, src, sizeof(char) * spitch,
                sizeof(char) * width, height);
            if (ret != KUPL_ERROR) {
                std::terminate();
            }
        } else {
            int ret = kupl_memcpy2d(dst, sizeof(char) * dpitch, src, sizeof(char) * spitch,
                sizeof(char) * width, height);
            if (ret != KUPL_OK) {
                std::terminate();
            }
        }
    }
    DT_FUZZ_END();
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
    printf("end -- %s\n", __func__);
}

void kupl_memcpy2d_async_coverage()
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * (size_t)MEMCPY2D_HEIGHT_MAX * MEMCPY2D_SPITCH_MAX);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * (size_t)MEMCPY2D_HEIGHT_MAX * MEMCPY2D_DPITCH_MAX);
    kupl_event_h event = kupl_event_create();
    int ret = kupl_memcpy2d_async(dst, sizeof(char) * MEMCPY2D_DPITCH_MAX, src,
                                  sizeof(char) * MEMCPY2D_SPITCH_MAX, sizeof(char) * MEMCPY2D_WIDTH_MAX,
                                  MEMCPY2D_HEIGHT_MAX, nullptr, event);
    if (ret == KUPL_OK) {
        kupl_event_wait(event);
    }
    kupl_event_destroy(event);
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}

void memcpy2d_async_example(int test_count)
{
    printf("start -- %s\n", __func__);
    kupl_memcpy2d_async_coverage();
    char *src = (char*)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * (size_t)MEMCPY2D_HEIGHT_MAX * MEMCPY2D_SPITCH_MAX);
    char *dst = (char*)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * (size_t)MEMCPY2D_HEIGHT_MAX * MEMCPY2D_DPITCH_MAX);
    kupl_event_h event = kupl_event_create();
    kupl_queue_h queue = kupl_queue_create();
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        int height = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY2D_HEIGHT_MIN,
                                                 MEMCPY2D_HEIGHT_MIN, MEMCPY2D_HEIGHT_MAX);
        int width = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY2D_WIDTH_MIN,
                                                MEMCPY2D_WIDTH_MIN, MEMCPY2D_WIDTH_MAX);
        int spitch = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY2D_SPITCH_MIN,
                                                 MEMCPY2D_SPITCH_MIN, MEMCPY2D_SPITCH_MAX);
        int dpitch = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY2D_DPITCH_MIN,
                                                 MEMCPY2D_DPITCH_MIN, MEMCPY2D_DPITCH_MAX);
        if (spitch < width || dpitch < width) {
            int ret = kupl_memcpy2d_async(dst, sizeof(char) * dpitch, src, sizeof(char) * spitch,
                sizeof(char) * width, height, queue, event);
            if (ret != KUPL_ERROR) {
                std::terminate();
            }
        } else {
            int ret = kupl_memcpy2d_async(dst, sizeof(char) * dpitch, src, sizeof(char) * spitch,
                sizeof(char) * width, height, queue, event);
            if (ret != KUPL_OK) {
                std::terminate();
            }
            kupl_event_wait(event);
        }
    }
    DT_FUZZ_END();
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
    kupl_event_destroy(event);
    kupl_queue_destroy(queue);
    printf("end -- %s\n", __func__);
}

static void func_test(void *args)
{
}

void kupl_memcpy_priority_coverage()
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * MEMCPY1D_COUNTS_MAX);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * MEMCPY1D_COUNTS_MAX);
    kupl_event_h event = kupl_event_create();
    kupl_queue_h queue = kupl_queue_create_with_priority(0);
    kupl_memcpy_async(dst, src, sizeof(char) * MEMCPY1D_COUNTS_MAX, queue, event);
    kupl_event_query(event);
    kupl_event_wait(event);
    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME,
        .func = func_test,
        .args = nullptr,
        .name = "func_test"
    };
    kupl_queue_submit(queue, &desc);
    kupl_memcpy_async(dst, src, sizeof(char) * MEMCPY1D_COUNTS_MAX, queue, event);
    kupl_event_query(event);
    kupl_event_wait(event);
    kupl_queue_wait(queue);
    kupl_event_destroy(event);
    kupl_queue_destroy(queue);
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}

void memcpy_priority_example(int test_count)
{
    printf("start -- %s\n", __func__);
    kupl_memcpy_priority_coverage();
    char *src = (char*)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * MEMCPY1D_COUNTS_MAX);
    char *dst = (char*)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * MEMCPY1D_COUNTS_MAX);
    kupl_event_h event1 = kupl_event_create();
    kupl_event_h event2 = kupl_event_create();
    int least_priority, greatest_priority;
    kupl_get_queue_priority_range(&least_priority, &greatest_priority);
    auto queue1 = kupl_queue_create_with_priority(least_priority);
    auto queue2 = kupl_queue_create_with_priority(greatest_priority);
    DT_Enable_Leak_Check(0, 0);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        int cnt = 0;
        int sz = *(int*)DT_SetGetNumberRange(&g_Element[cnt++], MEMCPY1D_COUNTS_MIN,
                                             MEMCPY1D_COUNTS_MIN, MEMCPY1D_COUNTS_MAX);
        int ret = kupl_memcpy_async(dst, src, sizeof(char) * sz, queue1, event1);
        if (ret != KUPL_OK) {
            std::terminate();
        }
        ret = kupl_memcpy_async(dst, src, sizeof(char) * sz, queue2, event2);
        if (ret != KUPL_OK) {
            std::terminate();
        }
        kupl_event_wait(event1);
        kupl_event_wait(event2);
    }
    DT_FUZZ_END();
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
    kupl_event_destroy(event1);
    kupl_event_destroy(event2);
    kupl_queue_destroy(queue1);
    kupl_queue_destroy(queue2);
    printf("end -- %s\n", __func__);
}
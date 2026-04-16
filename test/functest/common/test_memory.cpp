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

TEST(test_memory, kupl_kernel_concurrency)
{
    int num_executors = kupl_get_num_executors();
    kupl_set_kernel_concurrency(num_executors);
    ASSERT_EQ(kupl_get_kernel_concurrency(), num_executors);
}

TEST(test_memory, kupl_kernel_concurrency_local)
{
    int num_executors = kupl_get_num_executors();
    kupl_set_kernel_concurrency_local(num_executors);
    ASSERT_EQ(kupl_get_kernel_concurrency_local(), num_executors);
}

void kupl_mlock_test(size_t buffer_size, size_t lock_size)
{
    void *buffer = malloc(buffer_size);
    ASSERT_TRUE(buffer != nullptr);
    kupl_mlock(buffer, lock_size);
    kupl_munlock(buffer, lock_size);
    free(buffer);
}

TEST(test_memory, kupl_mlock)
{
    kupl_mlock_test(0, 0);
    kupl_mlock_test(1024, 1024);
    kupl_mlock_test(1024, 512);

    kupl_mlock(nullptr, 0);
    kupl_munlock(nullptr, 0);
}

void kupl_malloc_test(size_t buffer_size)
{
    void *buffer = kupl_malloc(KUPL_MEM_LARGE_CAP, buffer_size);
    ASSERT_TRUE(buffer != nullptr);
    kupl_free(KUPL_MEM_LARGE_CAP, buffer);
    if (kupl_hbw_check_available() && buffer_size < 4 * 1024 * 1024) {
        void *hbw_buffer = kupl_malloc(KUPL_MEM_HIGH_BW, buffer_size);
        ASSERT_TRUE(hbw_buffer != nullptr);
        kupl_free(KUPL_MEM_HIGH_BW, hbw_buffer);
    }
}

TEST(test_memory, kupl_malloc)
{
    void *buffer;
    int i;
    buffer = kupl_malloc(KUPL_MEM_DEFAULT, 0);
    ASSERT_TRUE(buffer == nullptr);
    kupl_free(KUPL_MEM_DEFAULT, buffer);
    kupl_malloc_test(512);
    kupl_malloc_test(1024);
    kupl_malloc_test(2 * 1024);
    kupl_malloc_test(4 * 1024);
    kupl_malloc_test(8 * 1024);
    kupl_malloc_test(16 * 1024);
    kupl_malloc_test(32 * 1024);
    kupl_malloc_test(64 * 1024);
    kupl_malloc_test(128 * 1024);
    kupl_malloc_test(256 * 1024);
    kupl_malloc_test(512 * 1024);
    kupl_malloc_test(1024 * 1024);
    kupl_malloc_test(2 * 1024 * 1024);
    kupl_malloc_test(4 * 1024 * 1024);

    void *buf_arr[5];
    for(i = 0; i < 5; i++) {
        buf_arr[i] = kupl_malloc(KUPL_MEM_DEFAULT, 1024 * 1024);
    }
    for(i = 0; i < 5; i++) {
        ASSERT_TRUE(buf_arr[i] != nullptr);
        kupl_free(KUPL_MEM_DEFAULT, buf_arr[i]);
    }
    kupl_free(KUPL_MEM_DEFAULT, nullptr);
}

void array_1d_init(char *array, size_t sz)
{
    for (size_t i = 0; i < sz; i++) {
        array[i] = (char)i;
    }
}

bool array_1d_check(char *array_src, char *array_dst, size_t sz)
{
    size_t diffnum = 0;
    for (size_t i = 0; i < sz; i++) {
        if (array_src[i] != array_dst[i]) {
            diffnum++;
        }
    }
    return diffnum == 0;
}

void kupl_memcpy_test(size_t sz)
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * sz);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * sz);
    if (src == nullptr || dst == nullptr) {
        return;
    }
    if (sizeof(char) * sz > (1uLL << 32)) {
        int ret = kupl_memcpy(dst, src, sizeof(char) * sz);
        ASSERT_TRUE(ret == KUPL_ERROR);
    } else {
        array_1d_init(src, sz);
        int ret = kupl_memcpy(dst, src, sizeof(char) * sz);
        ASSERT_TRUE(ret == KUPL_OK);
        bool res = array_1d_check(src, dst, sz);
        ASSERT_TRUE(res);
    }
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}

void kupl_memcpy_boundary_test(size_t sz)
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * sz * 2);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * sz * 2);
    array_1d_init(src, sz);
    int ret = kupl_memcpy(dst, src, sizeof(char) * sz);
    ASSERT_TRUE(ret == KUPL_OK);
    bool res = array_1d_check(src, dst, sz);
    ASSERT_TRUE(res);
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}

TEST(test_memory, kupl_memcpy)
{
    kupl_memcpy_test(0);
    kupl_memcpy_test(1);
    kupl_memcpy_test(128);
    kupl_memcpy_test(129);
    kupl_memcpy_test(1024);
    kupl_memcpy_test(1024 + 1);
    kupl_memcpy_test(16 * 1024);
    kupl_memcpy_test(16 * 1024 + 1);
    kupl_memcpy_test(16 * 1024 * 1024);
    kupl_memcpy_test(16 * 1024 * 1024 + 1);
    kupl_memcpy_test(32 * 1024 * 1024);
    kupl_memcpy_test(32 * 1024 * 1024 + 1);
    kupl_memcpy_test(64 * 1024 * 1024);
    kupl_memcpy_test(64 * 1024 * 1024 + 1);
    kupl_memcpy_test((1uLL << 32) + 1);
    kupl_memcpy_boundary_test(1024);
}

void kupl_memcpy_async_test(size_t sz)
{
    char *src = (char *)malloc(sizeof(char) * sz);
    char *dst = (char *)malloc(sizeof(char) * sz);
    kupl_event_h event = kupl_event_create();
    kupl_queue_h queue = kupl_queue_create();
    if (sizeof(char) * sz > (1uLL << 32)) {
        int ret = kupl_memcpy_async(dst, src, sizeof(char) * sz, queue, event);
        ASSERT_TRUE(ret == KUPL_ERROR);
    } else {
        array_1d_init(src, sz);
        int ret = kupl_memcpy_async(dst, src, sizeof(char) * sz, queue, event);
        ASSERT_TRUE(ret == KUPL_OK);
        kupl_event_wait(event);
        bool res = array_1d_check(src, dst, sz);
        ASSERT_TRUE(res);
    }
    kupl_event_destroy(event);
    kupl_queue_destroy(queue);
    free(src);
    free(dst);
}

void kupl_memcpy_async_null_queue_test(size_t sz)
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * sz);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * sz);
    array_1d_init(src, sz);
    kupl_event_h event = kupl_event_create();
    int ret = kupl_memcpy_async(dst, src, sizeof(char) * sz, nullptr, event);
    if (ret == KUPL_OK) {
        int status = kupl_event_query(event);
        while (status != KUPL_EVENT_STATUS_COMPLETE) {
            status = kupl_event_query(event);
        }
        bool res = array_1d_check(src, dst, sz);
        ASSERT_TRUE(res);
    }
    kupl_event_destroy(event);
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}

void kupl_memcpy_async_boundary_test(size_t sz)
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * sz * 2);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * sz * 2);
    kupl_event_h event = kupl_event_create();
    kupl_queue_h queue = kupl_queue_create();
    int ret = kupl_memcpy_async(dst, src, sizeof(char) * sz, queue, nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);

    array_1d_init(src, sz);
    ret = kupl_memcpy_async(dst, src, sizeof(char) * sz, queue, event);
    ASSERT_TRUE(ret == KUPL_OK);
    kupl_event_wait(event);
    kupl_event_destroy(event);
    kupl_queue_destroy(queue);
    bool res = array_1d_check(src, dst, sz);
    ASSERT_TRUE(res);
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}

TEST(test_memory, kupl_memcpy_async)
{
    kupl_memcpy_async_test(0);
    kupl_memcpy_async_test(1);
    kupl_memcpy_async_test(128);
    kupl_memcpy_async_test(129);
    kupl_memcpy_async_test(1024);
    kupl_memcpy_async_test(1024 + 1);
    kupl_memcpy_async_test(16 * 1024);
    kupl_memcpy_async_test(16 * 1024 + 1);
    kupl_memcpy_async_test(16 * 1024 * 1024);
    kupl_memcpy_async_test(16 * 1024 * 1024 + 1);
    kupl_memcpy_async_test(32 * 1024 * 1024);
    kupl_memcpy_async_test(32 * 1024 * 1024 + 1);
    kupl_memcpy_async_test(64 * 1024 * 1024);
    kupl_memcpy_async_test(64 * 1024 * 1024 + 1);
    kupl_memcpy_async_test((1uLL << 32) + 1);
    kupl_memcpy_async_null_queue_test(16 * 1024);
    kupl_memcpy_async_null_queue_test(16 * 1024 + 1);
    kupl_memcpy_async_null_queue_test(16 * 1024 * 1024);
    kupl_memcpy_async_null_queue_test(16 * 1024 * 1024 + 1);
    kupl_memcpy_async_null_queue_test(32 * 1024 * 1024);
    kupl_memcpy_async_null_queue_test(32 * 1024 * 1024 + 1);
    kupl_memcpy_async_null_queue_test(64 * 1024 * 1024);
    kupl_memcpy_async_null_queue_test(64 * 1024 * 1024 + 1);
    kupl_memcpy_async_boundary_test(1024);
}

void* memcpy_async_task1(void* arg)
{
    char *src = (char *)malloc(sizeof(char) * 1024);
    char *dst = (char *)malloc(sizeof(char) * 1024);
    array_1d_init(src, 1024);
    kupl_event_h event = kupl_event_create();
    int ret = kupl_memcpy_async(dst, src, sizeof(char) * 1024, nullptr, event);
    if (ret == KUPL_OK) {
        int status = kupl_event_query(event);
        while (status != KUPL_EVENT_STATUS_COMPLETE) {
            status = kupl_event_query(event);
        }
        bool res = array_1d_check(src, dst, 1024);
        if (!res) {
            printf("memcpy async failed.\n");
        }
    }
    kupl_event_destroy(event);
    free(src);
    free(dst);
    return NULL;
}

void* memcpy_async_task2(void* arg)
{
    char *src = (char *)malloc(sizeof(char) * 1024);
    char *dst = (char *)malloc(sizeof(char) * 1024);
    kupl_event_h event = kupl_event_create();
    kupl_queue_h queue = kupl_queue_create();

    array_1d_init(src, 1024);
    int ret = kupl_memcpy_async(dst, src, sizeof(char) * 1024, queue, event);
    if (ret == KUPL_ERROR) {
        printf("memcpy async failed.\n");
    }
    kupl_event_wait(event);
    bool res = array_1d_check(src, dst, 1024);
    if (!res) {
        printf("memcpy async failed.\n");
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
    char *src = (char *)malloc(sizeof(char) * 1024);
    char *dst = (char *)malloc(sizeof(char) * 1024);
    array_1d_init(src, 1024);
    int ret = kupl_memcpy_async(dst, src, sizeof(char) * 1024, nullptr, event);
    if (ret == KUPL_OK) {
        int status = kupl_event_query(event);
        while (status != KUPL_EVENT_STATUS_COMPLETE) {
            status = kupl_event_query(event);
        }
        bool res = array_1d_check(src, dst, 1024);
        if (!res) {
            printf("memcpy async failed.\n");
        }
    }
    free(src);
    free(dst);
    return NULL;
}

TEST(test_memory, kupl_memcpy_async_user_create_thread)
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

void array_2d_init(char *array, size_t height, size_t width, size_t spitch)
{
    for (size_t i = 0; i < height; i++) {
        for (size_t j = 0; j < width; j++) {
            array[i * spitch + j] = (char)(i * spitch + j);
        }
    }
}

bool array_2d_check(char *array_src, char *array_dst, size_t height, size_t width, size_t spitch, size_t dpitch)
{
    size_t diffnum = 0;
    for (size_t i = 0; i < height; i++) {
        for (size_t j = 0; j < width; j++) {
            if (array_src[i * spitch + j] != array_dst[i * dpitch + j]) {
                diffnum++;
            }
        }
    }
    return diffnum == 0;
}

void kupl_memcpy2d_test(size_t height, size_t width, size_t spitch, size_t dpitch)
{
    char *src = (char*)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * height * spitch);
    char *dst = (char*)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * height * dpitch);
    if (!src || !dst || spitch < width || dpitch < width || sizeof(char) *(spitch - width) > (1uLL << 32) || sizeof(char) *(dpitch - width) > (1uLL << 32)
        || sizeof(char) * width > (1uLL << 32) || height > (1uLL << 32)) {
        int ret = kupl_memcpy2d(dst, sizeof(char) * dpitch, src, sizeof(char) * spitch,
            sizeof(char) * width, height);
        ASSERT_TRUE(ret == KUPL_ERROR);
    } else {
        array_2d_init(src, height, width, spitch);
        int ret = kupl_memcpy2d(dst, sizeof(char) * dpitch, src, sizeof(char) * spitch,
            sizeof(char) * width, height);
        ASSERT_TRUE(ret == KUPL_OK);
        bool res = array_2d_check(src, dst, height, width, spitch, dpitch);
        ASSERT_TRUE(res);
    }
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
};

void kupl_memcpy2d_boundary_test(size_t height, size_t width, size_t spitch, size_t dpitch)
{
    ASSERT_GE(spitch, width);
    ASSERT_GE(dpitch, width);
    char *src = (char*)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * height * spitch * 2);
    char *dst = (char*)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * height * dpitch * 2);
    array_2d_init(src, height, width, spitch);
    int ret = kupl_memcpy2d(dst, sizeof(char) * dpitch, src, sizeof(char) * spitch,
                            sizeof(char) * width, height);
    ASSERT_TRUE(ret == KUPL_OK);
    bool res = array_2d_check(src, dst, height, width, spitch, dpitch);
    ASSERT_TRUE(res);
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
};

TEST(test_memory, kupl_memcpy2d)
{
    kupl_memcpy2d_test(0, 0, 0, 0);
    kupl_memcpy2d_test(1, 0, 1, 0);
    kupl_memcpy2d_test(1, 2, 3, 4);
    kupl_memcpy2d_test(1, 1, 1, 1);
    kupl_memcpy2d_test(0, 1, 1, 1);
    kupl_memcpy2d_test(0, 1, 0, 1);
    kupl_memcpy2d_test(0, 1, 1, 0);
    kupl_memcpy2d_test(16, 16, 17, 18);
    kupl_memcpy2d_test(17, 18, 19, 20);
    kupl_memcpy2d_test(1024, 1024, 1024, 1024);
    kupl_memcpy2d_test(1024, 2048, 2048, 2048);
    kupl_memcpy2d_test(1024, 2048, 1024, 2048);
    kupl_memcpy2d_test(1024, 2048, 4096, 2048);
    kupl_memcpy2d_test(1024, 2048, 1024, 1024);
    kupl_memcpy2d_test(1024, 2048, 2048, 1024);
    kupl_memcpy2d_test(1024, 2048, 2048, 4096);
    kupl_memcpy2d_test(1024, 16 * 1024, 16 * 1024, 16 * 1024);
    kupl_memcpy2d_test(1024, 16 * 1024 + 1, 16 * 1024 + 2, 16 * 1024 + 3);
    kupl_memcpy2d_test(1025, 16 * 1024 + 1, 16 * 1024 + 2, 16 * 1024 + 3);
    kupl_memcpy2d_test(1025, 16 * 1024 + 1, 16 * 1024, 16 * 1024);
    kupl_memcpy2d_test(1024, 32 * 1024, 32 * 1024, 32 * 1024);
    kupl_memcpy2d_test((1uLL << 32) + 1, 2048, 2048, 4096);
    kupl_memcpy2d_test(1024, 2048, 2049 + (1uLL << 32), 4096);
    kupl_memcpy2d_test(1024, 2048, 2048, 4096 + (1uLL << 32));
    kupl_memcpy2d_test(1024, (1uLL << 32) + 1, (1uLL << 32) + 1, (1uLL << 32) + 1);
    kupl_memcpy2d_test(1024, 32 * 1024 + 1, 32 * 1024 + 2, 32 * 1024 + 3);
    kupl_memcpy2d_test(1025, 32 * 1024 + 1, 32 * 1024 + 2, 32 * 1024 + 3);
    kupl_memcpy2d_test(1025, 32 * 1024 + 1, 32 * 1024, 32 * 1024);
    kupl_memcpy2d_test(1024, 64 * 1024, 64 * 1024, 64 * 1024);
    kupl_memcpy2d_boundary_test(16, 16, 17, 18);
}

void kupl_memcpy2d_async_test(size_t height, size_t width, size_t spitch, size_t dpitch)
{
    char *src = (char *)malloc(sizeof(char) * height * spitch);
    char *dst = (char *)malloc(sizeof(char) * height * dpitch);
    kupl_event_h event = kupl_event_create();
    kupl_queue_h queue = kupl_queue_create();
    if (spitch < width || dpitch < width || spitch - width > (1uLL << 32) || dpitch - width > (1uLL << 32)
        || width > (1uLL << 32) || height > (1uLL << 32)) {
        int ret = kupl_memcpy2d_async(dst, sizeof(char) * dpitch, src,
                                      sizeof(char) * spitch, sizeof(char) * width,
                                      height, queue, event);
        ASSERT_TRUE(ret == KUPL_ERROR);
    } else {
        array_2d_init(src, height, width, spitch);
        int ret = kupl_memcpy2d_async(dst, sizeof(char) * dpitch, src,
                                      sizeof(char) * spitch, sizeof(char) * width,
                                      height, queue, event);
        ASSERT_TRUE(ret == KUPL_OK);
        kupl_event_wait(event);
        bool res = array_2d_check(src, dst, height, width, spitch, dpitch);
        ASSERT_TRUE(res);
    }
    kupl_event_destroy(event);
    kupl_queue_destroy(queue);
    free(src);
    free(dst);
};

void kupl_memcpy2d_async_null_queue_test(size_t height, size_t width, size_t spitch, size_t dpitch)
{
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * height * spitch);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * height * dpitch);
    kupl_event_h event = kupl_event_create();
    if (spitch < width || dpitch < width) {
        int ret = kupl_memcpy2d_async(dst, sizeof(char) * dpitch, src,
                                      sizeof(char) * spitch, sizeof(char) * width,
                                      height, nullptr, event);
        ASSERT_TRUE(ret == KUPL_ERROR);
    } else {
        array_2d_init(src, height, width, spitch);
        int ret = kupl_memcpy2d_async(dst, sizeof(char) * dpitch, src,
                                      sizeof(char) * spitch, sizeof(char) * width,
                                      height, nullptr, event);
        if (ret == KUPL_OK) {
            kupl_event_wait(event);
            bool res = array_2d_check(src, dst, height, width, spitch, dpitch);
            ASSERT_TRUE(res);
        }
    }
    kupl_event_destroy(event);
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
};

void kupl_memcpy2d_async_boundary_test(size_t height, size_t width, size_t spitch, size_t dpitch)
{
    ASSERT_GE(spitch, width);
    ASSERT_GE(dpitch, width);
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * height * spitch * 2);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * height * dpitch * 2);
    kupl_event_h event = kupl_event_create();
    kupl_queue_h queue = kupl_queue_create();
    int ret = kupl_memcpy2d_async(dst, sizeof(char) * dpitch, src,
                                  sizeof(char) * spitch, sizeof(char) * width,
                                  height, queue, nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);

    array_2d_init(src, height, width, spitch);
    ret = kupl_memcpy2d_async(dst, sizeof(char) * dpitch, src,
                              sizeof(char) * spitch, sizeof(char) * width,
                              height, queue, event);
    ASSERT_TRUE(ret == KUPL_OK);
    kupl_event_wait(event);
    bool res = array_2d_check(src, dst, height, width, spitch, dpitch);
    ASSERT_TRUE(res);
    kupl_event_destroy(event);
    kupl_queue_destroy(queue);
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
};

TEST(test_memory, kupl_memcpy2d_async)
{
    kupl_memcpy2d_async_test(0, 0, 0, 0);
    kupl_memcpy2d_async_test(1, 0, 1, 0);
    kupl_memcpy2d_async_test(1, 2, 3, 4);
    kupl_memcpy2d_async_test(1, 1, 1, 1);
    kupl_memcpy2d_async_test(0, 1, 1, 1);
    kupl_memcpy2d_async_test(16, 16, 17, 18);
    kupl_memcpy2d_async_test(17, 18, 19, 20);
    kupl_memcpy2d_async_test(1024, 1024, 1024, 1024);
    kupl_memcpy2d_async_test(1024, 2048, 2048, 2048);
    kupl_memcpy2d_async_test(1024, 2048, 1024, 2048);
    kupl_memcpy2d_async_test(1024, 2048, 4096, 2048);
    kupl_memcpy2d_async_test(1024, 2048, 1024, 1024);
    kupl_memcpy2d_async_test(1024, 2048, 2048, 1024);
    kupl_memcpy2d_async_test(1024, 2048, 2048, 4096);
    kupl_memcpy2d_async_test(1024, 16 * 1024, 16 * 1024, 16 * 1024);
    kupl_memcpy2d_async_test(1024, 16 * 1024 + 1, 16 * 1024 + 2, 16 * 1024 + 3);
    kupl_memcpy2d_async_test(1025, 16 * 1024 + 1, 16 * 1024 + 2, 16 * 1024 + 3);
    kupl_memcpy2d_async_test(1024, 32 * 1024, 32 * 1024, 32 * 1024);
    kupl_memcpy2d_async_test(1024, 32 * 1024 + 1, 32 * 1024 + 2, 32 * 1024 + 3);
    kupl_memcpy2d_async_test(1025, 32 * 1024 + 1, 32 * 1024 + 2, 32 * 1024 + 3);
    kupl_memcpy2d_async_test(1024, 64 * 1024, 64 * 1024, 64 * 1024);
    kupl_memcpy2d_async_test((1uLL << 32) + 1, 2048, 2048, 4096);
    kupl_memcpy2d_async_test(1024, 2048, 2049 + (1uLL << 32), 4096);
    kupl_memcpy2d_async_test(1024, 2048, 2048, 4096 + (1uLL << 32));
    kupl_memcpy2d_async_test(1024, (1uLL << 32) + 1, (1uLL << 32) + 1, (1uLL << 32) + 1);
    kupl_memcpy2d_async_null_queue_test(1024, 1024, 1024, 1024);
    kupl_memcpy2d_async_null_queue_test(1024, 2048, 2048, 2048);
    kupl_memcpy2d_async_null_queue_test(1024, 2048, 1024, 2048);
    kupl_memcpy2d_async_null_queue_test(1024, 2048, 4096, 2048);
    kupl_memcpy2d_async_null_queue_test(1024, 2048, 1024, 1024);
    kupl_memcpy2d_async_null_queue_test(1024, 2048, 2048, 1024);
    kupl_memcpy2d_async_null_queue_test(1024, 2048, 2048, 4096);
    kupl_memcpy2d_async_boundary_test(16, 16, 17, 18);
}

void* memcpy2d_async_task1(void* arg)
{
    char *src = (char *)malloc(sizeof(char) * 1 * 1024);
    char *dst = (char *)malloc(sizeof(char) * 1 * 1024);
    kupl_event_h event = kupl_event_create();

    array_2d_init(src, 1, 1024, 1024);
    int ret = kupl_memcpy2d_async(dst, sizeof(char) * 1024, src,
                                    sizeof(char) * 1024, sizeof(char) * 1024,
                                    1, nullptr, event);
    if (ret == KUPL_OK) {
        kupl_event_wait(event);
        bool res = array_2d_check(src, dst, 1, 1024, 1024, 1024);
        if (!res) {
            printf("memcpy2d async failed.\n");
        }
    }

    kupl_event_destroy(event);
    free(src);
    free(dst);
    return NULL;
}

void* memcpy2d_async_task2(void* arg)
{
    char *src = (char *)malloc(sizeof(char) * 1 * 1024);
    char *dst = (char *)malloc(sizeof(char) * 1 * 1024);
    kupl_event_h event = kupl_event_create();
    kupl_queue_h queue = kupl_queue_create();

    array_2d_init(src, 1, 1024, 1024);
    int ret = kupl_memcpy2d_async(dst, sizeof(char) * 1024, src,
                                    sizeof(char) * 1024, sizeof(char) * 1024,
                                    1, queue, event);
    if (ret == KUPL_ERROR) {
        printf("memcpy2d async failed.\n");
    }
    kupl_event_wait(event);
    bool res = array_2d_check(src, dst, 1, 1024, 1024, 1024);
    if (!res) {
        printf("memcpy2d async failed.\n");
    }

    kupl_event_destroy(event);
    kupl_queue_destroy(queue);
    free(src);
    free(dst);
    return NULL;
}

void* memcpy2d_async_task3(void* arg)
{
    char *src = (char *)malloc(sizeof(char) * 1 * 1024);
    char *dst = (char *)malloc(sizeof(char) * 1 * 1024);

    array_2d_init(src, 1, 1024, 1024);
    int ret = kupl_memcpy2d_async(dst, sizeof(char) * 1024, src,
                                    sizeof(char) * 1024, sizeof(char) * 1024,
                                    1, nullptr, event);
    if (ret == KUPL_OK) {
        kupl_event_wait(event);
        bool res = array_2d_check(src, dst, 1, 1024, 1024, 1024);
        if (!res) {
            printf("memcpy2d async failed.\n");
        }
    }
    free(src);
    free(dst);
    return NULL;
}

TEST(test_memory, kupl_memcpy2d_async_user_create_thread)
{
    event = kupl_event_create();
    pthread_t thread1;
    pthread_create(&thread1, NULL, memcpy2d_async_task1, NULL);
    pthread_t thread2;
    pthread_create(&thread2, NULL, memcpy2d_async_task2, NULL);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    pthread_t thread3;
    pthread_create(&thread3, NULL, memcpy2d_async_task3, NULL);
    pthread_join(thread3, NULL);
    kupl_event_destroy(event);
}

TEST(test_memory, kupl_memcpy_err)
{
    size_t sz = 1024;
    kupl_memcpy(nullptr, nullptr, sz);
    char *src = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * sz);
    char *dst = (char *)kupl_malloc(KUPL_MEM_DEFAULT, sizeof(char) * sz);
    kupl_event_h event = kupl_event_create();
    int ret = kupl_memcpy_async(dst, src, sizeof(char) * sz, nullptr, event);
    if (ret == KUPL_OK) {
        ret = kupl_memcpy_async(dst, src, sizeof(char) * sz, nullptr, event);
        ASSERT_TRUE(ret == KUPL_ERROR);
        kupl_event_wait(event);
    }

    ret = kupl_memcpy_async(dst, src, sizeof(char) * sz, nullptr, event);
    if (ret == KUPL_OK) {
        int status = kupl_event_query(event);
        while( status != KUPL_EVENT_STATUS_COMPLETE) {
            status = kupl_event_query(event);
        }
        status = kupl_event_query(event);
        kupl_event_wait(event);
        bool res = array_1d_check(src, dst, sz);
        ASSERT_TRUE(res);
    }
    kupl_event_destroy(event);

    kupl_memcpy2d(dst, sz, src, sz, sz, (1uLL << 32) + 1);
    kupl_free(KUPL_MEM_DEFAULT, src);
    kupl_free(KUPL_MEM_DEFAULT, dst);
}
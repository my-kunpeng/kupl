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
#ifndef KUPL_POOL_H
#define KUPL_POOL_H

#include <climits>
#include "kupl.h"
#include "utils/sys/kupl_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kupl_pool {
    const char *name;
    size_t head;
    size_t tail;
    size_t capacity;
    size_t elem_size;
    void *data;
} kupl_pool_t;

static kupl_always_inline
kupl_pool_t* kupl_pool_create(size_t capacity, size_t elem_size)
{
    size_t actual_capacity = 1;
    while (actual_capacity < capacity) {
        actual_capacity <<= 1;
    }
    if (elem_size > (SIZE_MAX - sizeof(kupl_pool_t)) / actual_capacity) {
        kupl_error("elem size too large");
        return nullptr;
    }
    size_t alloc_size = sizeof(kupl_pool_t) + elem_size * actual_capacity;
    auto pool = (kupl_pool_t *)kupl_calloc(1, alloc_size);
    if (kupl_unlikely(pool == nullptr)) {
        return nullptr;
    }

    pool->name = "pool";
    pool->head = 0;
    pool->tail = 0;
    pool->capacity = actual_capacity;
    pool->elem_size = elem_size;
    pool->data = pool + 1;  /* because we alloc vector meta and data together */

    return pool;
}

static kupl_always_inline
void kupl_pool_cleanup(kupl_pool_t *pool)
{
    if (kupl_unlikely(pool == nullptr)) {
        return;
    }

    pool->head = 0;
    pool->tail = 0;
    pool->capacity = 0;
    pool->data = nullptr;

    kupl_free_inner(pool);
}

#define kupl_pool_size(_pool)                ((_pool)->tail - (_pool)->head)
#define kupl_pool_empty(_pool)               ((_pool)->head == (_pool)->tail)
#define kupl_pool_full(_pool)                (((_pool)->tail - (_pool)->head + 1) >= ((_pool)->capacity))
#define kupl_pool_put(_pool, _type, _elem)             \
do {                                                    \
    auto _arr = (_type *)(_pool)->data;                 \
    auto _push_idx = (_pool)->tail % (_pool)->capacity; \
    _arr[_push_idx] = _elem;                            \
    (_pool)->tail++;                                    \
} while (0)

#define kupl_pool_get(_pool, _elem)                                \
do {                                                                \
    decltype(&(_elem)) _arr = (decltype(&(_elem)))(_pool)->data;    \
    auto _idx = (_pool)->head++ % (_pool)->capacity;                \
    _elem = _arr[_idx];                                             \
} while (0)

#define kupl_pool_cleanup_all(_pool, _type, _clean, _args...)  \
do {                                                            \
    _type _v;                                                   \
    while (!kupl_pool_empty(_pool)) {                          \
        kupl_pool_get(_pool, _v);                              \
        _clean(_v, ##_args);                                    \
    }                                                           \
    kupl_pool_cleanup(_pool);                                  \
} while (0)

#ifdef __cplusplus
}
#endif

#endif
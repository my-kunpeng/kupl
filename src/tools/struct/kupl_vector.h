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
#ifndef KUPL_VECTOR_H
#define KUPL_VECTOR_H

#include <cstring>
#include <cstdint>
#include <climits>
#include "kupl.h"
#include "utils/arch/kupl_atomic.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/debug/kupl_log.h"
#include "memory/mpool/kupl_mpool.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief fixed length of vector
 */
typedef struct kupl_vector {
    const char *name;
    KUPL_ATOMIC_SIZE_T head; /* the head element index in this vector */
    KUPL_ATOMIC_SIZE_T tail; /* the tail element index in this vector */
    size_t capacity;         /* max count in this vector */
    size_t elem_size;        /* the size of element */
    void *data;              /* the data buffer */
} kupl_vector_t;

/**
 * @brief create a kupl_vector with capacity
 */
kupl_vector_t *kupl_vector_create(size_t capacity, size_t elem_size, const char *name);

/**
 * @brief cleanup the vector and release all memory
 */
void kupl_vector_cleanup(kupl_vector_t *vector);

/**
 * @brief check wheather the vector is empty
 */
static kupl_always_inline int kupl_vector_empty(kupl_vector_t *vector)
{
    if (kupl_unlikely(vector == nullptr)) {
        return 0;
    }

    return KUPL_ATOMIC_LD_RLX(&vector->head) == KUPL_ATOMIC_LD_RLX(&vector->tail);
}

/**
 * @brief get the size of vector
 */
static kupl_always_inline size_t kupl_vector_size(kupl_vector_t *vector)
{
    if (kupl_unlikely(vector == nullptr)) {
        return 0;
    }

    return KUPL_ATOMIC_LD_RLX(&vector->tail) - KUPL_ATOMIC_LD_RLX(&vector->head);
}

/**
 * @brief push the @b elem to vector's tail
 * @return KUPL_OK for push success, other for failed (maybe vector is fully)
 */
static kupl_always_inline int kupl_vector_push_back(kupl_vector_t *vector, const void *elem)
{
    if (kupl_unlikely(vector == nullptr || elem == nullptr)) {
        return KUPL_ERROR;
    }

    if (kupl_unlikely((vector->tail - vector->head + 1) >= vector->capacity)) {
        /* vector is full */
        return KUPL_ERROR;
    }

    auto push_index = vector->tail % vector->capacity;
    uint8_t *dest = (uint8_t *)vector->data + vector->elem_size * push_index;
    memcpy(dest, elem, vector->elem_size);
    KUPL_ATOMIC_ADD_RLS(&vector->tail, 1);

    return KUPL_OK;
}

/**
 * @brief pop the front element
 */
static kupl_always_inline void kupl_vector_pop_front(kupl_vector_t *vector)
{
    if (kupl_unlikely(vector == nullptr || vector->head == vector->tail)) {
        return;
    }

    vector->head++;
    return;
}

/**
 * @brief get the vector[idx] element
 */
static kupl_always_inline int kupl_vector_get(kupl_vector_t *vector, size_t idx, void *elem)
{
    if (kupl_unlikely(vector == nullptr || idx >= kupl_vector_size(vector) || elem == nullptr)) {
        return KUPL_ERROR;
    }

    auto index = (vector->head + idx) % vector->capacity;
    auto src = (uint8_t *)vector->data + vector->elem_size * index;
    memcpy(elem, src, vector->elem_size);

    return KUPL_OK;
}

/**
 * @brief set the vector[idx] element
 */
static kupl_always_inline int kupl_vector_set(kupl_vector_t *vector, size_t idx, const void *elem)
{
    if (kupl_unlikely(vector == nullptr || idx >= kupl_vector_size(vector) || elem == nullptr)) {
        return KUPL_ERROR;
    }

    auto index = (vector->head + idx) % vector->capacity;
    auto dest = (uint8_t *)vector->data + vector->elem_size * index;
    memcpy(dest, elem, vector->elem_size);

    return KUPL_OK;
}

static kupl_always_inline int kupl_vector_front(kupl_vector_t *vector, void *elem)
{
    if (kupl_unlikely(vector == nullptr || vector->tail <= vector->head)) {
        return KUPL_ERROR;
    }

    return kupl_vector_get(vector, 0, elem);
}

static kupl_always_inline int kupl_vector_back(kupl_vector_t *vector, void *elem)
{
    if (kupl_unlikely(vector == nullptr || vector->tail <= vector->head)) {
        return KUPL_ERROR;
    }

    size_t tail_index = vector->tail - vector->head - 1;
    return kupl_vector_get(vector, tail_index, elem);
}

#define kupl_vector_push_back_macro(_v, _elem)                   \
    ({                                                           \
        int _ret = KUPL_OK;                                      \
        auto _arr = (decltype(&(_elem)))(_v)->data;              \
        auto _tail = KUPL_ATOMIC_LD_RLX(&((_v)->tail));          \
        auto _head = KUPL_ATOMIC_LD_RLX(&((_v)->head));          \
        if (kupl_likely((_tail - _head + 1) < (_v)->capacity)) { \
            auto _push_idx = _tail % (_v)->capacity;             \
            _arr[_push_idx] = _elem;                             \
            KUPL_ATOMIC_ADD_RLS(&((_v)->tail), 1);               \
        } else {                                                 \
            _ret = KUPL_ERROR;                                   \
        }                                                        \
        _ret;                                                    \
    })

#define kupl_vector_front_macro(_v, _elem)                              \
    do {                                                                \
        decltype(&(_elem)) _arr = (decltype(&(_elem)))(_v)->data;       \
        auto _idx = KUPL_ATOMIC_LD_RLX(&((_v)->head)) % (_v)->capacity; \
        _elem = _arr[_idx];                                             \
    } while (0)

#define kupl_vector_back_macro(_v, _elem)                                     \
    do {                                                                      \
        decltype(&(_elem)) _arr = (decltype(&(_elem)))(_v)->data;             \
        auto _idx = (KUPL_ATOMIC_LD_RLX(&((_v)->tail)) - 1) % (_v)->capacity; \
        _elem = _arr[_idx];                                                   \
    } while (0)

#define kupl_vector_full_macro(_v) \
    (KUPL_ATOMIC_LD_RLX(&((_v)->tail)) - KUPL_ATOMIC_LD_RLX(&((_v)->head))) + 1 >= (_v)->capacity)
#define kupl_vector_empty_macro(_v) (KUPL_ATOMIC_LD_RLX(&((_v)->head)) == KUPL_ATOMIC_LD_RLX(&((_v)->tail)))
#define kupl_vector_pop_front_macro(_v) KUPL_ATOMIC_ADD_RLX(&((_v)->head), 1)

#ifdef __cplusplus
}
#endif

#endif
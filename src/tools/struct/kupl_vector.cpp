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
#include "kupl_vector.h"

kupl_vector_t *kupl_vector_create(size_t capacity, size_t elem_size, const char *name)
{
    if (kupl_unlikely(elem_size == 0)) {
        kupl_error("illegitimate vector elem size");
        return nullptr;
    }
    size_t actual_capacity = 1;
    while (actual_capacity < capacity) {
        actual_capacity <<= 1;
    }
    if (elem_size > (SIZE_MAX - sizeof(kupl_vector_t)) / actual_capacity) {
        kupl_error("elem size too large");
        return nullptr;
    }
    size_t alloc_size = sizeof(kupl_vector_t) + elem_size * actual_capacity;
    kupl_vector_t *vector = (kupl_vector_t *)kupl_calloc(1, alloc_size);
    if (kupl_unlikely(vector == nullptr)) {
        return nullptr;
    }

    vector->name = name;
    vector->head = 0;
    vector->tail = 0;
    vector->capacity = actual_capacity;
    vector->elem_size = elem_size;
    vector->data = vector + 1; /* because we alloc vector meta and data together */

    return vector;
}

void kupl_vector_cleanup(kupl_vector_t *vector)
{
    if (kupl_unlikely(vector == nullptr)) {
        return;
    }

    vector->head = 0;
    vector->tail = 0;
    vector->capacity = 0;
    vector->data = nullptr;

    kupl_free_inner(vector);
}

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
#ifndef KUPL_LIST_H
#define KUPL_LIST_H

#include "memory/mpool/kupl_mpool.h"
#include "utils/sys/kupl_compiler.h"

typedef struct kupl_list {
    struct kupl_list *prev;
    struct kupl_list *next;
} kupl_list_t;

#define kupl_list_init(_list) (_list)->prev = (_list)->next = (_list)

#define kupl_list_insert_before(_list, _elem)      \
do {                                                \
    (_elem)->prev = (_list)->prev;                  \
    (_elem)->next = _list;                          \
    (_list)->prev->next = _elem;                    \
    (_list)->prev = _elem;                          \
} while (0)

#define kupl_list_insert_after(_list, _elem)       \
do {                                                \
    (_elem)->prev = _list;                          \
    (_elem)->next = (_list)->next;                  \
    (_list)->next->prev = _elem;                    \
    (_list)->next = _elem;                          \
} while (0)

#define kupl_list_del(_elem)                       \
do {                                                \
    (_elem)->prev->next = (_elem)->next;            \
    (_elem)->next->prev = (_elem)->prev;            \
} while (0)

#define kupl_list_is_empty(_list) ((_list)->next == (_list))

/**
 * @brief Singly Void* Node list put, every node is void* type pointer
 */
#define kupl_sv_list_put(_head, _elem)             \
do {                                                \
    *(void **)(_elem) = _head;                      \
    (_head) = (_elem);                              \
} while (0)

#define kupl_sv_list_get(_head)                    \
({                                                  \
    void *_elem = (_head);                          \
    if (_elem != nullptr) {                         \
        (_head) = *((void **)_elem);                \
    }                                               \
    _elem;                                          \
})


typedef struct kupl_slist kupl_slist_t;

/**
 * @brief single linked list
 *
 */
struct kupl_slist {
    struct kupl_slist  *next;
    void                *data;
};

static kupl_always_inline
kupl_slist_t* kupl_slist_create(kupl_slist_t *next, void *data, int geid)
{
    auto link = (kupl_slist_t*)kupl_memory_alloc(sizeof(kupl_slist_t), geid);
    if (kupl_likely(link != nullptr)) {
        link->next = next;
        link->data = data;
    }
    return link;
}

static kupl_always_inline
void kupl_slist_insert_front(kupl_slist_t **head, void *data, int geid)
{
    if (kupl_unlikely(head == nullptr)) {
        return;
    }
    auto link = (kupl_slist_t*)kupl_memory_alloc(sizeof(kupl_slist_t), geid);
    if (kupl_likely(link != nullptr)) {
        link->next = *head;
        link->data = data;
        *head = link;
    }
}

static kupl_always_inline
void kupl_slist_insert_behind(kupl_slist_t **head, void *data, int geid)
{
    if (kupl_unlikely(head == nullptr || *head == nullptr)) {
        return;
    }
    auto link = (kupl_slist_t*)kupl_memory_alloc(sizeof(kupl_slist_t), geid);
    if (kupl_likely(link != nullptr)) {
        link->data = data;
        link->next = nullptr;
        (*head)->next = link;
        *head = link;
    }
}

// delete current slist, point to next slist
static kupl_always_inline
void kupl_slist_destroy(kupl_slist_t **head, int geid)
{
    if (kupl_unlikely(head == nullptr || *head == nullptr)) {
        return;
    }
    kupl_slist_t *tmp = *head;
    *head = (*head)->next;
    kupl_memory_free(tmp, geid);
}

static kupl_always_inline
void kupl_slist_destroy_all(kupl_slist_t **head, int geid)
{
    while (*head != nullptr) {
        kupl_slist_t *tmp = *head;
        *head = (*head)->next;
        kupl_memory_free(tmp, geid);
    }
}

static kupl_always_inline
uint32_t kupl_slist_count(kupl_slist_t *head)
{
    uint32_t cnt = 0;
    while (head != nullptr) {
        cnt += 1;
        head = head->next;
    }
    return cnt;
}

#endif
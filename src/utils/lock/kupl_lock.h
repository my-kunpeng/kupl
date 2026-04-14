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
#ifndef KUPL_LOCK_H
#define KUPL_LOCK_H

#include "kupl.h"
#include "memory/mpool/kupl_mpool.h"
#include "utils/arch/kupl_atomic.h"
#include "utils/sys/kupl_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/* spinlock wait */
#if defined(__arm__) || defined(__aarch64__)
#define KUPL_ARM_ARCH
#endif

typedef enum kupl_lock_type {
    PTHREAD_SPINLOCK,
    TICKET_ARRAY_LOCK,
} kupl_lock_type_t;

typedef struct kupl_lock kupl_lock_t;
typedef void (*kupl_lock_func_lock_t)(kupl_lock_t *);
typedef void (*kupl_lock_func_unlock_t)(kupl_lock_t *);
typedef int (*kupl_lock_func_trylock_t)(kupl_lock_t *);

typedef struct kupl_lock {
    kupl_lock_type_t type;
    void *raw_lock;
    volatile void *vol_raw_lock;
    kupl_lock_func_lock_t lock;
    kupl_lock_func_unlock_t unlock;
    kupl_lock_func_trylock_t trylock;   /* retrun 1 for get lock */
} kupl_lock_t;

/**
 * @brief Create a kupl_lock_t type is @b type
 */
kupl_lock_t* kupl_lock_create(kupl_lock_type type);

/**
 * @brief Cleanup a kupl_lock_t
 */
void kupl_lock_cleanup(kupl_lock_t *lock);

/* spinlock pause wait */
static kupl_always_inline
void kupl_spin_wait()
{
#ifdef KUPL_ARM_ARCH
    __asm__ __volatile__ ("yield");
#else
    #pragma message ("No 'pause' instruction/intrisic found for this architecture ")
#endif
}

/* spinlock pause wait release */
static kupl_always_inline
void kupl_spin_wait_release()
{
}

typedef struct kupl_lock_guard_data {
    bool lock __attribute__((aligned (8)));
    int count;
    int status;
} kupl_lock_guard_data_t;

struct kupl_lock_guard_t {
    KUPL_ATOMIC_FLAG *lock;

    kupl_lock_guard_t(const kupl_lock_guard_t&) = delete;
    kupl_lock_guard_t& operator=(const kupl_lock_guard_t&) = delete;

    explicit kupl_lock_guard_t(bool *flag) : lock((KUPL_ATOMIC_FLAG*)flag)
    {
        while (lock->test_and_set(std::memory_order_acquire)) {
            /* busy wait */
        };
    }

    ~kupl_lock_guard_t()
    {
        lock->clear();
    }
};

/* if _ret == 1, the invoker func should return _guard_data.status */
#define kupl_init_lock_guard(_guard_data)           \
({                                                  \
    int _ret = 0;                                   \
    kupl_lock_guard_t guard(&(_guard_data).lock);   \
    if ((_guard_data).count++ > 0) {                \
        _ret = 1;                                   \
    }                                               \
    _ret;                                           \
})

/* if _ret == 1, the invoker func should return */
#define kupl_fini_lock_guard(_guard_data)           \
({                                                  \
    int _ret = 0;                                   \
    kupl_lock_guard_t guard(&(_guard_data).lock);   \
    if (--(_guard_data).count != 0) {               \
        _ret = 1;                                   \
    }                                               \
    _ret;                                           \
})

#define kupl_guard_data_set_status(_guard_data, _status)    \
do {                                                        \
    (_guard_data).status = _status;                         \
    if ((_status) != KUPL_OK) {                             \
        (_guard_data).count--;                              \
    }                                                       \
} while (0)

#ifdef __cplusplus
}
#endif

#endif
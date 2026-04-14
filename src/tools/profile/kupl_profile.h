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
#ifndef KUPL_PROFILE_H
#define KUPL_PROFILE_H

#include <cstdint>
#include "utils/time/kupl_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Paste two tokens */
#define _KUPL_TOKENPASTE(_a, _b) _a ## _b
#define KUPL_TOKENPASTE(_a, _b) _KUPL_TOKENPASTE(_a, _b)

/* Count number of macro arguments */
#define _KUPL_NUM_ARGS(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, N, ...) N
#define KUPL_NUM_ARGS(...) _KUPL_NUM_ARGS(, ## __VA_ARGS__, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

/* Expand macro for each argument in the list. */
#define KUPL_FOREACH_1(_macro, _op, _a, ...) _macro(_a)
#define KUPL_FOREACH_2(_macro, _op, _a, ...) _macro(_a); _op(); KUPL_FOREACH_1(_macro, _op, __VA_ARGS__)
#define KUPL_FOREACH_3(_macro, _op, _a, ...) _macro(_a); _op(); KUPL_FOREACH_2(_macro, _op, __VA_ARGS__)
#define KUPL_FOREACH_4(_macro, _op, _a, ...) _macro(_a); _op(); KUPL_FOREACH_3(_macro, _op, __VA_ARGS__)
#define KUPL_FOREACH_5(_macro, _op, _a, ...) _macro(_a); _op(); KUPL_FOREACH_4(_macro, _op, __VA_ARGS__)
#define KUPL_FOREACH_6(_macro, _op, _a, ...) _macro(_a); _op(); KUPL_FOREACH_5(_macro, _op, __VA_ARGS__)
#define KUPL_FOREACH_7(_macro, _op, _a, ...) _macro(_a); _op(); KUPL_FOREACH_6(_macro, _op, __VA_ARGS__)
#define KUPL_FOREACH_8(_macro, _op, _a, ...) _macro(_a); _op(); KUPL_FOREACH_7(_macro, _op, __VA_ARGS__)
#define KUPL_FOREACH_9(_macro, _op, _a, ...) _macro(_a); _op(); KUPL_FOREACH_8(_macro, _op, __VA_ARGS__)
#define KUPL_FOREACH_10(_macro, _op, _a, ...) _macro(_a); _op(); KUPL_FOREACH_9(_macro, _op, __VA_ARGS__)
#define KUPL_FOREACH_11(_macro, _op, _a, ...) _macro(_a); _op(); KUPL_FOREACH_10(_macro, _op, __VA_ARGS__)
#define KUPL_FOREACH_12(_macro, _op, _a, ...) _macro(_a); _op(); KUPL_FOREACH_11(_macro, _op, __VA_ARGS__)
#define KUPL_FOREACH_13(_macro, _op, _a, ...) _macro(_a); _op(); KUPL_FOREACH_12(_macro, _op, __VA_ARGS__)
#define KUPL_FOREACH_14(_macro, _op, _a, ...) _macro(_a); _op(); KUPL_FOREACH_13(_macro, _op, __VA_ARGS__)
#define KUPL_FOREACH(_macro, _op, ...) \
    KUPL_TOKENPASTE(KUPL_FOREACH_, KUPL_NUM_ARGS(__VA_ARGS__)(_macro, _op, __VA_ARGS__))


#ifdef ENABLE_KUPL_PROFILE

#define MAX_THREAD_NUM         1024     /* max profile thread */
#define MAX_PROFILE_ID         1024     /* max id */

#define profile_now_ns kupl_now_ns
int profile_get_threadid(void);

void profile_module_init(void);
void profile_module_fini(void);
int profile_register_id(const char *name);
int profile_register_id_once(int *id, const char *name);
void profile_start(int id);
void profile_end(int id);
void profile_cancel(int id);
void profile_clone(int src_id, int dst_id);
void profile_summary(int max_tid, int max_id);
void profile_reset(void);
int get_kupl_prof_level_profile();

/** @note all id var must define in global static variable, because this function is not thread safe */
#define __NOP()
#define __ID_VAR(_var)                 __profile_id_ ## _var
#define __ID_DEFINE(_var)              static int __ID_VAR(_var) = 0
#define __ID_INIT(_var)                __ID_VAR(_var) = profile_register_id(# _var)
#define __ID_DEFIEN_INIT(_var)         static int __ID_INIT(_var)
#define PROFILE_ID_REG(...)            KUPL_FOREACH(__ID_DEFIEN_INIT, __NOP, __VA_ARGS__)
#define PROFILE_START(_var)            profile_start(__ID_VAR(_var))
#define PROFILE_END(_var)              profile_end(__ID_VAR(_var))
#define PROFILE_CANCEL(_var)           profile_cancel(__ID_VAR(_var))
#define PROFILE_CLONE(_var0, _var1)    profile_clone(__ID_VAR(_var0), __ID_VAR(_var1))
#define KUPL_PROF_LEVEL_PROFILE        !get_kupl_prof_level_profile()

/**
 * @brief simple add START() and END() before code
 * @note CAN NOT contain return in _code block.
 */
#define PROFILE_CODE_START(_var)              \
    static int __ID_VAR(_var) = -1;           \
({                                            \
    if (KUPL_PROF_LEVEL_PROFILE) {            \
        __ID_VAR(_var)  = profile_register_id_once(&__ID_VAR(_var), # _var);    \
        asm volatile(""::: "memory");         \
        PROFILE_START(_var);                  \
        asm volatile(""::: "memory");         \
    }                                         \
})

#define PROFILE_CODE_END(_var)                \
({                                            \
    if (KUPL_PROF_LEVEL_PROFILE) {            \
        asm volatile(""::: "memory");         \
        PROFILE_END(_var);                    \
        asm volatile(""::: "memory");         \
    }                                         \
})

#define PROFILE_CODE_COND_START(_var, _cond)                                \
    static int __ID_VAR(_var)   = -1;                                       \
    static int __ID_VAR(_var ## _1)   = -1;                                 \
    static int __ID_VAR(_var ## _0)   = -1;                                 \
({                                                                          \
    if (KUPL_PROF_LEVEL_PROFILE) {                                          \
        __ID_VAR(_var)  = profile_register_id_once(&__ID_VAR(_var), # _var);                    \
        __ID_VAR(_var ## _1)  = profile_register_id_once(&__ID_VAR(_var ## _1), # _var"_1");    \
        __ID_VAR(_var ## _0)  = profile_register_id_once(&__ID_VAR(_var ## _0), # _var"_0");    \
        asm volatile(""::: "memory");                                       \
        PROFILE_START(_var);                                                \
        asm volatile(""::: "memory");                                       \
    }                                                                       \
})

#define PROFILE_CODE_COND_END(_var, _cond)                                  \
({                                                                          \
    if (KUPL_PROF_LEVEL_PROFILE) {                                          \
        asm volatile(""::: "memory");                                       \
        PROFILE_END(_var);                                                  \
        asm volatile(""::: "memory");                                       \
        if (_cond) {                                                        \
            PROFILE_CLONE(_var, _var##_1);                                  \
        } else {                                                            \
            PROFILE_CLONE(_var, _var##_0);                                  \
        }                                                                   \
        PROFILE_CANCEL(_var);                                               \
    }                                                                       \
})


void profile_stats_record(uint64_t key, const char *file, const char *func, int line, const char *_msg);
#define PROFILE_DATA_STATS(_key, _msg)  profile_stats_record(_key, __FILE__, __FUNCTION__, __LINE__, _msg)

#else

#define profile_module_init()
#define profile_module_fini()
#define profile_start(_id)
#define profile_end(_id)
#define profile_summary(...)
#define profile_reset()
#define PROFILE_ID_REG(...)
#define PROFILE_START(_var)
#define PROFILE_END(_var)
#define PROFILE_CANCEL(_var)
#define PROFILE_CLONE(_var0, _var1)
#define PROFILE_CODE_START(_var)
#define PROFILE_CODE_END(_var)
#define PROFILE_CODE_COND_START(_var, _cond)
#define PROFILE_CODE_COND_END(_var, _cond)
#define PROFILE_DATA_STATS(...)

#endif

#ifdef __cplusplus
}
#endif

#endif
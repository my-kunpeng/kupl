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
#ifndef KUPL_PROFILE_TRACE_H
#define KUPL_PROFILE_TRACE_H

#include <cstdint>
#include "utils/time/kupl_time.h"

#ifdef __cplusplus
extern "C" {
#endif

enum kupl_ptrace_tag_t {
    KUPL_PTRACE_TASK_CREATE,                        /* task create */
    KUPL_PTRACE_TASK_SUBMIT,                        /* submit task to dag */
    KUPL_PTRACE_TASK_ENQUEUE,                       /* add task to sched */
    KUPL_PTRACE_TASK_DEQUEUE,                       /* add task to sched */
    KUPL_PTRACE_TASK_EXECUTE,                       /* execute task */
    KUPL_PTRACE_PERIOD_TASK_EXECUTE,                /* period task not finish */
    KUPL_PTRACE_TASK_RESET,                         /* reset task */
    KUPL_PTRACE_TASK_RELEASE,                       /* release task */
    KUPL_PTRACE_TASK_FINISH,                        /* finish task */
    KUPL_PTRACE_TASK_INIT,                          /* initial task */
    KUPL_PTRACE_TASK_CLEANUP,                       /* clean task */
    KUPL_PTRACE_TASK_INVOKE,                        /* invoke task */
    KUPL_PTRACE_TASK_WAIT,                          /* wait task */
    KUPL_PTRACE_TASK_IN_LOOP,                       /* repeat task in loop */
    KUPL_PTRACE_ULT_CREATE,                         /* create ult */
	KUPL_PTRACE_ULT_INIT,                           /* initial ult */
	KUPL_PTRACE_ULT_CLEANUP,                        /* clean ult */
	KUPL_PTRACE_ULT_EXECUTE,                        /* execute ult */
	KUPL_PTRACE_ULT_FINISH,                         /* finish ult */
	KUPL_PTRACE_ULT_INVOKE,                         /* invoke ult */
	KUPL_PTRACE_GLOBAL_EGROUP_GET,                  /* get global egroup */
    KUPL_PTRACE_PARALLEL_FOR_GRAPH_GET,             /* get gragh which in parallel for */
	KUPL_PTRACE_PARALLEL_FOR_CHECK,                 /* check parallel for */
	KUPL_PTRACE_PARALLEL_FOR_NUM_THREADS,           /* get the number of threads which in parallel for */
	KUPL_PTRACE_PARALLEL_FOR_POLICY_STATIC,         /* 1 dimension static parallel for */
	KUPL_PTRACE_PARALLEL_FOR_POLICY_DYNAMIC,        /* 1 dimension dynamic parallel for */
	KUPL_PTRACE_PARALLEL_FOR_POLICY_3D_STATIC,      /* 3 dimension static parallel for */
    KUPL_PTRACE_PARALLEL_FOR_POLICY_TASK,           /* 1 dimension taskloop parallel for */
	KUPL_PTRACE_PARALLEL_FOR,                       /* parallel for main interface */
	KUPL_PTRACE_INVOKE_PARALLEL_FUNC,               /* invoke parallel for function */
    KUPL_PTRACE_TAG_MAX
};

inline const char* ptrace_tag_str(kupl_ptrace_tag_t tag)
{
    switch (tag) {
        case KUPL_PTRACE_TASK_CREATE:                    return "TASK_CREATE";
        case KUPL_PTRACE_TASK_SUBMIT:                    return "TASK_SUBMIT";
        case KUPL_PTRACE_TASK_ENQUEUE:                   return "TASK_ENQUEUE";
        case KUPL_PTRACE_TASK_DEQUEUE:                   return "TASK_DEQUEUE";
        case KUPL_PTRACE_TASK_EXECUTE:                   return "TASK_EXECUTE";
        case KUPL_PTRACE_PERIOD_TASK_EXECUTE:            return "PERIOD_TASK_EXECUTE";
        case KUPL_PTRACE_TASK_RESET:                     return "TASK_RESET";
        case KUPL_PTRACE_TASK_RELEASE:                   return "TASK_RELEASE";
        case KUPL_PTRACE_TASK_FINISH:                    return "TASK_FINISH";
        case KUPL_PTRACE_TASK_INIT:                      return "TASK_INIT";
        case KUPL_PTRACE_TASK_CLEANUP:                   return "TASK_CLEANUP";
        case KUPL_PTRACE_TASK_INVOKE:                    return "TASK_INVOKE";
        case KUPL_PTRACE_TASK_WAIT:                      return "TASK_WAIT";
        case KUPL_PTRACE_TASK_IN_LOOP:                   return "TASK_IN_LOOP";
        case KUPL_PTRACE_ULT_CREATE:                     return "ULT_CREATE";
        case KUPL_PTRACE_ULT_INIT:                       return "ULT_INIT";
        case KUPL_PTRACE_ULT_CLEANUP:                    return "ULT_CLEANUP";
        case KUPL_PTRACE_ULT_EXECUTE:                    return "ULT_EXECUTE";
        case KUPL_PTRACE_ULT_FINISH:                     return "ULT_FINISH";
        case KUPL_PTRACE_ULT_INVOKE:                     return "ULT_INVOKE";
        case KUPL_PTRACE_GLOBAL_EGROUP_GET:              return "GLOBAL_EGROUP_GET";
        case KUPL_PTRACE_PARALLEL_FOR_GRAPH_GET:         return "PARALLEL_FOR_GRAPH_GET";
        case KUPL_PTRACE_PARALLEL_FOR_CHECK:             return "PARALLEL_FOR_CHECK";
        case KUPL_PTRACE_PARALLEL_FOR_NUM_THREADS:       return "PARALLEL_FOR_NUM_THREADS";
        case KUPL_PTRACE_PARALLEL_FOR_POLICY_STATIC:     return "PARALLEL_FOR_POLICY_STATIC";
        case KUPL_PTRACE_PARALLEL_FOR_POLICY_3D_STATIC:  return "PARALLEL_FOR_POLICY_3D_STATIC";
        case KUPL_PTRACE_PARALLEL_FOR:                   return "PARALLEL_FOR";
        case KUPL_PTRACE_INVOKE_PARALLEL_FUNC:           return "PARALLEL_FUNC";

        default:                                    return "TAG_UNKNOW";
    }
}

#ifdef ENABLE_KUPL_TRACE
int kupl_ptrace_init();
void kupl_ptrace_fini();
void kupl_ptrace_record(kupl_ptrace_tag_t tag, uint64_t ts1, uint64_t ts2);
uint64_t get_current_timestamp_ns();
int get_kupl_prof_level_trace();

#define KUPL_PROF_LEVEL_TRACE        !get_kupl_prof_level_trace()

#define KUPL_PTRACE_START(_tag)                                    \
    uint64_t _ts1 ## _tag = 0;                                     \
({                                                                 \
    if (KUPL_PROF_LEVEL_TRACE) {                                   \
        asm volatile("" ::: "memory");                             \
        _ts1 ## _tag = kupl_now_ns();                              \
        asm volatile("" ::: "memory");                             \
    }                                                              \
})                                                                 \

#define KUPL_PTRACE_END(_tag)                                      \
({                                                                 \
    if (KUPL_PROF_LEVEL_TRACE) {                                   \
        asm volatile("" ::: "memory");                             \
        uint64_t _ts2 ## _tag = kupl_now_ns();                     \
        asm volatile("" ::: "memory");                             \
        kupl_ptrace_record(_tag, (_ts1 ## _tag), (_ts2 ## _tag));  \
    }                                                              \
})


#define KUPL_PTRACE_COND_START(_tag)                               \
    uint64_t _ts1 ## _tag = 0;                                     \
({                                                                 \
    if (KUPL_PROF_LEVEL_TRACE) {                                   \
        asm volatile("" ::: "memory");                             \
        _ts1 ## _tag = kupl_now_ns();                              \
        asm volatile("" ::: "memory");                             \
    }                                                              \
})                                                                 \

#define KUPL_PTRACE_COND_END(_tag, _trace)                         \
({                                                                 \
    if (KUPL_PROF_LEVEL_TRACE) {                                   \
        asm volatile("" ::: "memory");                             \
        uint64_t _ts2 ## _tag = kupl_now_ns();                     \
        asm volatile("" ::: "memory");                             \
        if (_trace) {                                              \
            kupl_ptrace_record(_tag, (_ts1 ## _tag), (_ts2 ## _tag));  \
        }                                                          \
    }                                                              \
})

#else

#define kupl_ptrace_init() KUPL_OK
#define kupl_ptrace_fini()
#define kupl_ptrace_record(tag...)
#define KUPL_PTRACE_START(_tag)
#define KUPL_PTRACE_END(_tag)
#define KUPL_PTRACE_COND_START(_tag)
#define KUPL_PTRACE_COND_END(_tag, _trace)
#endif // ENABLE_KUPL_TRACE

#ifdef __cplusplus
}
#endif
#endif // KUPL_PROFILE_TRACE_H

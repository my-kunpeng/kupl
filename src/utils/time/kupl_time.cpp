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
#include "kupl_time.h"
#include <ctime>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include "kupl.h"
#include "core/kupl_core.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/debug/kupl_log.h"

#define VTC_FACTOR 1000
#define S2U_FACTOR 1000000    // 1s = 1000000us
#define S2N_FACTOR 1000000000 // 1s = 1000000000ns

static uint64_t g_nanoSecondsPerTick = 1;

/**
 * @brief ARM architecture hardware monotonic
 */
static kupl_always_inline uint64_t kupl_arm_virtual_timer_count()
{
    uint64_t counter;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(counter));
    return counter;
}

static kupl_always_inline uint32_t kupl_arm_virtual_timer_freq()
{
    union {
        uint64_t freq;
        struct {
            uint32_t low;
            uint32_t high;
        } bits;
    } counter;

    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(counter.freq));
    return counter.bits.low; /* Top 32 bits are reserved */
}

static uint64_t kupl_arm_now_ns()
{
    uint64_t count = kupl_arm_virtual_timer_count();
    return count * g_nanoSecondsPerTick;
}

void kupl_time_init()
{
    uint64_t tmp = static_cast<uint64_t>(kupl_arm_virtual_timer_freq());
    if (tmp != 0) {
        g_nanoSecondsPerTick = S2N_FACTOR / tmp;
    }
}

void kupl_time_fini() {}

uint64_t kupl_now_ns()
{
    return kupl_arm_now_ns();
}

double kupl_get_wtime()
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return 0;
    }
    return (double)kupl_arm_now_ns() / S2N_FACTOR;
}

void kupl_timestamp(char *buffer, size_t buffersize)
{
    // check the pointer
    if (kupl_unlikely(buffer == nullptr)) {
        kupl_error("fail to get buffer string");
        return;
    }
    if (kupl_unlikely(buffersize < KUPL_TIME_BUF_SIZE)) {
        kupl_error("timestamp buffer size too small");
        return;
    }

    // get current time stamp
    time_t current_time = time(NULL);
    if (kupl_likely(current_time != (time_t)(-1))) {
        // transform time_t to struct tm
        struct tm *tm_info = localtime(&current_time);
        if (kupl_likely(tm_info != nullptr)) {
            // format output date-time
            strftime(buffer, KUPL_TIME_BUF_SIZE * sizeof(char), "%Y-%m-%d_%H:%M:%S", tm_info);
        }
    }
}
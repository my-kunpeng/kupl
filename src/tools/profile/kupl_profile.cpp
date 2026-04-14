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
#include "kupl_profile.h"
#include <pthread.h>
#include <cstdio>
#include <unordered_map>
#include <cstring>
#include "utils/arch/kupl_atomic.h"
#include "utils/debug/kupl_assert.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/sys/kupl_math.h"
#include "utils/sys/kupl_glibc_version.h"
#include "utils/config/kupl_config.h"

#ifdef ENABLE_KUPL_PROFILE

#define PROFILE_ID_INIT     (-1)
struct profile_data {
    const char *msg = nullptr;
    int id = PROFILE_ID_INIT;

    uint64_t time_pointer = 0;
    uint64_t count = 0;
    uint64_t duration = 0;
    uint64_t sum = 0;
    uint64_t max = 0;
    uint64_t min = UINT64_MAX;

    uint64_t recursion = 0;

    void start(uint64_t now = 0)
    {
        time_pointer = (now == 0 ? profile_now_ns() : now);
        recursion++;
    }

    void end(uint64_t now = 0)
    {
        if (time_pointer == 0) {
            return;
        }
        duration = (now == 0 ? profile_now_ns() : now) - time_pointer;
        sum += duration;
        count++;
        max = duration > max ? duration : max;
        min = duration < min ? duration : min;

        recursion--;
    }

    void add(uint64_t dur)
    {
        if (dur == 0) {
            return;
        }
        duration = dur;
        sum += duration;
        count++;
        max = duration > max ? duration : max;
        min = duration < min ? duration : min;
    }

    void reset()
    {
        msg = nullptr;
        id = PROFILE_ID_INIT;
        time_pointer = 0;
        count = 0;
        duration = 0;
        sum = 0;
        max = 0;
        min = UINT64_MAX;
    }
};

struct stats_data_value {
    uint64_t user_key = 0;
    const char *file = nullptr;
    const char *func = nullptr;
    const char *msg = nullptr;
    uint64_t count = 0;

    stats_data_value(uint64_t k, const char *fl, const char *fu, const char *m, uint64_t c)
        : user_key(k), file(fl), func(fu), msg(m), count(c)
    {
    }

    stats_data_value() = default;
};

static profile_data g_data[MAX_THREAD_NUM][MAX_PROFILE_ID];
static int g_profile_id_seq = 0;
static const char* g_profile_id_name[MAX_PROFILE_ID];
static std::unordered_map<uint64_t, stats_data_value> g_profile_stats[MAX_THREAD_NUM];
static int kupl_prof_level_profile;

int get_kupl_prof_level_profile()
{
    return kupl_prof_level_profile;
}

KUPL_ATOMIC_INT g_thread_id;
int profile_get_threadid()
{
    static thread_local int id = PROFILE_ID_INIT;
    if (id != PROFILE_ID_INIT) {
        return id;
    }

    id = KUPL_ATOMIC_ADD(&g_thread_id, 1);
    return id;
}

int profile_register_id(const char *name)
{
    auto id = g_profile_id_seq++;
    kupl_assertv(id >= MAX_PROFILE_ID, "The profile id %d is too big, please change macro MAX_PROFILE_ID which is %d",
        id, MAX_PROFILE_ID);
    if (id >= MAX_PROFILE_ID) {
        return PROFILE_ID_INIT;
    }

    g_profile_id_name[id] = strdup(name);
    return id;
}

static pthread_spinlock_t lock;
int profile_register_id_once(int *id, const char *name)
{
    if (kupl_likely(*id >= 0)) {
        return *id;
    }

    pthread_spin_lock(&lock);
    if (*id < 0) {
        *id = profile_register_id(name);
    }
    pthread_spin_unlock(&lock);
    return *id;
}

void profile_start(int id)
{
    if (kupl_unlikely(id < 0 || id >= MAX_PROFILE_ID)) {
        return;
    }
    auto now = profile_now_ns();
    auto data = &g_data[profile_get_threadid()][id];
    if (data->msg == nullptr) {
        data->msg = strdup(g_profile_id_name[id]);
    }

    data->start(now);
}

void profile_end(int id)
{
    if (kupl_unlikely(id < 0 || id >= MAX_PROFILE_ID)) {
        return;
    }
    auto now = profile_now_ns();
    auto data = &g_data[profile_get_threadid()][id];
    data->end(now);
}

void profile_clone(int src_id, int dst_id)
{
    if (kupl_unlikely(src_id < 0 || src_id >= MAX_PROFILE_ID || dst_id < 0 || dst_id >= MAX_PROFILE_ID)) {
        return;
    }
    auto src_data = &g_data[profile_get_threadid()][src_id];
    auto dst_data = &g_data[profile_get_threadid()][dst_id];
    if (dst_data->msg == nullptr) {
        dst_data->msg = g_profile_id_name[dst_id];
    }
    dst_data->add(src_data->duration);
}

void profile_cancel(int id)
{
    if (kupl_unlikely(id < 0 || id >= MAX_PROFILE_ID)) {
        return;
    }
    auto data = &g_data[profile_get_threadid()][id];
    data->msg = nullptr;
}

void profile_reset()
{
    for (int t = 0; t < MAX_THREAD_NUM; ++t) {
        for (int id = 0; id < MAX_PROFILE_ID; ++id) {
            auto data = &g_data[t][id];
            data->reset();
        }
        g_profile_stats[t].clear();
    }
}

void profile_summary(int max_tid, int max_id)
{
    const static uint64_t NS2MS_FACTOR = 1000000;
    max_tid = (max_tid == -1) ? (int)(KUPL_ATOMIC_LD(&g_thread_id)) : max_tid;
    max_id = (max_id == -1) ? g_profile_id_seq : max_id;

    printf("============== start profile =============\n");
    printf("%10s %7s %30s %18s %12s "
           "%15s %15s %15s\n",
           "thread_id", "msg_id", "msg", "sum(ms)", "count",
           "avg(ns)", "max(ns)", "min(ns)");
    for (int t = 0; t < max_tid; ++t) {
        for (int id = 0; id < max_id; ++id) {
            auto data = &g_data[t][id];
            if (data->msg == nullptr) {
                continue;
            }
            printf("%10d %7d %30s %18.6lf %12lu "   // thread id msg sum count
                   "%15lu %15lu %15lu\n",         // avg max min
                   t, id, data->msg, (double)(data->sum/NS2MS_FACTOR), data->count,
                   data->sum / kupl_max(1, data->count), data->max, kupl_min(data->min, data->min + 1)
                   );
        }
    }

    printf("\n %6s   %10s %30s %10s\n", "thread", "key", "name", "count");
    for (int t = 0; t < max_tid; ++t) {
        for (auto &kv : g_profile_stats[t]) {
            printf(" %6d 0x%010lx %30s %10lu\n", t, kv.second.user_key, kv.second.msg, kv.second.count);
        }
    }
    printf("============== end profile =============\n");
}

void profile_stats_record(uint64_t key, const char *file, const char *func, int line, const char *msg)
{
    auto stats = &g_profile_stats[profile_get_threadid()];
    uint64_t inner_key = (uint64_t)file + (uint64_t)func + (((uint64_t)line) << 5) + key;
    auto it = stats->find(inner_key);
    if (it != stats->end()) {
        kupl_assert(it->second.user_key == key);
        kupl_assert(it->second.msg == msg);
        kupl_assert(it->second.func == func);
        kupl_assert(it->second.file == file);

        it->second.count++;
        return;
    }

    (*stats)[inner_key] = stats_data_value(key, file, func, msg, 1);
    return;
}

void profile_module_init()
{
    kupl_prof_level_profile = strcmp(kupl_config_get_value_str(KUPL_PROF_LEVEL), "statistic") &
                              strcmp(kupl_config_get_value_str(KUPL_PROF_LEVEL), "trace");
    if (kupl_prof_level_profile != 0) {
        return;
    }
    pthread_spin_init(&lock, 0);
}

void profile_module_fini()
{
    if (kupl_prof_level_profile != 0) {
        return;
    }
    profile_summary(-1, -1);
    pthread_spin_destroy(&lock);

    int max_tid = (int)g_thread_id.load();
    int max_id = g_profile_id_seq;
    for (int t = 0; t < max_tid; ++t) {
        for (int id = 0; id < max_id; ++id) {
            auto data = &g_data[t][id];
            if (data->id != PROFILE_ID_INIT && data->msg != nullptr) {
                free((void *)(const_cast<char *>(data->msg)));
                data->msg = nullptr;
            }
            if (g_profile_id_name[id] != nullptr) {
                free(const_cast<char *>(g_profile_id_name[id]));
                g_profile_id_name[id] = nullptr;
            }
        }
    }
}

#endif // ENABLE_KUPL_PROFILE
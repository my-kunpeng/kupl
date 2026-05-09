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
#include "kupl_sched.h"
#include <unistd.h>
#include "plugin/kupl_sched_plugin_load.h"
#include "executor/backend/kupl_executor_backend.h"
#include "executor/kupl_executor.h"
#include "utils/arch/kupl_atomic.h"
#include "utils/config/kupl_config.h"
#include "tools/profile/kupl_profile.h"
#include "utils/sys/kupl_hardware.h"

typedef struct kupl_sched {
    KUPL_ATOMIC_INT ref;
    KUPL_ATOMIC_BOOL finished;
    void *plugin_sched;
    kupl_sched_plugin_api_t plugin;
    kupl_sched_plugin_property_t plugin_property;
} kupl_sched_t;

static bool g_sched_inited = false;
static kupl_sched_t *g_sched;
static kupl_sched_t *g_sched_expand = nullptr;
static thread_local int g_sched_yield_count = 0;
static int g_sched_use_yield, g_sched_init_yield_count;

int kupl_sched_init()
{
    if (kupl_unlikely(kupl_sched_plugin_load() != KUPL_OK)) {
        return kupl_log_error_return(ERROR, "Load sched plugin failed");
    }
    std::string sched_policy = kupl_config_get_value_str(KUPL_SCHED_POLICY);
    if (sched_policy == "sspe") {
        g_sched = kupl_sched_create("sspe");
    } else {
        g_sched = kupl_sched_create("hybrid");
    }

    if (kupl_unlikely(g_sched == nullptr)) {
        kupl_sched_plugin_unload();
        return kupl_log_error_return(ERROR, "Create sched failed");
    }

    g_sched_use_yield = kupl_config_get_value(KUPL_ENABLE_YIELD);
    g_sched_init_yield_count = kupl_config_get_value(KUPL_YIELD_COUNT);

    g_sched_inited = true;
    return KUPL_OK;
}

int kupl_sched_expand()
{
    if (g_sched_expand == nullptr) {
        g_sched_expand = kupl_sched_create("expand_static_mq");
        if (kupl_unlikely(g_sched_expand == nullptr)) {
            kupl_sched_plugin_unload();
            return kupl_log_error_return(ERROR, "Create sched failed");
        }
    } else {
        g_sched_expand->plugin_sched = g_sched_expand->plugin.expand(g_sched_expand->plugin_sched);
        if (kupl_unlikely(g_sched_expand->plugin_sched == nullptr)) {
            kupl_safe_free(g_sched_expand);
            g_sched_expand = nullptr;
            return KUPL_ERROR;
        }
    }

    return KUPL_OK;
}

void kupl_sched_expand_fini()
{
    kupl_sched_cleanup(g_sched_expand);
    g_sched_expand = nullptr;
    return;
}

void kupl_sched_fini()
{
    kupl_trace("scheduler_finalize\n");
    kupl_sched_cleanup(g_sched);
    g_sched = nullptr;
    kupl_sched_cleanup(g_sched_expand);
    g_sched_expand = nullptr;
    kupl_sched_plugin_unload();
    return;
}

kupl_sched_t *kupl_get_global_sched()
{
    return g_sched;
}

kupl_sched_t *kupl_get_global_sched_expand()
{
    return g_sched_expand;
}

kupl_sched_t *kupl_sched_create(const char *plugin_name)
{
    kupl_sched_t *sched = (kupl_sched_t *)kupl_calloc(1, sizeof(kupl_sched_t));
    if (kupl_unlikely(sched == nullptr)) {
        return nullptr;
    }

    /** sched plugin initialize */
    if (kupl_unlikely(kupl_sched_plugin_find(plugin_name, &sched->plugin, &sched->plugin_property) != KUPL_OK)) {
        goto err_free;
    }

    sched->plugin_sched = sched->plugin.create();
    if (kupl_unlikely(sched->plugin_sched == nullptr)) {
        goto err_free;
    }

    sched->ref = 1;
    sched->finished = false;

    return sched;

err_free:
    kupl_safe_free(sched);
    return nullptr;
}

void kupl_sched_cleanup(kupl_sched_t *sched)
{
    if (kupl_unlikely(sched == nullptr)) {
        return;
    }

    sched->finished = true;
    kupl_backend_type_t backend_type = kupl_backend_type_get();
    if (backend_type == KUPL_BACKEND_PTHREAD) {
        KUPL_ATOMIC_SUB(&sched->ref, 1);
        while (KUPL_ATOMIC_LD(&sched->ref) != 0) {}
    } else if (backend_type == KUPL_BACKEND_OMP) {
        if (KUPL_ATOMIC_SUB(&sched->ref, 1) > 1) {
            return;
        }
    }

    if (sched->plugin_sched != nullptr) {
        sched->plugin.cleanup(sched->plugin_sched);
    }

    kupl_safe_free(sched);
}

static thread_local bool sched_full = false;

/**
 * @brief add taskbase to sched
 *
 * @return KUPL_OK, taskbase add to sched success or executed immediately
 * @return KUPL_ERROR, taskbase add to sched failed
 *
 */
void kupl_sched_add_tb(kupl_sched_t *sched, kupl_taskbase_t *tb)
{
    int status;

    KUPL_ATOMIC_OR_RLS(&tb->state, KUPL_TB_STATE_READY);
    kupl_tb_count_add(tb);

    if (sched_full || tb->flag == KUPL_TB_FLAG_IMM) {
        goto invoke;
    }

    PROFILE_CODE_START(sched_add_taskbase);
    status = sched->plugin.add_tb(sched->plugin_sched, tb);
    PROFILE_CODE_END(sched_add_taskbase);

    if (kupl_unlikely(status != KUPL_OK)) {
        sched_full = true;
        goto invoke;
    }
    return;
invoke:
    tb->ops->invoke(tb);
    return;
}

int kupl_sched_add_ult(kupl_sched_t *sched, kupl_ult_t *ult)
{
    return sched->plugin.add_tb(sched->plugin_sched, &ult->tb);
}

int kupl_sched_execute_tb(kupl_sched_t *sched)
{
    static kupl_compute_place_t cp;
    PROFILE_CODE_COND_START(sched_get_taskbase, (tb != nullptr));
    kupl_taskbase_t *tb = sched->plugin.get_tb(sched->plugin_sched, cp);
    PROFILE_CODE_COND_END(sched_get_taskbase, (tb != nullptr));
    if (tb == nullptr) {
        kupl_sched_yield();
        return 0;
    }
    sched_full = false;
    return tb->ops->invoke(tb);
}

void kupl_sched_yield()
{
    if (g_sched_use_yield == 1) {
        g_sched_yield_count--;
        if (g_sched_yield_count <= 0) {
            sched_yield();
            g_sched_yield_count = g_sched_init_yield_count;
        }
    }
}
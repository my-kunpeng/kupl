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
#include "kupl_sched_hybrid.h"
#include <string>
#include "mt/scheduler/plugin/kupl_sched_plugin_api.h"
#include "mt/scheduler/plugin/kupl_sched_plugin_load.h"
#include "utils/config/kupl_config.h"
#include "utils/lock/kupl_lock.h"
#include "utils/arch/kupl_atomic.h"

static int KUPL_SCHED_PLUGINS_SELECT_NUM = 2;
static int KUPL_SCHED_PLUGIN_HYBRID_OUTER_TB_SCHED_IDX = 0;
static int KUPL_SCHED_PLUGIN_HYBRID_INNER_TB_SCHED_IDX = 1;

typedef struct {
    void *plugin_sched;
    kupl_sched_plugin_api_t plugin;
    kupl_sched_plugin_property_t plugin_property;
    KUPL_ATOMIC_BOOL plugin_using;
} kupl_sched_hybrid_plugin_t;

typedef struct {
    kupl_sched_hybrid_plugin_t hybrid_plugin[KUPL_SCHED_PLUGINS_NUM];
} kupl_sched_hybrid_t;

static int kupl_sched_hybrid_init(kupl_sched_plugin_property_t *property)
{
    property->name = KUPL_SCHED_PLUGIN_HYBRID_NAME;
    property->private_data_len = 0;
    property->score = KUPL_SCHED_PLUGIN_HYBRID_SCORE;

    return KUPL_OK;
}

static void kupl_sched_hybrid_fini()
{
}

static void kupl_sched_hybrid_cleanup(void *sched);

static kupl_always_inline
void kupl_sched_hybrid_get_plugin(const char *plugin_name, kupl_sched_hybrid_t *sched_hybrid, int level)
{
    if (kupl_unlikely(kupl_sched_plugin_find(plugin_name, &sched_hybrid->hybrid_plugin[level].plugin,
        &sched_hybrid->hybrid_plugin[level].plugin_property) != KUPL_OK)) {
        if (!level) {
            kupl_warn("invalid env: KUPL_SCHED_POLICY,"
                " plugin: %s not found. so select default plugin sspe for outer level.", plugin_name);
            kupl_sched_plugin_find("sspe", &sched_hybrid->hybrid_plugin[level].plugin,
                                    &sched_hybrid->hybrid_plugin[level].plugin_property);
        } else {
            kupl_warn("invalid env: KUPL_SCHED_POLICY,"
                " plugin: %s not found. so select default plugin static_mq for inner level.", plugin_name);
            kupl_sched_plugin_find("static_mq", &sched_hybrid->hybrid_plugin[level].plugin,
                                    &sched_hybrid->hybrid_plugin[level].plugin_property);
        }
    }
}

static void* kupl_sched_hybrid_create()
{
    kupl_sched_hybrid_t *sched_hybrid = (kupl_sched_hybrid_t *)kupl_calloc(1, sizeof(kupl_sched_hybrid_t));
    if (sched_hybrid == nullptr) {
        kupl_error("sched_hybrid alloc failed");
        return nullptr;
    }
    std::string sched_policy = kupl_config_get_value_str(KUPL_SCHED_POLICY);
    std::string sched_policy_outer = "sspe";
    std::string sched_policy_inner = "static_mq";
    if (sched_policy.length() > 0 && sched_policy != sched_policy_outer) {
        sched_policy_inner = sched_policy;
    }

    kupl_sched_hybrid_get_plugin(sched_policy_outer.c_str(), sched_hybrid, 0);
    kupl_sched_hybrid_get_plugin(sched_policy_inner.c_str(), sched_hybrid, 1);

    for (int i = 0; i < KUPL_SCHED_PLUGINS_SELECT_NUM; i++) {
        sched_hybrid->hybrid_plugin[i].plugin_using = false;
        sched_hybrid->hybrid_plugin[i].plugin_sched = sched_hybrid->hybrid_plugin[i].plugin.create();
        if (kupl_unlikely(sched_hybrid->hybrid_plugin[i].plugin_sched == nullptr)) {
            kupl_sched_hybrid_cleanup(sched_hybrid);
            return nullptr;
        }
    }
    return sched_hybrid;
}

static void kupl_sched_hybrid_cleanup(void *sched)
{
    if (sched == nullptr) {
        return;
    }
    kupl_sched_hybrid_t *sched_hybrid = (kupl_sched_hybrid_t *)sched;
    for (int i = 0; i < KUPL_SCHED_PLUGINS_SELECT_NUM; i++) {
        sched_hybrid->hybrid_plugin[i].plugin.cleanup(sched_hybrid->hybrid_plugin[i].plugin_sched);
    }

    kupl_safe_free(sched_hybrid);
    return;
}

static int kupl_sched_hybrid_add_tb(void *sched, kupl_taskbase_t *tb)
{
    kupl_sched_hybrid_t *sched_hybrid = (kupl_sched_hybrid_t *)sched;
    int plugin_idx;
    if (tb->flag & KUPL_TB_FLAG_HYBRID_OUTER) {
        plugin_idx = KUPL_SCHED_PLUGIN_HYBRID_OUTER_TB_SCHED_IDX;
    } else {
        plugin_idx = KUPL_SCHED_PLUGIN_HYBRID_INNER_TB_SCHED_IDX;
    }
    if (!KUPL_ATOMIC_LD_RLX(&sched_hybrid->hybrid_plugin[plugin_idx].plugin_using)) {
        sched_hybrid->hybrid_plugin[plugin_idx].plugin_using = true;
    }
    return sched_hybrid->hybrid_plugin[plugin_idx].plugin.
           add_tb(sched_hybrid->hybrid_plugin[plugin_idx].plugin_sched, tb);
}

static kupl_taskbase_t *kupl_sched_hybrid_get_tb(void *sched, kupl_compute_place_t cp)
{
    kupl_sched_hybrid_t *sched_hybrid = (kupl_sched_hybrid_t *)sched;
    kupl_taskbase_t *tb = nullptr;
    for (int i = 0; i < KUPL_SCHED_PLUGINS_SELECT_NUM; i++) {
        if (KUPL_ATOMIC_LD_RLX(&sched_hybrid->hybrid_plugin[i].plugin_using)) {
            tb = sched_hybrid->hybrid_plugin[i].plugin.get_tb(sched_hybrid->hybrid_plugin[i].plugin_sched, cp);
            if (tb != nullptr) {
                break;
            }
        }
    }
    return tb;
}

static const kupl_sched_plugin_api_t KUPL_SCHED_PLUGIN_GLOBAL_VAR(hybrid) = {
    .name = KUPL_SCHED_PLUGIN_HYBRID_NAME,
    .init = kupl_sched_hybrid_init,
    .fini = kupl_sched_hybrid_fini,
    .create = kupl_sched_hybrid_create,
    .expand = nullptr,
    .cleanup = kupl_sched_hybrid_cleanup,
    .add_tb = kupl_sched_hybrid_add_tb,
    .get_tb = kupl_sched_hybrid_get_tb,
};

const kupl_sched_plugin_api_t* kupl_sched_hybrid_get_instance()
{
    return &KUPL_SCHED_PLUGIN_GLOBAL_VAR(hybrid);
}
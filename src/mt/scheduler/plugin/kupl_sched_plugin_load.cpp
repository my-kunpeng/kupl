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
#include "kupl_sched_plugin_load.h"
#include <glob.h>
#include <string>
#include "sspe/kupl_sched_sspe.h"
#include "hybrid/kupl_sched_hybrid.h"
#include "mq/kupl_sched_mq.h"
#include "static_mq/kupl_sched_static_mq.h"
#include "utils/config/kupl_config.h"
#include "utils/debug/kupl_assert.h"
#include "utils/sys/kupl_dl_module.h"
#include "utils/sys/kupl_glibc_version.h"

#define KUPL_SCHED_PLUGIN_DEFAULT (-1)
#define KUPL_MAX_SCHED_PLUGIN_LOADED 128

typedef struct kupl_sched_plugin_info {
    int plugin_count;
    int best_score_idx;
    kupl_dl_module_t *modules[KUPL_MAX_SCHED_PLUGIN_LOADED];
    const kupl_sched_plugin_api_t *plugins[KUPL_MAX_SCHED_PLUGIN_LOADED];
    kupl_sched_plugin_property_t properties[KUPL_MAX_SCHED_PLUGIN_LOADED];
} kupl_sched_plugin_info_t;

static kupl_sched_plugin_info_t *g_info;

static int kupl_sched_plugin_load_builtin(const std::string &plugin_name)
{
    kupl_sched_plugin_property_t property;
    const kupl_sched_plugin_api_t *plugin = nullptr;
    /* load builtin mq scheduler */
    if (plugin_name == "mq") {
        plugin = kupl_sched_mq_get_instance();
    } else if (plugin_name == "sspe") {
        plugin = kupl_sched_sspe_get_instance();
    } else if (plugin_name == "hybrid") {
        plugin = kupl_sched_hybrid_get_instance();
    } else if (plugin_name == "static_mq") {
        plugin = kupl_sched_static_mq_get_instance();
    } else if (plugin_name == "expand_static_mq") {
        plugin = kupl_sched_expand_static_mq_get_instance();
    }

    if (kupl_unlikely(plugin == nullptr || plugin->init(&property) != KUPL_OK)) {
        return kupl_log_error_return(ERROR, "load builtin plugin failed");
    }

    int now_plugin_idx = g_info->plugin_count;
    g_info->modules[now_plugin_idx] = nullptr; /* builtin don't use dlopen */
    g_info->plugins[now_plugin_idx] = plugin;
    g_info->properties[now_plugin_idx] = property;

    kupl_info("load builtin sched plugin %s", property.name);
    ++g_info->plugin_count;
    return KUPL_OK;
}

int kupl_sched_plugin_load()
{
    if (g_info == nullptr) {
        g_info = (kupl_sched_plugin_info_t *)kupl_malloc_inner(sizeof(kupl_sched_plugin_info_t));
        if (g_info == nullptr) {
            return kupl_log_error_return(ERROR, "g_info malloc failed");
        }
    }
    g_info->plugin_count = 0;
    g_info->best_score_idx = KUPL_SCHED_PLUGIN_DEFAULT;
    /* load kupl default plugin */
    kupl_sched_plugin_load_builtin("sspe");
    kupl_sched_plugin_load_builtin("mq");
    kupl_sched_plugin_load_builtin("static_mq");
    kupl_sched_plugin_load_builtin("expand_static_mq");
    kupl_sched_plugin_load_builtin("hybrid");

    if (g_info->plugin_count == 0) {
        kupl_sched_plugin_unload();
        return kupl_log_error_return(ERROR, "No valid sched plugin is found");
    }

    int best_score_idx = KUPL_SCHED_PLUGIN_DEFAULT;
    std::string user_select_plugin = kupl_config_get_value_str(KUPL_SCHED_POLICY);
    if (user_select_plugin != "") {
        for (int i = 0; i < g_info->plugin_count; ++i) {
            if (strcmp(user_select_plugin.c_str(), g_info->properties[i].name) == 0) {
                kupl_info("user select sched plugin: %s", user_select_plugin.c_str());
                best_score_idx = i;
            }
        }
    }

    /* get best plugin from score */
    if (best_score_idx < 0) {
        best_score_idx = 0;
        for (int i = 1; i < g_info->plugin_count; ++i) {
            if (g_info->properties[i].score >= g_info->properties[best_score_idx].score) {
                best_score_idx = i;
            }
        }
    }
    g_info->best_score_idx = best_score_idx;
    return KUPL_OK;
}

void kupl_sched_plugin_unload()
{
    if (g_info == nullptr) {
        return;
    }
    for (int i = 0; i < g_info->plugin_count; ++i) {
        auto plugin = g_info->plugins[i];
        if (plugin != nullptr && plugin->fini != nullptr) {
            plugin->fini();
            g_info->plugins[i] = nullptr;
        }

        if (g_info->modules[i] != nullptr) {
            kupl_dl_close(g_info->modules[i]);
            g_info->modules[i] = nullptr;
        }
    }

    kupl_safe_free(g_info);
    g_info = nullptr;
}

int kupl_sched_plugin_find(const char *plugin_name, kupl_sched_plugin_api_t *plugin,
                           kupl_sched_plugin_property_t *plugin_property)
{
    if (g_info == nullptr || g_info->best_score_idx < 0) {
        return KUPL_ERROR;
    }

    if (plugin_name == nullptr) {
        *plugin = *g_info->plugins[g_info->best_score_idx];
        *plugin_property = g_info->properties[g_info->best_score_idx];
        kupl_info("select best score sched plugin %s", plugin->name);
        return KUPL_OK;
    }

    for (int i = 0; i < g_info->plugin_count; ++i) {
        if (strcmp(plugin_name, g_info->plugins[i]->name) == 0) {
            *plugin = *g_info->plugins[i];
            *plugin_property = g_info->properties[i];
            kupl_info("select user's sched plugin %s", plugin->name);
            return KUPL_OK;
        }
    }

    return KUPL_ERROR;
}
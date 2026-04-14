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
#ifndef KUPL_SCHED_PLUGIN_LOAD_H
#define KUPL_SCHED_PLUGIN_LOAD_H

#include "kupl_sched_plugin_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load all built-in plugins and load all dynamic library plugins, we will initialize
 *        all plugins and return the highest-score's plugin.
 * @note  When some plugin have same score, will return the last loaded plugin.
 *
 * @param [out] plugin          the highest score plugin
 * @param [out] plugin_property the property of selected plugin
 * @return                      KUPL_OK for success, other for failed
 */
int kupl_sched_plugin_load(void);

/**
 * @brief Unload all plugins, after this routine user
 * can't call @ref plugin's API from kupl_sched_plugin_load() anymore.
 */
void kupl_sched_plugin_unload(void);

/**
 * @brief Find the plugin by @a plugin_name
 * @note  When name is nullptr it will return the highest score plugin
 *
 * @param [in] plugin_name          the plugin name want find, it can be NULL for find default plugin
 * @param [out] plugin              the selected plugin
 * @param [out] plugin_property     the property of selected plugin
 *
 * @return  KUPL_OK for success, other for failed
 */
int kupl_sched_plugin_find(const char *plugin_name,
                           kupl_sched_plugin_api_t *plugin,
                           kupl_sched_plugin_property_t *plugin_property);

#ifdef __cplusplus
}
#endif

#endif
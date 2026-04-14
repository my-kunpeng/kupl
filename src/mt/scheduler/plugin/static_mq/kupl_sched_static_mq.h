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
#ifndef KUPL_SCHEDULER_STATIC_MQ_H
#define KUPL_SCHEDULER_STATIC_MQ_H

#include "mt/scheduler/plugin/kupl_sched_plugin_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KUPL_SCHED_PLUGIN_STATIC_MQ_NAME       "static_mq"
#define KUPL_SCHED_PLUGIN_EXPAND_STATIC_MQ_NAME       "expand_static_mq"
#define KUPL_SCHED_PLUGIN_STATIC_MQ_SCORE      (-1)

const kupl_sched_plugin_api_t* kupl_sched_static_mq_get_instance();
const kupl_sched_plugin_api_t* kupl_sched_expand_static_mq_get_instance();

#ifdef __cplusplus
}
#endif

#endif
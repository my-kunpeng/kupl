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
#include "kupl_utils.h"

bool g_utils_inited = false;

int kupl_utils_init()
{
    kupl_config_load();
    kupl_time_init();
    if (kupl_unlikely(kupl_host_info_init() != KUPL_OK)) {
        goto err_host_info_init;
    }
    g_utils_inited = true;
    return KUPL_OK;

err_host_info_init:
    kupl_time_fini();
    kupl_config_unload();
    return KUPL_ERROR;
}

void kupl_utils_fini()
{
    kupl_host_info_fini();
    kupl_time_fini();
    kupl_config_unload();
    g_utils_inited = false;
}

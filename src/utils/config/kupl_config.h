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
#ifndef KUPL_CONFIG_H
#define KUPL_CONFIG_H

#include <pthread.h>
#include <sched.h>
#include "utils/sys/kupl_glibc_version.h"

#define KUPL_CONFIG_TO_ENUM(_env, _type)  _env ## _ ## _type ## _ENUM

/** define the enum of int/str type enum configure */
#define KUPL_CONFIG_INT(_env, ...) KUPL_CONFIG_TO_ENUM(_env, INT),
#define KUPL_CONFIG_STR(_env, ...) KUPL_CONFIG_TO_ENUM(_env, STR),
enum kupl_config_enum {
    KUPL_CONFIG_ENUM_FIRST = -1,
    #include "kupl_config_var.inc"
    KUPL_CONFIG_ENUM_LAST
};
#undef KUPL_CONFIG_INT
#undef KUPL_CONFIG_STR

int kupl_config_int_type_value(kupl_config_enum env);
const char *kupl_config_str_type_value(kupl_config_enum env);

/**
 * @brief get the value of configure @ref _env
 *
 * @param _env      the name of this configure in environment
 */
#define kupl_config_get_value(_env)        kupl_config_int_type_value(KUPL_CONFIG_TO_ENUM(_env, INT))
#define kupl_config_get_value_str(_env)    kupl_config_str_type_value(KUPL_CONFIG_TO_ENUM(_env, STR))

/**
 * @brief load all default config or read config from environment
 *
 * @note this function call call more than one times, and will read newer environment
 */
void kupl_config_load(void);
void kupl_config_unload(void);

#endif
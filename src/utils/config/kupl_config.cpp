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
#include "kupl_config.h"
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <cstdio>
#include <cerrno>
#include "utils/sys/kupl_compiler.h"

enum kupl_config_type {
    KUPL_CONFIG_TYPE_INT,
    KUPL_CONFIG_TYPE_STR,
};

struct kupl_config_kv {
    enum kupl_config_type type;
    const char *name;
    const char *doc;
    union {
        struct {
            int value;
            int def_value;
            int lower;
            int upper;
        } cfg_int;

        struct {
            const char *value;
            const char *def_value;
        } cfg_str;
    };
};

#define KUPL_CONFIG_COUNT ((int)KUPL_CONFIG_ENUM_LAST)

static kupl_config_kv g_kupl_config_kv[KUPL_CONFIG_COUNT];

/* int, the valid range is [lower, upper] */
static void get_env_int(struct kupl_config_kv *cfg)
{
    auto key = cfg->name;
    auto value = &cfg->cfg_int.value;
    auto upper = cfg->cfg_int.upper;
    auto lower = cfg->cfg_int.lower;

    auto v_str = getenv(key);
    if (v_str == nullptr) {
        return;
    }

    char *end = nullptr;
    auto v_int = std::strtol(v_str, &end, KUPL_BASE_DEC);
    if (end == nullptr || end == v_str || *end != '\0' || errno == ERANGE) {
        printf("%s=%s is invalid, so use default %d\n", key, v_str, *value);
        return;
    }

    if (v_int > upper || v_int < lower) {
        printf("%s=%s is invalid, so use default %d\n", key, v_str, *value);
        return;
    }

    *value = (int)v_int;
    return;
}

static void get_env_str(struct kupl_config_kv *cfg)
{
    auto key = cfg->name;
    auto value = &cfg->cfg_str.value;

    auto v_str = getenv(key);
    if (v_str == nullptr) {
        return;
    }

    /* It has a old value need to free */
    if ((*value) != nullptr && (*value) != cfg->cfg_str.def_value) {
        free(const_cast<char *>(*value));
    }

    *value = strdup(v_str);
    if (*value == nullptr) {
        *value = cfg->cfg_str.def_value;
    }
    return;
}

void kupl_config_load()
{
    /** initialize all the configures with default value and read from environment */
    #define KUPL_CONFIG_INT(_env, _def, _lower, _upper, _doc)                                  \
            g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, INT)].type = KUPL_CONFIG_TYPE_INT;    \
            g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, INT)].name = # _env;                   \
            g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, INT)].doc = _doc;                      \
            g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, INT)].cfg_int.value = _def;            \
            g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, INT)].cfg_int.def_value = _def;        \
            g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, INT)].cfg_int.upper = _upper;          \
            g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, INT)].cfg_int.lower = _lower;          \
            get_env_int(&g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, INT)]);

    #define KUPL_CONFIG_STR(_env, _def, _doc)                                                  \
            g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, STR)].type = KUPL_CONFIG_TYPE_STR;    \
            g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, STR)].name = # _env;                   \
            g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, STR)].doc = _doc;                      \
            g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, STR)].cfg_str.value = _def;            \
            g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, STR)].cfg_str.def_value = _def;        \
            get_env_str(&g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, STR)]);

    #include "kupl_config_var.inc"
    #undef KUPL_CONFIG_INT
    #undef KUPL_CONFIG_STR

    /* print all configures */
    if (kupl_config_get_value(KUPL_ENABLE_VERBOSE)) {
        #define KUPL_CONFIG_INT(_env, ...) printf("%s=%d\n",   \
                                                    g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, INT)].name,    \
                                                    g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, INT)].cfg_int.value);
        #define KUPL_CONFIG_STR(_env, ...) printf("%s=%s\n",   \
                                                    g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, STR)].name,    \
                                                    g_kupl_config_kv[KUPL_CONFIG_TO_ENUM(_env, STR)].cfg_str.value);
        #include "kupl_config_var.inc"
        #undef KUPL_CONFIG_INT
        #undef KUPL_CONFIG_STR
    }
}

void kupl_config_unload()
{
    for (int i = 0; i < KUPL_CONFIG_COUNT; ++i) {
        auto cfg = &g_kupl_config_kv[i];
        if (cfg->type == KUPL_CONFIG_TYPE_STR && cfg->cfg_str.value != cfg->cfg_str.def_value) {
            free(const_cast<char *>(cfg->cfg_str.value));
            cfg->cfg_str.value = cfg->cfg_str.def_value;
        }
    }
}

int kupl_config_int_type_value(kupl_config_enum env)
{
    return g_kupl_config_kv[env].cfg_int.value;
}

const char *kupl_config_str_type_value(kupl_config_enum env)
{
    return g_kupl_config_kv[env].cfg_str.value;
}
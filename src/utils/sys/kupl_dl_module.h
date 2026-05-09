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
#ifndef KUPL_DL_MODULE_H
#define KUPL_DL_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kupl_dl_module {
    void *handle;
} kupl_dl_module_t;

typedef struct kupl_dl_module_sym {
    void *sym;
    const char *sym_name;
} kupl_dl_module_sym_t;

/**
 * @brief Get the default library path
 *
 * @return Returns the path of the current dynamic library exist path
 */
const char *kupl_dl_get_default_path(void);

/**
 * @brief Dlopen the module, and load all functions to @a funcs
 *
 * @param libpath       the path of library module you want to dlopen()
 * @param syms          all functions or variables will use dlsym() to get
 * @param syms_count    the count in syms array
 *
 * @return              the handle of this libray, return NULL if error happened
 */
kupl_dl_module_t *kupl_dl_open(const char *libpath, kupl_dl_module_sym_t *syms, int sym_count);

/**
 * @brief Dlclose the module
 */
void kupl_dl_close(kupl_dl_module_t *module);

#ifdef __cplusplus
}
#endif
#endif
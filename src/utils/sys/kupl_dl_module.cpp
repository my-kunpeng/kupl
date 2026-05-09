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
#include "kupl_dl_module.h"
#include <cstring>
#include <dlfcn.h>
#include <libgen.h>
#include <climits>
#include <string>
#include "kupl.h"
#include "kupl_compiler.h"
#include "memory/mpool/kupl_mpool.h"
#include "utils/debug/kupl_log.h"
#include "utils/sys/kupl_glibc_version.h"

static int g_dl_self_found_var; /* just for dladdr() to find this library's path */
static char g_dl_default_path[KUPL_PATH_MAX];

const char *kupl_dl_get_default_path()
{
    if (g_dl_default_path[0] != '\0') {
        return g_dl_default_path;
    }

    dlerror();
    Dl_info dl_info;
    int ret = dladdr((void *)&g_dl_self_found_var, &dl_info);
    if (ret == 0 || dl_info.dli_fname == nullptr) {
        const char *error = dlerror();
        if (error) {
            kupl_error("Failed to dladdr, %s", error);
        }
        return nullptr;
    }

    std::string lib_path(dl_info.dli_fname);
    char *dir = dirname(const_cast<char *>(lib_path.c_str()));
    if (kupl_unlikely(dir == nullptr || strlen(dir) >= KUPL_PATH_MAX)) {
        return nullptr;
    }

    memcpy(g_dl_default_path, dir, strlen(dir));
    return g_dl_default_path;
}

kupl_dl_module_t *kupl_dl_open(const char *libpath, kupl_dl_module_sym_t *syms, int sym_count)
{
    if (kupl_unlikely(libpath == nullptr || syms == nullptr || sym_count <= 0)) {
        return nullptr;
    }

    kupl_dl_module_t *m = (kupl_dl_module_t *)kupl_malloc_inner(sizeof(kupl_dl_module_t));
    if (kupl_unlikely(m == nullptr)) {
        return nullptr;
    }

    m->handle = dlopen(libpath, RTLD_LAZY | RTLD_LOCAL);
    if (kupl_unlikely(m->handle == nullptr)) {
        const char *error = dlerror();
        if (error) {
            kupl_debug("dlopen %s failed %s", libpath, error);
        }
        goto err_free;
    }

    for (int i = 0; i < sym_count; ++i) {
        syms[i].sym = dlsym(m->handle, syms[i].sym_name);
        if (kupl_unlikely(syms[i].sym == nullptr)) {
            kupl_debug("%s read %s failed", libpath, syms[i].sym_name);
            goto err_dlclose;
        }
    }

    return m;

err_dlclose:
    dlclose(m->handle);
    m->handle = nullptr;

err_free:
    kupl_safe_free(m);
    return nullptr;
}

void kupl_dl_close(kupl_dl_module_t *module)
{
    if (module == nullptr || module->handle == nullptr) {
        return;
    }

    dlclose(module->handle);
    module->handle = nullptr;
    kupl_free_inner(module);
}
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
#include "kupl_shm.h"
#include <cstring>
#include "memory/mpool/kupl_mpool.h"
#include "memory/shm/posix/kupl_shm_posix.h"
#include "memory/shm/sls/kupl_shm_sls.h"
#include "utils/config/kupl_config.h"
#include "utils/debug/kupl_log.h"
#include "utils/type/kupl_status.h"
#include "utils/sys/kupl_dl_module.h"
#include "memory/shm/fence/kupl_fence.h"
#include "tools/struct/kupl_vla.h"

#include <string>
#include <unistd.h>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/types.h>
#include <climits>

#define LIBKUPL_SHM_XPMEM_SO "/shm/libkupl_shm_xpmem.so"
bool g_shm_inited = false;
bool g_is_abnormal_exit = false;

typedef void (*kupl_shm_reg_ops_func_t)();
typedef void (*kupl_shm_dereg_ops_func_t)();

static kupl_shm_type_t g_shm_type = KUPL_SHM_INIT;
static const kupl_shm_ops_t *g_kupl_shm_ops[KUPL_SHM_LAST] = {nullptr, nullptr, nullptr};
static kupl_dl_module_t *g_kupl_shm_xpmem_module = nullptr;

kupl_shm_info shm_info = {.is_contig = 0};


static void kupl_shm_load_module_posix()
{
    kupl_shm_posix_reg_ops();
}

static void kupl_shm_unload_module_posix()
{
    kupl_shm_posix_dereg_ops();
}

static void kupl_shm_load_module_xpmem()
{
    int ret;
    std::string lib_xpmem_path;
    kupl_dl_module_sym_t reg_ops;
    kupl_shm_reg_ops_func_t kupl_shm_xpmem_reg_ops;

    lib_xpmem_path = std::string(kupl_dl_get_default_path()) + LIBKUPL_SHM_XPMEM_SO;
    ret = access(lib_xpmem_path.c_str(), F_OK);
    if (ret) {
        return;
    }

    reg_ops.sym = nullptr;
    reg_ops.sym_name = "kupl_shm_xpmem_reg_ops";

    if (g_kupl_shm_xpmem_module == nullptr) {
        g_kupl_shm_xpmem_module = kupl_dl_open(lib_xpmem_path.c_str(), &reg_ops, 1);
    }
    if (g_kupl_shm_xpmem_module) {
        kupl_shm_xpmem_reg_ops = (kupl_shm_reg_ops_func_t)reg_ops.sym;
        if (kupl_shm_xpmem_reg_ops) {
            kupl_shm_xpmem_reg_ops();
        }
    }
}

static void kupl_shm_unload_module_xpmem()
{
    std::string lib_xpmem_path;
    kupl_dl_module_sym_t dereg_ops;
    kupl_shm_dereg_ops_func_t kupl_shm_xpmem_dereg_ops;

    if (g_kupl_shm_xpmem_module) {
        lib_xpmem_path = std::string(kupl_dl_get_default_path()) + LIBKUPL_SHM_XPMEM_SO;

        dereg_ops.sym = nullptr;
        dereg_ops.sym_name = "kupl_shm_xpmem_dereg_ops";

        g_kupl_shm_xpmem_module = kupl_dl_open(lib_xpmem_path.c_str(), &dereg_ops, 1);
        if (g_kupl_shm_xpmem_module) {
            kupl_shm_xpmem_dereg_ops = (kupl_shm_dereg_ops_func_t)dereg_ops.sym;
            if (kupl_shm_xpmem_dereg_ops) {
                kupl_shm_xpmem_dereg_ops();
            }
            kupl_dl_close(g_kupl_shm_xpmem_module);
            g_kupl_shm_xpmem_module = nullptr;
        }
    }
}

static void kupl_shm_load_module_sls()
{
    kupl_shm_sls_reg_ops();
}

static void kupl_shm_unload_module_sls()
{
    kupl_shm_sls_dereg_ops();
}

static void kupl_shm_load_modules()
{
    kupl_shm_load_module_posix();
    kupl_shm_load_module_xpmem();
    kupl_shm_load_module_sls();
}

static void kupl_shm_unload_modules()
{
    kupl_shm_unload_module_xpmem();
    kupl_shm_unload_module_posix();
    kupl_shm_unload_module_sls();
}

static kupl_shm_type_t kupl_shm_type_select()
{
    std::string shm_type_str = kupl_config_get_value_str(KUPL_SHM_TYPE);
    if (shm_type_str == std::string("posix") || shm_type_str == std::string("auto")) {
        g_shm_type = KUPL_SHM_POSIX;
    } else if (shm_type_str == std::string("xpmem")) {
        g_shm_type = KUPL_SHM_XPMEM;
    } else if (shm_type_str == std::string("sls")) {
        g_shm_type = KUPL_SHM_SLS;
    } else {
        kupl_warn("Unsupported shm type: %s, change to posix.", shm_type_str.c_str());
        g_shm_type = KUPL_SHM_POSIX;
    }

    return g_shm_type;
}

static void kupl_shm_type_set(kupl_shm_type_t shm_type)
{
    g_shm_type = shm_type;
}

void kupl_shm_ops_set(kupl_shm_type_t type, const kupl_shm_ops_t *ops)
{
    g_kupl_shm_ops[type] = ops;
}

int kupl_shm_init()
{
    int ret;
    kupl_shm_type_t shm_type;
    if (!g_utils_inited && kupl_utils_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (!g_mpool_inited && kupl_mpool_init() == KUPL_ERROR) {
        kupl_utils_fini();
        return KUPL_ERROR;
    }
    kupl_fence_init();
    kupl_shm_load_modules();

    shm_type = kupl_shm_type_select();
    if ((g_kupl_shm_ops[shm_type] == nullptr) && (shm_type == KUPL_SHM_XPMEM)) {
        kupl_warn("The shm type xpmem is invalid, change to posix");
        shm_type = KUPL_SHM_POSIX;
        kupl_shm_type_set(KUPL_SHM_POSIX);
    }

    ret = g_kupl_shm_ops[shm_type]->init();
    if ((ret == KUPL_ERROR) && (shm_type == KUPL_SHM_XPMEM || shm_type == KUPL_SHM_SLS)) {
        kupl_warn("xpmem or sls init failed, change to posix");
        shm_type = KUPL_SHM_POSIX;
        kupl_shm_type_set(shm_type);
        ret = g_kupl_shm_ops[shm_type]->init();
    }
    g_shm_inited = true;
    return ret;
}

int kupl_shm_finalize()
{
    if (!g_shm_inited) {
        return KUPL_OK;
    }
    int ret = g_kupl_shm_ops[g_shm_type]->finalize();
    if (!g_is_abnormal_exit) {
        kupl_shm_final_cleanup();
    }
    kupl_shm_unload_modules();
    kupl_mpool_fini();
    g_shm_inited = false;
    return ret;
}

int kupl_shm_fence_create(kupl_shm_win_h win)
{
    int ret;
    void *baseptr;
    int peer_flag_num = win->comm->size;
    int comm_flag_num = 1;

    /* Alloc "single" shm buffer for both peer fence and comm fence.
       The layout of fence flag data datais below:
     *
     * +--------------+--------------+--------------+--------------+--------------+
     * |  CACHE_LINE  |  CACHE_LINE  |    ......    |  CACHE_LINE  |  CACHE_LINE  |
     * +--------------+--------------+--------------+--------------+--------------+
     * |<-                       peer_fence                      ->|<-comm_fence->|
     *                       (numprocs x flag)                        (1 x flag)
     *
     */
    size_t size = KUPL_CACHE_LINE * KUPL_SHM_FENCE_FLAG_WIN_SIZE * ((size_t)peer_flag_num + (size_t)comm_flag_num);
    win->fence_win_ptr = (void *)kupl_malloc_inner(sizeof(kupl_shm_win_h));

    ret = kupl_shm_win_alloc_inner(size, win->comm, &baseptr,
                                   (kupl_shm_win_h *)win->fence_win_ptr, 0);
    if (ret != KUPL_OK) {
        kupl_shm_win_free_inner(*(kupl_shm_win_h *)win->fence_win_ptr, 0);
        return KUPL_ERROR;
    }
    // initialization
    kupl_shm_win_h fence_win = *(kupl_shm_win_h *)win->fence_win_ptr;
    fence_win->alloc_size = size;
    fence_win->offset = 0;
    memset(baseptr, 0, size);

    win->peer_fence = kupl_fence_create(KUPL_FENCE_ALGO_P2P, win);
    win->comm_fence = kupl_fence_create(KUPL_FENCE_ALGO_DEFAULT, win);
    if (kupl_unlikely(win->peer_fence == nullptr || win->comm_fence == nullptr)) {
        kupl_shm_win_free_inner(*(kupl_shm_win_h *)win->fence_win_ptr, 0);
        kupl_free_inner(win->fence_win_ptr);
        return KUPL_ERROR;
    }
    win->peer_fence->win = *(kupl_shm_win_h *)win->fence_win_ptr;
    win->comm_fence->win = *(kupl_shm_win_h *)win->fence_win_ptr;
    return ret;
}

void kupl_shm_fence_destroy(kupl_shm_win_h win)
{
    kupl_fence_destroy(win->comm_fence);
    kupl_fence_destroy(win->peer_fence);
    kupl_shm_win_free_inner(*(kupl_shm_win_h *)win->fence_win_ptr, 0);
    kupl_safe_free(win->fence_win_ptr);
    return;
}

int kupl_shm_win_alloc(size_t size, kupl_shm_comm_h comm, void **baseptr, kupl_shm_win_h *win)
{
    if (!g_shm_inited && kupl_shm_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (comm == nullptr || win == nullptr || baseptr == nullptr) {
        g_is_abnormal_exit = true;
        return kupl_log_error_return(FATAL, "comm, win or baseptr are nullptr");
    }
    int ret = kupl_shm_win_alloc_inner(size, comm, baseptr, win, 1);
    if (ret == KUPL_OK) {
        memset(*baseptr, 0, size);
        (*win)->alloc_size = size;
        (*win)->offset = 0;
        kupl_shm_fence_create(*win);
        if (!(*win)->comm_fence || !(*win)->peer_fence) {
            kupl_shm_win_free(*win);
            ret = KUPL_ERROR;
        }
    }
    if (ret == KUPL_ERROR) {
        g_is_abnormal_exit = true;
        kupl_fatal("win alloc failed");
    }
    return ret;
}

int kupl_shm_win_free(kupl_shm_win_h win)
{
    if (!g_shm_inited && kupl_shm_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (win == nullptr) {
        return kupl_log_error_return(ERROR, "win is nullptr, no free");
    }
    kupl_shm_oob_barrier_cb_t oob_fence = win->comm->oob_fence;
    oob_fence(win->comm->group);
    kupl_shm_fence_destroy(win);
    return kupl_shm_win_free_inner(win, 1);
}

int kupl_shm_win_alloc_inner(size_t size, kupl_shm_comm_h comm, void **baseptr, kupl_shm_win_h *win, int flag)
{
    int numprocs = comm->size;
    kupl_vla<size_t>offset_list((size_t)numprocs);
    if (kupl_unlikely(offset_list.get_data() == nullptr)) {
        return KUPL_ERROR;
    }
    for (int i = 0; i < numprocs; i++) {
        offset_list[i] = 0;
    }
    if (shm_info.is_contig) {
        kupl_vla<size_t>size_list((size_t)numprocs);
        if (kupl_unlikely(size_list.get_data() == nullptr)) {
            return KUPL_ERROR;
        }
        kupl_shm_oob_allgather_cb_t oob_allgather = comm->oob_allgather;
        oob_allgather(&size, size_list.get_data(), sizeof(size_t), comm->group, KUPL_SHM_DATATYPE_CHAR);
        size = size_list[0];
        for (int i = 1; i < numprocs; i++) {
            size += size_list[i];
            offset_list[i] = offset_list[i - 1] + size_list[i - 1];
        }
    }
    if (size > INT_MAX) {
        return kupl_log_error_return(ERROR, "win alloc size illegal");
    }
    return g_kupl_shm_ops[g_shm_type]->shm_win_alloc(size, comm, baseptr, win, flag, offset_list.get_data());
}

int kupl_shm_win_free_inner(kupl_shm_win_h win, int flag)
{
    if (win == nullptr) {
        return kupl_log_error_return(ERROR, "win is nullptr, no free");
    }
    return g_kupl_shm_ops[g_shm_type]->shm_win_free(win, flag);
}

int kupl_shm_win_query(kupl_shm_win_h win, int remote_rank, void **baseptr)
{
    if (!g_shm_inited && kupl_shm_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if ((win == nullptr) || (baseptr == nullptr)) {
        return kupl_log_error_return(ERROR, "win or baseptr is nullptr");
    }

    if (remote_rank < 0 || remote_rank >= win->size) {
        return kupl_log_error_return(ERROR, "invalid remote rank");
    }
    return g_kupl_shm_ops[g_shm_type]->shm_win_query(win, remote_rank, baseptr);
}

int kupl_shm_info_set(kupl_info_flag_t info_flag, uint32_t value)
{
    if (!g_shm_inited && kupl_shm_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    switch (info_flag) {
        case KUPL_SHM_INFO_IS_CONTIG:
            if (value != 0 && value != 1) {
                return kupl_log_error_return(ERROR, "KUPL_SHM_INFO_IS_CONTIG value is not right");
            }
            shm_info.is_contig = value;
            break;
        default:
            return kupl_log_error_return(ERROR, "info_flag is not right");
    }
    return KUPL_OK;
}
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
#include "kupl_shm_sls.h"
#include "memory/mpool/kupl_mpool.h"
#include "utils/debug/kupl_log.h"
#include "utils/type/kupl_status.h"
#include "utils/sys/kupl_compiler.h"

#include <cstdint>
#include <unistd.h>
#include <map>
#include <vector>
#include <string>
#include <numa.h>
#include <numaif.h>

static int g_kupl_shm_sls_use_hbw = 0;
static int g_kupl_shm_sls_use_hugepage = 0;

static kupl_always_inline long kupl_shm_sls_bind_to_hbw()
{
    int cpu = sched_getcpu();
    int hbw_node = numa_node_of_cpu(cpu) + 16;
    unsigned long nodemask = 1 << hbw_node;
    unsigned long maxnode = 64;

    return set_mempolicy(MPOL_BIND, &nodemask, maxnode);
}

static kupl_always_inline long kupl_shm_sls_unbind_to_hbw()
{
    int cpu = sched_getcpu();
    int ddr_node = numa_node_of_cpu(cpu);
    unsigned long nodemask = 1 << ddr_node;
    unsigned long maxnode = 64;

    return set_mempolicy(MPOL_BIND, &nodemask, maxnode);
}

static kupl_always_inline int kupl_shm_sls_get_fd()
{
    kupl_shm_sls_slfs_t res;
    int ret = kupl_shm_sls_init_fs(res);
    if (ret == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    return res.init.fd;
}

static int kupl_shm_sls_allgather_addr(size_t size, void *baseptr, kupl_shm_win_h win, size_t *offset_list)
{
    int myid = win->rank;
    int numprocs = win->size;
    kupl_shm_sls_params_t *sls_params_list = nullptr;
    kupl_shm_oob_allgather_cb_t oob_allgather;
    kupl_shm_sls_slfs_t slfs;
    int ret = KUPL_ERROR;

    sls_params_list = (kupl_shm_sls_params_t *)kupl_malloc_inner(sizeof(kupl_shm_sls_params_t) * (size_t)numprocs);
    if (sls_params_list == nullptr) {
        kupl_error("sls_params_list malloc failed");
        return KUPL_ERROR;
    }

    /* set sls params list */
    sls_params_list[myid].fd = kupl_shm_sls_get_fd();
    sls_params_list[myid].pid = getpid();
    sls_params_list[myid].addr = baseptr;
    sls_params_list[myid].size = size;

    oob_allgather = win->comm->oob_allgather;
    oob_allgather(nullptr, sls_params_list, sizeof(kupl_shm_sls_params_t), win->comm->group, KUPL_SHM_DATATYPE_CHAR);

    void *attach_addr;
    void *base_addr;
    for (int i = 0; i < numprocs; i++) {
        if (myid != i) {
            ret = kupl_shm_sls_zcopy_all(sls_params_list[myid].fd, sls_params_list[i].addr, &attach_addr,
                                         sls_params_list[i].pid, sls_params_list[myid].pid, sls_params_list[i].size,
                                         &base_addr, slfs);
            if (ret == KUPL_ERROR) {
                kupl_error("kupl_shm_sls_zcopy_all failed");
                goto free_sls_params_list;
            }
            win->info[i].attach_address = attach_addr;
            win->info[i].base_address = base_addr;
            offset_list[i] = 0;
        } else {
            win->info[i].attach_address = baseptr;
            offset_list[i] = 0;
        }
    }

    ret = KUPL_OK;
free_sls_params_list:
    kupl_free_inner(sls_params_list);
    return ret;
}

int kupl_shm_sls_init()
{
    if (!kupl_shm_sls_init_module()) {
        return KUPL_ERROR;
    }
    std::string use_hbw = kupl_config_get_value_str(KUPL_SHM_ON_PACKAGE);
    if (use_hbw.length() > 0 && use_hbw == "y") {
        g_kupl_shm_sls_use_hbw = 1;
    } else {
        g_kupl_shm_sls_use_hbw = 0;
    }
    std::string use_hugepage = kupl_config_get_value_str(KUPL_SHM_ENABLE_HUGEPAGE);
    if (use_hbw.length() > 0 && use_hugepage == "y") {
        g_kupl_shm_sls_use_hugepage = 1;
    } else {
        g_kupl_shm_sls_use_hugepage = 0;
    }
    return KUPL_OK;
}

int kupl_shm_sls_finalize()
{
    return KUPL_OK;
}

static kupl_always_inline int kupl_shm_sls_win_add(kupl_shm_comm_h comm, kupl_shm_win_h win, int flag)
{
    if (flag) {
        kupl_list_insert_after(&comm->win_list, &win->list);
    }
    return KUPL_OK;
}

static kupl_always_inline int kupl_shm_sls_win_del(kupl_shm_win_h win, int flag)
{
    if (flag) {
        kupl_list_del(&win->list);
    }
    return KUPL_OK;
}

int kupl_shm_sls_win_alloc(size_t size, kupl_shm_comm_h comm, void **baseptr, kupl_shm_win_h *win, int flag,
                           size_t *offset_list)
{
    int ret;
    long err;
    kupl_shm_win_h win_temp;

    win_temp = (kupl_shm_win_t *)kupl_malloc_inner(sizeof(kupl_shm_win_t));
    if (win_temp == nullptr) {
        kupl_error("kupl_shm_sls_win_alloc malloc win failed");
        return KUPL_ERROR;
    }

    win_temp->rank = comm->rank;
    win_temp->size = comm->size;
    win_temp->info = (kupl_shm_base_info_t *)kupl_malloc_inner(sizeof(kupl_shm_base_info_t) * (size_t)comm->size);
    if (win_temp->info == nullptr) {
        kupl_error("kupl_shm_sls_win_alloc malloc info failed");
        ret = KUPL_ERROR;
        goto err_free_win;
    }

    win_temp->comm = comm;

    if (g_kupl_shm_sls_use_hugepage) {
        win_temp->base_ptr = kupl_malloc_hugepages_inner(size, &win_temp->align_size, g_kupl_shm_sls_use_hbw);
    } else {
        /* alloc buffer on HBW */
        if (g_kupl_shm_sls_use_hbw) {
            err = kupl_shm_sls_bind_to_hbw();
            if (err == -1) {
                kupl_error("set_mempolicy bind to hbw node failed");
                ret = KUPL_ERROR;
                goto err_free_info;
            }
        }
        win_temp->base_ptr = kupl_malloc_inner(size);
        /* restore memory allocation on DDR */
        if (g_kupl_shm_sls_use_hbw) {
            err = kupl_shm_sls_unbind_to_hbw();
            if (err == -1) {
                kupl_error("set_mempolicy unbind to hbw node failed");
                ret = KUPL_ERROR;
                goto err_free_base_ptr;
            }
        }
    }
    if (win_temp->base_ptr == nullptr) {
        kupl_error("kupl_shm_sls_win_alloc failed with malloc size %zu", size);
        ret = KUPL_ERROR;
        goto err_free_info;
    }

    memset(win_temp->base_ptr, 0, size);
    /* pin base_ptr */
    if (g_kupl_shm_sls_use_hugepage) {
        kupl_mlock(win_temp->base_ptr, win_temp->align_size);
    } else {
        kupl_mlock(win_temp->base_ptr, size);
    }

    if (kupl_shm_sls_allgather_addr(size, win_temp->base_ptr, win_temp, offset_list)) {
        ret = KUPL_ERROR;
        goto err_free_base_ptr;
    }

    kupl_shm_sls_win_add(comm, win_temp, flag);
    *baseptr = win_temp->base_ptr;
    *win = win_temp;

    return KUPL_OK;

err_free_base_ptr:
    kupl_safe_free(win_temp->base_ptr);
err_free_info:
    kupl_safe_free(win_temp->info);
err_free_win:
    kupl_safe_free(win_temp);
    return ret;
}

int kupl_shm_sls_win_free(kupl_shm_win_h win, int flag)
{
    /* detatch the address from remote side */
    for (int i = 0; i < win->size; i++) {
        if (i == win->rank) {
            win->info[i].attach_address = nullptr;
        } else {
            kupl_safe_free(win->info[i].base_address);
            win->info[i].attach_address = nullptr;
        }
    }

    if (g_kupl_shm_sls_use_hugepage) {
        /* unpin the addares */
        kupl_munlock(win->base_ptr, win->align_size);
        kupl_free_hugepages_inner(win->base_ptr, win->align_size);
        win->base_ptr = nullptr;
    } else {
        /* unpin the addares */
        kupl_munlock(win->base_ptr, win->alloc_size);
        kupl_safe_free(win->base_ptr);
    }

    kupl_shm_sls_win_del(win, flag);

    kupl_safe_free(win->info);
    kupl_safe_free(win);

    return KUPL_OK;
}

int kupl_shm_sls_win_mpool_alloc(size_t size, kupl_shm_comm_h comm, void **baseptr, kupl_shm_win_h *win, int flag,
                                 size_t *offset_list)
{
    int ret;
    if (size == 0) {
        kupl_warn("kupl mpool does not support 0 size allocation, fallback to malloc.")
            ret = kupl_shm_sls_win_alloc(0, comm, baseptr, win, flag, offset_list);
        return ret;
    }
    kupl_shm_win_h win_temp;

    win_temp = (kupl_shm_win_t *)kupl_malloc_inner(sizeof(kupl_shm_win_t));
    if (win_temp == nullptr) {
        kupl_error("kupl_shm_sls_win_alloc malloc win failed");
        return KUPL_ERROR;
    }

    win_temp->rank = comm->rank;
    win_temp->size = comm->size;
    win_temp->info = (kupl_shm_base_info_t *)kupl_malloc_inner(sizeof(kupl_shm_base_info_t) * (size_t)comm->size);
    if (win_temp->info == nullptr) {
        kupl_error("kupl_shm_sls_win_alloc malloc info failed");
        ret = KUPL_ERROR;
        goto err_free_win;
    }

    win_temp->comm = comm;

    if (g_kupl_shm_sls_use_hugepage) {
        win_temp->base_ptr = kupl_mpool_malloc_hugepages_inner(size, &win_temp->align_size, g_kupl_shm_sls_use_hbw);
    } else {
        /* alloc buffer on HBW */
        if (g_kupl_shm_sls_use_hbw) {
            win_temp->base_ptr = kupl_hbw_malloc(size);
        } else {
            win_temp->base_ptr = kupl_malloc(KUPL_MEM_LARGE_CAP, size);
        }
    }

    if (win_temp->base_ptr == nullptr) {
        kupl_error("kupl_shm_sls_win_alloc failed with malloc size %zu", size);
        ret = KUPL_ERROR;
        goto err_free_info;
    }

    memset(win_temp->base_ptr, 0, size);

    if (kupl_shm_sls_allgather_addr(size, win_temp->base_ptr, win_temp, offset_list)) {
        ret = KUPL_ERROR;
        goto err_free_base_ptr;
    }

    kupl_shm_sls_win_add(comm, win_temp, flag);
    *baseptr = win_temp->base_ptr;
    *win = win_temp;

    return KUPL_OK;

err_free_base_ptr:
    kupl_safe_free(win_temp->base_ptr);
err_free_info:
    kupl_safe_free(win_temp->info);
err_free_win:
    kupl_safe_free(win_temp);
    return ret;
}

int kupl_shm_sls_win_mpool_free(kupl_shm_win_h win, int flag)
{
    if (win->alloc_size == 0) {
        return kupl_shm_sls_win_free(win, flag);
    }
    /* detatch the address from remote side */
    for (int i = 0; i < win->size; i++) {
        if (i == win->rank) {
            win->info[i].attach_address = nullptr;
        } else {
            kupl_safe_free(win->info[i].base_address);
            win->info[i].attach_address = nullptr;
        }
    }

    if (g_kupl_shm_sls_use_hugepage) {
        /* unpin the addares */
        kupl_mpool_free_hugepages_inner(win->base_ptr, win->align_size, g_kupl_shm_sls_use_hbw);
        win->base_ptr = nullptr;
    } else {
        /* unpin the addares */
        if (g_kupl_shm_sls_use_hbw) {
            kupl_hbw_free(win->base_ptr);
        } else {
            kupl_free(KUPL_MEM_LARGE_CAP, win->base_ptr);
        }
    }

    kupl_shm_sls_win_del(win, flag);

    kupl_safe_free(win->info);
    kupl_safe_free(win);

    return KUPL_OK;
}

int kupl_shm_sls_win_query(kupl_shm_win_h win, int remote_rank, void **baseptr)
{
    *baseptr = win->info[remote_rank].attach_address;
    return KUPL_OK;
}

static const kupl_shm_ops_t g_kupl_shm_sls_ops = {.init = kupl_shm_sls_init,
                                                  .finalize = kupl_shm_sls_finalize,
                                                  .shm_win_alloc = kupl_shm_sls_win_alloc,
                                                  .shm_win_free = kupl_shm_sls_win_free,
                                                  .shm_win_query = kupl_shm_sls_win_query};

static const kupl_shm_ops_t g_kupl_shm_sls_mpool_ops = {.init = kupl_shm_sls_init,
                                                        .finalize = kupl_shm_sls_finalize,
                                                        .shm_win_alloc = kupl_shm_sls_win_mpool_alloc,
                                                        .shm_win_free = kupl_shm_sls_win_mpool_free,
                                                        .shm_win_query = kupl_shm_sls_win_query};

void kupl_shm_sls_reg_ops()
{
    std::string use_mpool = kupl_config_get_value_str(KUPL_ENABLE_MPOOL);
    if (use_mpool.length() > 0 && use_mpool == "y") {
        kupl_shm_ops_set(KUPL_SHM_SLS, &g_kupl_shm_sls_mpool_ops);
    } else {
        kupl_shm_ops_set(KUPL_SHM_SLS, &g_kupl_shm_sls_ops);
    }
}

void kupl_shm_sls_dereg_ops()
{
    kupl_shm_ops_set(KUPL_SHM_SLS, nullptr);
}
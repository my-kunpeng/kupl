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

#include "kupl_shm_xpmem.h"
#include "memory/mpool/kupl_mpool.h"
#include "utils/debug/kupl_log.h"
#include "utils/type/kupl_status.h"
#include "utils/sys/kupl_compiler.h"

#include <cstdint>
#include <unistd.h>
#include <map>
#include <vector>

typedef std::vector<kupl_shm_xpmem_remote_region> kupl_shm_xpmem_remote_regions_t;
typedef std::map<kupl_shm_xpmem_segid_t, kupl_shm_xpmem_apid_t> kupl_shm_xpmem_remote_keys_t;
typedef std::map<kupl_shm_xpmem_apid_t, kupl_shm_xpmem_remote_regions_t> kupl_shm_xpmem_remote_caches_t;

static kupl_shm_xpmem_segid_t g_xsegid = -1;
static kupl_shm_xpmem_remote_keys_t g_remote_keys;
static kupl_shm_xpmem_remote_caches_t g_remote_caches;

static kupl_always_inline
int kupl_shm_xpmem_make_global_xsegid(kupl_shm_xpmem_segid_t *xsegid_p)
{
    g_xsegid = kupl_shm_xpmem_make(0, XPMEM_MAXADDR_SIZE, XPMEM_PERMIT_MODE, (void*)0600);
    if (g_xsegid < 0) {
        return kupl_log_error_return(ERROR, "kupl_shm_xpmem_make failed");
    }

    *xsegid_p = g_xsegid;
    return KUPL_OK;
}

static kupl_always_inline
int kupl_shm_xpmem_get_global_xsegid(kupl_shm_xpmem_segid_t *xsegid_p)
{
    if (g_xsegid < 0) {
        return kupl_shm_xpmem_make_global_xsegid(xsegid_p);
    }

    *xsegid_p = g_xsegid;
    return KUPL_OK;
}

static kupl_always_inline
int kupl_shm_xpmem_remove_global_xsegid()
{
    if (g_xsegid > 0) {
        return kupl_shm_xpmem_remove(g_xsegid);
    }

    return KUPL_OK;
}

static kupl_shm_xpmem_apid_t kupl_shm_xpmem_get_apid(kupl_shm_xpmem_remote_keys_t &remote_keys,
    kupl_shm_xpmem_segid_t remote_xsegid)
{
    kupl_shm_xpmem_apid_t remote_xapid = -1;

    auto iter = remote_keys.find(remote_xsegid);
    if (iter != remote_keys.end()) {
        return iter->second;
    }

    remote_xapid = kupl_shm_xpmem_get(remote_xsegid, XPMEM_RDWR, XPMEM_PERMIT_MODE, nullptr);
    if (remote_xapid == -1) {
        kupl_error("kupl_shm_xpmem_get failed");
        return -1;
    }
    /* Saved in the cache to avoid repeated attach */
    remote_keys.insert(std::pair<kupl_shm_xpmem_segid_t, kupl_shm_xpmem_apid_t>(remote_xsegid, remote_xapid));

    return remote_xapid;
}

static kupl_shm_xpmem_remote_region_t kupl_shm_xpmem_create_remote_region(
    kupl_shm_xpmem_apid_t xapid, uintptr_t remote_addr, size_t length)
{
    kupl_shm_xpmem_addr_t xaddr;
    kupl_shm_xpmem_remote_region_t remote_region;

    remote_region.start = kupl_shm_align_down_pow2(remote_addr, PAGE_SIZE);
    remote_region.end = kupl_shm_align_up_pow2((remote_addr+length), PAGE_SIZE);

    xaddr.apid = xapid;
    xaddr.offset = remote_region.start;
    remote_region.attach_pg_aligned_addr = kupl_shm_xpmem_attach(xaddr, remote_region.end - remote_region.start,
                                                                 nullptr);
    if (remote_region.attach_pg_aligned_addr == nullptr) {
        kupl_error("kupl_shm_xpmem_attach failed");
    }
    return remote_region;
}

static kupl_shm_xpmem_remote_region_t kupl_shm_xpmem_get_remote_region(
    kupl_shm_xpmem_remote_regions_t &remote_regions, kupl_shm_xpmem_apid_t xapid,
    uintptr_t remote_addr, size_t length)
{
    kupl_shm_xpmem_remote_region_t remote_region;
    uintptr_t start;
    uintptr_t end;

    start = kupl_shm_align_down_pow2(remote_addr, PAGE_SIZE);
    end = kupl_shm_align_up_pow2((remote_addr+length), PAGE_SIZE);

    for (auto iter = remote_regions.begin(); iter != remote_regions.end(); iter++) {
        if ((start >= iter->start) && (end <= iter->end)) {
            return *iter;
        }
    }

    remote_region = kupl_shm_xpmem_create_remote_region(xapid, remote_addr, length);
    if (remote_region.attach_pg_aligned_addr != nullptr) {
        /* Saved in the cache to avoid repeated attach */
        remote_regions.push_back(remote_region);
    }

    return remote_region;
}

static int kupl_shm_xpmem_delete_remote_regions(kupl_shm_xpmem_remote_regions_t &remote_regions)
{
    int ret;

    for (auto iter = remote_regions.begin(); iter != remote_regions.end(); iter++) {
        ret = kupl_shm_xpmem_detach(iter->attach_pg_aligned_addr);
        if (ret == -1) {
            return kupl_log_error_return(ERROR, "kupl_shm_xpmem_detach failed");
        }
    }

    return KUPL_OK;
}

static int kupl_shm_xpmem_delete_remote_keys(kupl_shm_xpmem_remote_keys_t &remote_keys)
{
    int ret;

    for (auto iter = remote_keys.begin(); iter != remote_keys.end(); iter++) {
        ret = kupl_shm_xpmem_release(iter->second);
        if (ret == -1) {
            return kupl_log_error_return(ERROR, "kupl_shm_xpmem_release failed");
        }
    }

    return KUPL_OK;
}

static int kupl_shm_xpmem_get_trans_mem(kupl_shm_xpmem_packed_params_t params,
    kupl_shm_xpmem_remote_trans_mem_t *remote_trans_mem_p)
{
    kupl_shm_xpmem_apid_t xapid;
    kupl_shm_xpmem_remote_region_t remote_region;
    kupl_shm_xpmem_remote_regions_t remote_regions;

    xapid = kupl_shm_xpmem_get_apid(g_remote_keys, params.xsegid);
    if (xapid == -1) {
        return KUPL_ERROR;
    }

    auto iter = g_remote_caches.find(xapid);
    if (iter != g_remote_caches.end()) {
        remote_region = kupl_shm_xpmem_get_remote_region(
            iter->second, xapid, params.remote_addr, params.length);
        if (remote_region.attach_pg_aligned_addr == nullptr) {
            return KUPL_ERROR;
        }
    } else {
        remote_region = kupl_shm_xpmem_create_remote_region(xapid, params.remote_addr, params.length);
        if (remote_region.attach_pg_aligned_addr == nullptr) {
            return KUPL_ERROR;
        }
        /* Saved in the cache to avoid repeated attach */
        remote_regions.push_back(remote_region);
        g_remote_caches.insert(
            std::pair<kupl_shm_xpmem_apid_t, kupl_shm_xpmem_remote_regions_t>{xapid, remote_regions});
    }

    remote_trans_mem_p->attach_addr = ((uint8_t *)remote_region.attach_pg_aligned_addr) +
        (params.remote_addr - remote_region.start);
    remote_trans_mem_p->length = params.length;

    return KUPL_OK;
}

static int kupl_shm_xpmem_allgather_addr(size_t size, void *baseptr, kupl_shm_win_h win)
{
    int myid = win->rank;
    int numprocs = win->size;
    kupl_shm_xpmem_packed_params_t *packed_params_list = nullptr;
    kupl_shm_oob_allgather_cb_t oob_allgather;
    kupl_shm_xpmem_remote_trans_mem_t remote_trans_mem;
    int ret;

    packed_params_list = (kupl_shm_xpmem_packed_params_t *)kupl_malloc_inner(
        sizeof(kupl_shm_xpmem_packed_params_t) * numprocs);
    if (packed_params_list == nullptr) {
        return KUPL_ERROR;
    }
    packed_params_list[myid].xsegid = g_xsegid;
    packed_params_list[myid].remote_addr = (uintptr_t)baseptr;
    packed_params_list[myid].length = size;

    oob_allgather = win->comm->oob_allgather;
    oob_allgather(nullptr, packed_params_list, sizeof(kupl_shm_xpmem_packed_params_t), win->comm->group,
                  KUPL_SHM_DATATYPE_CHAR);

    for (int i = 0; i < numprocs; i++) {
        if (myid != i) {
            ret = kupl_shm_xpmem_get_trans_mem(packed_params_list[i], &remote_trans_mem);
            if (ret == KUPL_ERROR) {
                goto free_packed_params_list;
            }
            win->info[i].attach_address = remote_trans_mem.attach_addr;
        } else {
            win->info[i].attach_address = baseptr;
        }
    }

free_packed_params_list:
    kupl_free_inner(packed_params_list);
    return ret;
}

int kupl_shm_xpmem_init()
{
    int ver;
    kupl_shm_xpmem_segid_t xsegid;

    /* check xpmem */
    ver = kupl_shm_xpmem_version();
    if (ver == -1) {
        kupl_error("Check xpmem version failed");
        return ver;
    }

    return kupl_shm_xpmem_get_global_xsegid(&xsegid);
}

int kupl_shm_xpmem_finalize()
{
    int ret;

    for (auto iter = g_remote_caches.begin(); iter != g_remote_caches.end(); iter++) {
        ret = kupl_shm_xpmem_delete_remote_regions(iter->second);
        if (ret == KUPL_ERROR) {
            return ret;
        }
    }

    ret = kupl_shm_xpmem_delete_remote_keys(g_remote_keys);
    if (ret == KUPL_ERROR) {
        return ret;
    }

    return kupl_shm_xpmem_remove_global_xsegid();
}

static kupl_always_inline
int kupl_shm_xpmem_win_add(kupl_shm_comm_h comm, kupl_shm_win_h win, int flag)
{
    if (flag) {
        kupl_list_insert_after(&comm->win_list, &win->list);
    }
    return KUPL_OK;
}

static kupl_always_inline
int kupl_shm_xpmem_win_del(kupl_shm_win_h win, int flag)
{
    if (flag) {
        kupl_list_del(&win->list);
    }
    return KUPL_OK;
}

int kupl_shm_xpmem_win_alloc(size_t size, kupl_shm_comm_h comm, void **baseptr, kupl_shm_win_h *win, int flag,
    size_t *offset_list)
{
    int ret;
    kupl_shm_win_h win_temp;

    win_temp = (kupl_shm_win_t *)kupl_malloc_inner(sizeof(kupl_shm_win_t));
    if (win_temp == nullptr) {
        return kupl_log_error_return(ERROR, "kupl_shm_xpmem_win_alloc malloc win failed");
    }

    win_temp->rank = comm->rank;
    win_temp->size = comm->size;
    win_temp->info = (kupl_shm_base_info_t *)kupl_malloc_inner(sizeof(kupl_shm_base_info_t) * comm->size);
    if (win_temp->info == nullptr) {
        kupl_error("kupl_shm_xpmem_win_alloc malloc info failed");
        ret = KUPL_ERROR;
        goto err_free_win;
    }

    win_temp->comm = comm;
    win_temp->base_ptr = kupl_malloc_inner(size);
    if (win_temp->base_ptr == nullptr) {
        kupl_error("kupl_shm_xpmem_win_alloc failed with malloc size %zu", size);
        ret = KUPL_ERROR;
        goto err_free_info;
    }

    if (kupl_shm_xpmem_allgather_addr(size, win_temp->base_ptr, win_temp)) {
        ret = KUPL_ERROR;
        goto err_free_base_ptr;
    }

    kupl_shm_xpmem_win_add(comm, win_temp, flag);
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

int kupl_shm_xpmem_win_free(kupl_shm_win_h win, int flag)
{
    kupl_shm_xpmem_win_del(win, flag);

    kupl_safe_free(win->base_ptr);
    kupl_safe_free(win->info);
    kupl_safe_free(win);

    return KUPL_OK;
}

int kupl_shm_xpmem_win_query(kupl_shm_win_h win, int remote_rank, void **baseptr)
{
    *baseptr = win->info[remote_rank].attach_address;
    return KUPL_OK;
}

static const kupl_shm_ops_t g_kupl_shm_xpmem_ops = {
    .init               = kupl_shm_xpmem_init,
    .finalize           = kupl_shm_xpmem_finalize,
    .shm_win_alloc      = kupl_shm_xpmem_win_alloc,
    .shm_win_free       = kupl_shm_xpmem_win_free,
    .shm_win_query      = kupl_shm_xpmem_win_query
};

void kupl_shm_xpmem_reg_ops()
{
    kupl_shm_ops_set(KUPL_SHM_XPMEM, &g_kupl_shm_xpmem_ops);
}

void kupl_shm_xpmem_dereg_ops()
{
    kupl_shm_ops_set(KUPL_SHM_XPMEM, nullptr);
}

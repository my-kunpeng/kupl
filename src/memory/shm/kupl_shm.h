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
#ifndef KUPL_SHM_H
#define KUPL_SHM_H
#include "kupl.h"
#include "kupl_shmc.h"
#include "utils/kupl_utils.h"
#include "memory/shm/fence/kupl_fence.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)

#define kupl_shm_align_down_pow2(_n, _alignment) ((_n) & ~((_alignment) - 1))

#define kupl_shm_align_up_pow2(_n, _alignment) kupl_shm_align_down_pow2((_n) + (_alignment) - 1, _alignment)

typedef struct kupl_fence *kupl_fence_h;

struct kupl_shm_info {
    uint32_t is_contig;
};
typedef struct kupl_shm_info *kupl_shm_info_h;
extern kupl_shm_info shm_info;
extern bool g_is_abnormal_exit;
extern bool g_shm_inited;
typedef struct kupl_shm_win {
    kupl_fence_h comm_fence;
    kupl_fence_h peer_fence;
    void *fence_win_ptr;
    kupl_shm_base_info_t *info;
    kupl_shm_comm_h comm;
    int size;
    int rank;
    void *base_ptr;
    size_t alloc_size;
    size_t offset;
    kupl_list_t list;
    uint32_t is_contig;
    int sum_size;
    size_t align_size;
} kupl_shm_win_t;

typedef kupl_shm_win_t *kupl_shm_win_h;

typedef enum kupl_shm_type {
    KUPL_SHM_INIT = -1,
    KUPL_SHM_POSIX,
    KUPL_SHM_XPMEM,
    KUPL_SHM_SLS,
    KUPL_SHM_LAST,
} kupl_shm_type_t;

typedef int (*kupl_shm_init_func_t)(void);
typedef int (*kupl_shm_finalize_func_t)(void);
typedef int (*kupl_shm_win_alloc_func_t)(size_t size, kupl_shm_comm_h comm, void **baseptr, kupl_shm_win_h *win,
                                         int flag, size_t *offset_list);
typedef int (*kupl_shm_win_free_func_t)(kupl_shm_win_h win, int flag);
typedef int (*kupl_shm_win_query_func_t)(kupl_shm_win_h win, int remote_rank, void **baseptr);

typedef struct kupl_shm_ops {
    kupl_shm_init_func_t init;
    kupl_shm_finalize_func_t finalize;
    kupl_shm_win_alloc_func_t shm_win_alloc;
    kupl_shm_win_free_func_t shm_win_free;
    kupl_shm_win_query_func_t shm_win_query;
} kupl_shm_ops_t;

void kupl_shm_ops_set(kupl_shm_type_t type, const kupl_shm_ops_t *ops);

int kupl_shm_init(void);
int kupl_shm_finalize(void);
int kupl_shm_win_alloc_inner(size_t size, kupl_shm_comm_h comm, void **baseptr, kupl_shm_win_h *win, int flag);
int kupl_shm_win_free_inner(kupl_shm_win_h win, int flag);

#ifdef __cplusplus
}
#endif

#endif

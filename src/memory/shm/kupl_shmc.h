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
#ifndef KUPL_SHMC_H
#define KUPL_SHMC_H
#include "kupl.h"
#include "utils/type/kupl_status.h"
#include "utils/debug/kupl_log.h"
#include "tools/struct/kupl_list.h"
#include "utils/sys/kupl_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KUPL_SHM_COMM_MAX_SIZE 1024

typedef enum kupl_shm_coll_type {
    KUPL_SHM_COLL_TYPE_BCAST = 0,
    KUPL_SHM_COLL_TYPE_ALLREDUCE,
    KUPL_SHM_COLL_TYPE_ALLTOALL
} kupl_shm_coll_type_t;

typedef enum kupl_shm_bcast_algo {
    KUPL_SHM_BCAST_ALGO_AUTO = 0,
    KUPL_SHM_BCAST_ALGO_LINEAR,
    KUPL_SHM_BCAST_ALGO_LINEAR_OPT,
    KUPL_SHM_BCAST_ALGO_TOPO_AWARE_LINEAR,
    KUPL_SHM_BCAST_ALGO_TOPO_AWARE_LINEAR_OPT,
    KUPL_SHM_BCAST_ALGO_RING_PIPELINE,
    KUPL_SHM_BCAST_ALGO_LINEAR_SCATTER_LINEAR_ALLGATHER,
    KUPL_SHM_BCAST_ALGO_MAX,
} kupl_shm_bcast_algo_t;

typedef enum kupl_shm_allreduce_algo {
    KUPL_SHM_ALLREDUCE_ALGO_AUTO = 0,
    KUPL_SHM_ALLREDUCE_ALGO_LINEAR,
    KUPL_SHM_ALLREDUCE_ALGO_RB_RH_RD,
    KUPL_SHM_ALLREDUCE_ALGO_MAX,
} kupl_shm_allreduce_algo_t;

typedef enum kupl_shm_alltoall_algo {
    KUPL_SHM_ALLTOALL_ALGO_AUTO = 0,
    KUPL_SHM_ALLTOALL_ALGO_LINEAR_READ,
    KUPL_SHM_ALLTOALL_ALGO_LINEAR_WRITE,
    KUPL_SHM_ALLTOALL_ALGO_MAX
} kupl_shm_alltoall_algo_t;

typedef struct kupl_shm_base_info {
    void *attach_address;
    void *base_address;
} __attribute__((packed)) kupl_shm_base_info_t;

typedef struct kupl_shm_bcast_wins {
    kupl_shm_win_h win;
    kupl_shm_win_h notify_root_win;
    kupl_shm_win_h notify_others_win;
    kupl_shm_win_h p2p_win;
} kupl_shm_bcast_wins_t;

typedef struct kupl_shm_allreduce_wins {
    kupl_shm_win_h send_win;
    kupl_shm_win_h recv_win;
} kupl_shm_allreduce_wins_t;

typedef struct kupl_shm_alltoall_wins {
    kupl_shm_win_h send_win;
    kupl_shm_win_h recv_win;
} kupl_shm_alltoall_wins_t;

typedef struct kupl_shm_bcast_args {
    void *buffer;
    int count;
    int root;
    kupl_shm_datatype datatype;
    int type_size;
} kupl_shm_bcast_args_t;

typedef struct kupl_shm_allreduce_args {
    const void *sendbuf;
    void *recvbuf;
    int count;
    kupl_shm_reduce_op_t op;
    kupl_shm_datatype datatype;
    kupl_shm_allreduce_wins_t wins;
    int type_size;
} kupl_shm_allreduce_args_t;

typedef struct kupl_shm_alltoall_args {
    const void *sendbuf;
    void *recvbuf;
    int sendcount;
    int recvcount;
    kupl_shm_datatype sendtype;
    kupl_shm_datatype recvtype;
    kupl_shm_alltoall_wins_t wins;
    int type_size;
} kupl_shm_alltoall_args_t;

typedef struct kupl_shm_coll_args {
    kupl_shm_coll_type_t type;
    union args {
        kupl_shm_bcast_args_t bcast;
        kupl_shm_allreduce_args_t allreduce;
        kupl_shm_alltoall_args_t alltoall;
    } args_t;
    union algo {
        kupl_shm_bcast_algo_t bcast;
        kupl_shm_allreduce_algo_t allreduce;
        kupl_shm_alltoall_algo_t alltoall;
    } algo_t;
    union wins {
        kupl_shm_bcast_wins_t bcast;
        kupl_shm_allreduce_wins_t allreduce;
        kupl_shm_alltoall_wins_t alltoall;
    } wins_t;
} kupl_shm_coll_args_t;

typedef struct kupl_shm_comm {
    int cid;
    int pid;
    int size;
    int rank;
    kupl_shm_oob_allgather_cb_t oob_allgather;
    kupl_shm_oob_barrier_cb_t oob_fence;
    void *group;
    kupl_list_t win_list;
    kupl_list_t request_list;
    kupl_list_t list;
} *kupl_shm_comm_h, kupl_shm_comm_t;

typedef struct kupl_shm_request {
    kupl_shm_comm_h comm;
    kupl_shm_coll_args_t args;
    size_t id;
    size_t s_offset;
    size_t r_offset;
    kupl_list_t list;
} *kupl_shm_request_h, kupl_shm_request_t;

void kupl_shm_final_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif

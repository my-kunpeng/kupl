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
#include "kupl_alltoall.h"
static int do_alltoall_with_linear_read(kupl_shm_request_h request)
{
    const char *sendbuf = (const char *)request->args.args_t.alltoall.sendbuf;
    char *recvbuf = (char *)request->args.args_t.alltoall.recvbuf;
    kupl_shm_comm_h comm = request->comm;
    kupl_shm_win_h send_win = request->args.args_t.alltoall.wins.send_win;
    int type_size = request->args.args_t.alltoall.type_size;

    int ret = KUPL_OK;
    int recv_rank = 0;
    int rank = comm->rank;
    int comm_size = comm->size;
    int count = request->args.args_t.alltoall.sendcount;
    void *mysend_base;
    kupl_shm_win_query(send_win, rank, &mysend_base);
    int diff = static_cast<int>(sendbuf - (char *)mysend_base);

    kupl_shm_fence(send_win);
    for (int i = 0; i < comm_size; i++) {
        recv_rank = (rank + i) % comm_size;
        void *recv_rank_sendbuf;
        ret = kupl_shm_win_query(send_win, recv_rank, &recv_rank_sendbuf);
        if (ret != 0) {
            kupl_warn("query send_rank recvbuf failed");
            return KUPL_ERROR;
        }
        char *src_ptr = (char *)recv_rank_sendbuf + diff + count * rank * type_size;
        char *dst_ptr = recvbuf + count * recv_rank * type_size;
        ret = kupl_memcpy(dst_ptr, src_ptr, (size_t)(count * type_size));
        if (ret != 0) {
            kupl_warn("kupl memcpy failed");
            return KUPL_ERROR;
        }
    }
    kupl_shm_fence(send_win);

    return KUPL_OK;
}

static int do_alltoall_with_linear_write(kupl_shm_request_h request)
{
    const char *sendbuf = (const char *)request->args.args_t.alltoall.sendbuf;
    char *recvbuf = (char *)request->args.args_t.alltoall.recvbuf;
    kupl_shm_comm_h comm = request->comm;
    kupl_shm_win_h recv_win = request->args.args_t.alltoall.wins.recv_win;
    int type_size = request->args.args_t.alltoall.type_size;

    int ret = KUPL_OK;
    int send_rank = 0;
    int rank = comm->rank;
    int comm_size = comm->size;
    int count = request->args.args_t.alltoall.sendcount;
    void *myrecv_base;
    kupl_shm_win_query(recv_win, rank, &myrecv_base);
    int diff = static_cast<int>(recvbuf - (char *)myrecv_base);

    kupl_shm_fence(recv_win);
    for (int i = 0; i < comm_size; i++) {
        send_rank = (rank + i) % comm_size;
        char *src_ptr = const_cast<char *>(sendbuf) + count * send_rank * type_size;
        void *send_rank_recvbuf;
        ret = kupl_shm_win_query(recv_win, send_rank, &send_rank_recvbuf);
        if (ret != 0) {
            kupl_warn("query send_rank recvbuf failed");
            return KUPL_ERROR;
        }
        char *dst_ptr = (char *)send_rank_recvbuf + diff + count * rank * type_size;
        ret = kupl_memcpy(dst_ptr, src_ptr, (size_t)(count * type_size));
        if (ret != 0) {
            kupl_warn("kupl memcpy failed");
            return KUPL_ERROR;
        }
    }
    kupl_shm_fence(recv_win);

    return KUPL_OK;
}

int kupl_do_alltoall(kupl_shm_request_h request)
{
    kupl_shm_alltoall_algo_t algo = request->args.algo_t.alltoall;
    switch (algo) {
        case KUPL_SHM_ALLTOALL_ALGO_AUTO:
            return do_alltoall_with_linear_write(request);
        case KUPL_SHM_ALLTOALL_ALGO_LINEAR_READ:
            return do_alltoall_with_linear_read(request);
        case KUPL_SHM_ALLTOALL_ALGO_LINEAR_WRITE:
            return do_alltoall_with_linear_write(request);
        default:
            g_is_abnormal_exit = true;
            kupl_fatal("do alltoall attempt to select algorithm %d when only 0-%d is valid?", algo,
                       KUPL_SHM_ALLTOALL_ALGO_MAX - 1);
    }
    return KUPL_ERROR;
}
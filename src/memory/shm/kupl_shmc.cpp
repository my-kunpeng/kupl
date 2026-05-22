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
#include "kupl_shmc.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "kupl_shm.h"
#include "memory/shm/bcast/kupl_bcast.h"
#include "memory/shm/fence/kupl_fence.h"
#include "memory/mpool/kupl_mpool.h"
#include "memory/shm/allreduce/kupl_allreduce.h"
#include "memory/shm/alltoall/kupl_alltoall.h"

static kupl_list_t comm_list;
static bool comm_list_inited = false;

static void kupl_shm_type_size(kupl_shm_datatype datatype, int *size)
{
    switch (datatype) {
        case KUPL_SHM_DATATYPE_CHAR:
            *size = sizeof(char);
            break;
        case KUPL_SHM_DATATYPE_INT:
            *size = sizeof(int);
            break;
        case KUPL_SHM_DATATYPE_LONG:
            *size = sizeof(long);
            break;
        case KUPL_SHM_DATATYPE_FLOAT:
            *size = sizeof(float);
            break;
        case KUPL_SHM_DATATYPE_DOUBLE:
            *size = sizeof(double);
            break;
        default:
            *size = -1;
            break;
    }
}

static kupl_always_inline kupl_shm_alltoall_algo_t alltoall_algo_select()
{
    auto algo = KUPL_SHM_ALLTOALL_ALGO_AUTO;
    std::string algo_str = kupl_config_get_value_str(KUPL_SHM_ALLTOALL_ALGORITHM);
    if (algo_str.length() > 0) {
        if (algo_str == "1") {
            algo = KUPL_SHM_ALLTOALL_ALGO_LINEAR_READ;
        } else if (algo_str == "2") {
            algo = KUPL_SHM_ALLTOALL_ALGO_LINEAR_WRITE;
        }
    }
    return algo;
}

static kupl_always_inline kupl_shm_bcast_algo_t bcast_algo_select()
{
    auto algo = KUPL_SHM_BCAST_ALGO_AUTO;
    std::string algo_str = kupl_config_get_value_str(KUPL_SHM_BCAST_ALGORITHM);
    if (algo_str.length() > 0) {
        if (algo_str == "1") {
            algo = KUPL_SHM_BCAST_ALGO_LINEAR;
        } else if (algo_str == "2") {
            algo = KUPL_SHM_BCAST_ALGO_LINEAR_OPT;
        } else if (algo_str == "3") {
            algo = KUPL_SHM_BCAST_ALGO_TOPO_AWARE_LINEAR;
        } else if (algo_str == "4") {
            algo = KUPL_SHM_BCAST_ALGO_TOPO_AWARE_LINEAR_OPT;
        } else if (algo_str == "5") {
            algo = KUPL_SHM_BCAST_ALGO_RING_PIPELINE;
        } else if (algo_str == "6") {
            algo = KUPL_SHM_BCAST_ALGO_LINEAR_SCATTER_LINEAR_ALLGATHER;
        } else {
            kupl_warn("Unsupported bcast algorithm: %s, change to auto.", algo_str.c_str());
        }
    }
    return algo;
}

static kupl_always_inline kupl_shm_allreduce_algo_t allreduce_algo_select()
{
    auto algo = KUPL_SHM_ALLREDUCE_ALGO_AUTO;
    std::string algo_str = kupl_config_get_value_str(KUPL_SHM_ALLREDUCE_ALGORITHM);
    if (algo_str.length() > 0) {
        if (algo_str == "1") {
            algo = KUPL_SHM_ALLREDUCE_ALGO_LINEAR;
        } else if (algo_str == "2") {
            algo = KUPL_SHM_ALLREDUCE_ALGO_RB_RH_RD;
        } else {
            kupl_warn("Unsupported allreduce algorithm: %s, change to auto.", algo_str.c_str());
        }
    }
    return algo;
}

bool is_buf_valid(const void *buf, const void *base_ptr, size_t max_offset)
{
    return (uintptr_t)buf >= (uintptr_t)base_ptr && (uintptr_t)buf < (uintptr_t)((const char *)base_ptr + max_offset);
}

size_t get_offset(const void *buf, const void *base_ptr)
{
    return (uintptr_t)buf - (uintptr_t)base_ptr;
}

bool kupl_find_win_by_buffer_address(const void *buf, kupl_shm_win_h curr_win)
{
    size_t alloc_size = curr_win->alloc_size;
    void *base_ptr = curr_win->base_ptr;
    bool is_valid = is_buf_valid(buf, base_ptr, alloc_size);
    if (is_valid) {
        curr_win->offset = get_offset(buf, base_ptr);
    }
    return is_valid;
}

static kupl_always_inline kupl_shm_win_h kupl_shm_find_win(const void *buffer, kupl_shm_comm_h comm)
{
    kupl_list_t *head = &comm->win_list;
    kupl_list_t *next = head->next;
    while (next != head) {
        kupl_shm_win_h curr_win = kupl_container_of(next, kupl_shm_win_t, list);
        next = next->next; /* to find next */
        if (kupl_find_win_by_buffer_address(buffer, curr_win)) {
            return curr_win;
        }
    }
    return nullptr;
}

void kupl_shm_final_cleanup()
{
    if (!comm_list_inited) {
        return;
    }
    kupl_list_t *head = &comm_list;
    kupl_list_t *next = head->next;
    while (next != head) {
        kupl_shm_comm_h curr_comm = kupl_container_of(next, kupl_shm_comm_t, list);
        next = next->next; /* to find next */
        kupl_shm_comm_destroy(curr_comm);
    }
}

int kupl_shm_fence(kupl_shm_win_h win)
{
    if (win == nullptr || win->comm == nullptr || win->comm_fence == nullptr) {
        return KUPL_ERROR;
    }
    kupl_fence_wait(win->comm_fence, win->rank, win->size);
    return KUPL_OK;
}

int kupl_shm_peer_fence(kupl_shm_win_h win, int remote_rank)
{
    if (win == nullptr || win->comm == nullptr || win->peer_fence == nullptr) {
        return KUPL_ERROR;
    }
    if (remote_rank < 0 || remote_rank >= win->comm->size) {
        return KUPL_ERROR;
    }
    kupl_fence_wait(win->peer_fence, win->rank, remote_rank);
    return KUPL_OK;
}

int kupl_shm_comm_create(int size, int rank, int pid, kupl_shm_oob_cb_h oob_cbs, void *group, kupl_shm_comm_h *comm)
{
    if (!g_shm_inited && kupl_shm_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (size <= 0 || size > KUPL_SHM_COMM_MAX_SIZE || rank < 0 || rank >= size || pid < 0) {
        g_is_abnormal_exit = true;
        return kupl_log_error_return(FATAL, "size, rank or pid out of range");
    }
    if (comm == nullptr || group == nullptr || oob_cbs == nullptr || oob_cbs->oob_allgather == nullptr ||
        oob_cbs->oob_barrier == nullptr) {
        g_is_abnormal_exit = true;
        return kupl_log_error_return(FATAL, "comm, group or oob_allgather is nullptr");
    }
    *comm = (kupl_shm_comm *)kupl_malloc_inner(sizeof(kupl_shm_comm));
    if (*comm == nullptr) {
        g_is_abnormal_exit = true;
        return kupl_log_error_return(FATAL, "kupl_malloc failed, errno = %s", strerror(errno));
    }
    (*comm)->size = size;
    (*comm)->rank = rank;
    (*comm)->pid = pid;
    (*comm)->oob_allgather = oob_cbs->oob_allgather;
    (*comm)->oob_fence = oob_cbs->oob_barrier;
    (*comm)->group = group;
    kupl_list_init(&(*comm)->win_list);
    kupl_list_init(&(*comm)->request_list);

    if (kupl_unlikely(!comm_list_inited)) {
        kupl_list_init(&comm_list);
        comm_list_inited = true;
    }
    kupl_list_insert_after(&comm_list, &(*comm)->list);
    return KUPL_OK;
}

int kupl_shm_comm_rank(kupl_shm_comm_h comm, int *rank)
{
    if (comm == nullptr || rank == nullptr) {
        return kupl_log_error_return(ERROR, "comm or rank is nullptr");
    }
    *rank = comm->rank;
    return KUPL_OK;
}

int kupl_shm_comm_size(kupl_shm_comm_h comm, int *size)
{
    if (comm == nullptr || size == nullptr) {
        return kupl_log_error_return(ERROR, "comm or size is nullptr");
    }
    *size = comm->size;
    return KUPL_OK;
}

static kupl_always_inline bool kupl_check_type_size(int type_size)
{
    const int valid_sizes[] = {
        sizeof(char), sizeof(int), sizeof(long), sizeof(float), sizeof(double),
    };
    bool valid_type = false;
    for (auto s : valid_sizes) {
        if (type_size == s) {
            valid_type = true;
            break;
        }
    }
    return valid_type;
}

static kupl_always_inline int kupl_allreduce_request_check(kupl_shm_request_h request)
{
    // check wins
    kupl_shm_win_h send_win = request->args.args_t.allreduce.wins.send_win;
    kupl_shm_win_h recv_win = request->args.args_t.allreduce.wins.recv_win;
    if (send_win == nullptr || recv_win == nullptr) {
        return kupl_log_error_return(ERROR, "kupl win is nullptr in allreduce request check");
    }

    // check args
    int count = request->args.args_t.allreduce.count;
    kupl_shm_reduce_op_t op = request->args.args_t.allreduce.op;
    int type_size = request->args.args_t.allreduce.type_size;
    if (op != KUPL_SHM_REDUCE_OP_SUM) {
        return kupl_log_error_return(ERROR, "tmp only support sum ops");
    }
    if (!kupl_check_type_size(type_size)) {
        return kupl_log_error_return(ERROR, "type_size is invalid");
    }
    if (count <= 0) {
        return kupl_log_error_return(ERROR, "invalid count");
    }

    // check buf
    const void *sendbuf = request->args.args_t.allreduce.sendbuf;
    void *recvbuf = request->args.args_t.allreduce.recvbuf;
    if (sendbuf == nullptr || recvbuf == nullptr) {
        return kupl_log_error_return(ERROR, "comm, request, sendbuf or recvbuf is nullptr");
    }

    // check algo
    kupl_shm_comm_h comm = request->comm;
    if (comm->size == 1) {
        request->args.algo_t.allreduce = KUPL_SHM_ALLREDUCE_ALGO_RB_RH_RD;
    }
    return KUPL_OK;
}

static kupl_always_inline int kupl_bcast_request_check(kupl_shm_request_h request)
{
    // check wins
    kupl_shm_win_h win = request->args.wins_t.bcast.win;
    kupl_shm_win_h notify_others_win = request->args.wins_t.bcast.notify_others_win;
    kupl_shm_win_h notify_root_win = request->args.wins_t.bcast.notify_root_win;
    kupl_shm_win_h p2p_win = request->args.wins_t.bcast.p2p_win;
    if (win == nullptr || notify_others_win == nullptr || notify_root_win == nullptr || p2p_win == nullptr) {
        return kupl_log_error_return(ERROR, "bcast wins is null");
    }
    // check args
    kupl_shm_comm_h comm = request->comm;
    int type_size = request->args.args_t.bcast.type_size;
    int count = request->args.args_t.bcast.count;
    int root = request->args.args_t.bcast.root;
    if (!kupl_check_type_size(type_size)) {
        return kupl_log_error_return(ERROR, "type_size is invalid");
    }
    if (count <= 0) {
        return kupl_log_error_return(ERROR, "invalid count");
    }
    if (root < 0 || root >= comm->size) {
        return kupl_log_error_return(ERROR, "invalid root");
    }

    // check buf
    void *buffer = request->args.args_t.bcast.buffer;
    if (buffer == nullptr) {
        return kupl_log_error_return(ERROR, "bcast buffer is null");
    }

    // check algo
    if (request->args.algo_t.bcast == KUPL_SHM_BCAST_ALGO_TOPO_AWARE_LINEAR && root != 0) {
        kupl_warn("Topo_aware_linear algorithm not support root != 0, change to auto.");
        request->args.algo_t.bcast = KUPL_SHM_BCAST_ALGO_AUTO;
    }
    if (request->args.algo_t.bcast == KUPL_SHM_BCAST_ALGO_TOPO_AWARE_LINEAR_OPT && root != 0) {
        kupl_warn("Topo_aware_linear_opt algorithm not support root != 0, change to auto.");
        request->args.algo_t.bcast = KUPL_SHM_BCAST_ALGO_AUTO;
    }
    return KUPL_OK;
}

static kupl_always_inline int kupl_alltoall_request_check(kupl_shm_request_h request)
{
    // check wins
    kupl_shm_win_h send_win = request->args.args_t.alltoall.wins.send_win;
    kupl_shm_win_h recv_win = request->args.args_t.alltoall.wins.recv_win;
    kupl_shm_alltoall_algo_t algo = request->args.algo_t.alltoall;
    if ((recv_win == nullptr && send_win == nullptr) ||
        (recv_win == nullptr && algo == KUPL_SHM_ALLTOALL_ALGO_LINEAR_WRITE) ||
        (send_win == nullptr && algo == KUPL_SHM_ALLTOALL_ALGO_LINEAR_READ)) {
        return kupl_log_error_return(ERROR, "kupl alltoall recv win or send win not found!");
    }

    // check args
    kupl_shm_comm_h comm = request->comm;
    int type_size = request->args.args_t.alltoall.type_size;
    int comm_size = comm->size;
    int count = request->args.args_t.alltoall.sendcount;
    if (!kupl_check_type_size(type_size)) {
        return kupl_log_error_return(ERROR, "type_size is invalid");
    }
    if (count <= 0 || count > INT_MAX / (comm_size * type_size)) {
        return kupl_log_error_return(ERROR, "invalid count");
    }
    if (UINT_MAX / (unsigned int)count < (unsigned int)type_size) {
        return kupl_log_error_return(ERROR, "invalid count: too big");
    }

    // check buf
    const void *sendbuf = request->args.args_t.alltoall.sendbuf;
    void *recvbuf = request->args.args_t.alltoall.recvbuf;
    if (sendbuf == nullptr || recvbuf == nullptr) {
        return kupl_log_error_return(ERROR, "comm, request, sendbuf or recvbuf is nullptr");
    }
    return KUPL_OK;
}

static kupl_always_inline int kupl_shm_request_param_check(kupl_shm_request_h request)
{
    if (request == nullptr) {
        return kupl_log_error_return(ERROR, "request is nullptr");
    }
    if (request->comm == nullptr) {
        return kupl_log_error_return(ERROR, "kupl comm is nullptr");
    }
    int comm_size = request->comm->size;
    int rank = request->comm->rank;
    if (comm_size <= 0 || comm_size > KUPL_SHM_COMM_MAX_SIZE || rank < 0 || rank >= comm_size) {
        return kupl_log_error_return(ERROR, "comm size or rank is out of range");
    }
    return KUPL_OK;
}

int kupl_shm_request_start(kupl_shm_request_h request)
{
    if (!g_shm_inited && kupl_shm_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    int ret = kupl_shm_request_param_check(request);
    if (ret == KUPL_ERROR) {
        kupl_safe_free(request);
        g_is_abnormal_exit = true;
        return kupl_log_error_return(FATAL, "request param check failed...");
    }
    switch (request->args.type) {
        case KUPL_SHM_COLL_TYPE_ALLREDUCE:
            ret = kupl_allreduce_request_check(request);
            if (ret == KUPL_OK) {
                ret = kupl_do_allreduce(request);
            }
            break;
        case KUPL_SHM_COLL_TYPE_BCAST:
            ret = kupl_bcast_request_check(request);
            if (ret == KUPL_OK) {
                ret = kupl_do_bcast(request);
            }
            break;
        case KUPL_SHM_COLL_TYPE_ALLTOALL:
            ret = kupl_alltoall_request_check(request);
            if (ret == KUPL_OK) {
                ret = kupl_do_alltoall(request);
            }
            break;
        default:
            kupl_error("unknown coll type...");
            ret = KUPL_ERROR;
            break;
    }
    if (ret == KUPL_ERROR) {
        kupl_safe_free(request);
        g_is_abnormal_exit = true;
        kupl_fatal("request start failed...");
    }
    return KUPL_OK;
}

int kupl_shm_request_wait(kupl_shm_request_h request)
{
    if (!g_shm_inited && kupl_shm_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (request == nullptr) {
        g_is_abnormal_exit = true;
        return kupl_log_error_return(FATAL, "request is nullptr");
    }
    return KUPL_OK;
}

int kupl_shm_request_free(kupl_shm_request_h request)
{
    if (request == nullptr) {
        return kupl_log_error_return(ERROR, "request is nullptr");
    }
    if (request->args.type == KUPL_SHM_COLL_TYPE_BCAST) {
        kupl_shm_win_free(request->args.wins_t.bcast.notify_root_win);
        kupl_shm_win_free(request->args.wins_t.bcast.notify_others_win);
        kupl_shm_win_free(request->args.wins_t.bcast.p2p_win);
    }
    kupl_list_del(&request->list);
    kupl_safe_free(request);
    return KUPL_OK;
}

int kupl_shm_comm_destroy(kupl_shm_comm_h comm)
{
    if (comm == nullptr) {
        return kupl_log_error_return(ERROR, "comm is nullptr, no destroy");
    }
    kupl_list_t *head = &comm->request_list;
    kupl_list_t *next = head->next;
    while (next != head) {
        kupl_shm_request_h curr_request = kupl_container_of(next, kupl_shm_request_t, list);
        next = next->next; /* to find next */
        kupl_shm_request_free(curr_request);
    }
    head = &comm->win_list;
    next = head->next;
    while (next != head) {
        kupl_shm_win_h curr_win = kupl_container_of(next, kupl_shm_win_t, list);
        next = next->next; /* to find next */
        kupl_shm_win_free(curr_win);
    }
    kupl_list_del(&comm->list);
    kupl_safe_free(comm);
    return KUPL_OK;
}

static kupl_always_inline int kupl_shm_allreduce_param_check(const void *sendbuf, void *recvbuf, int count,
                                                             kupl_shm_reduce_op_t op, kupl_shm_comm_h comm,
                                                             kupl_shm_request_h *request)
{
    if (count <= 0) {
        return kupl_log_error_return(ERROR, "invalid count");
    }
    if (request == nullptr || sendbuf == nullptr || recvbuf == nullptr || comm == nullptr) {
        return kupl_log_error_return(ERROR, "comm, request, sendbuf or recvbuf is nullptr");
    }
    if (op != KUPL_SHM_REDUCE_OP_SUM) {
        return kupl_log_error_return(ERROR, "tmp only support sum ops");
    }
    return KUPL_OK;
}

int kupl_shm_allreduce_init(const void *sendbuf, void *recvbuf, int count, kupl_shm_datatype datatype,
                            kupl_shm_reduce_op_t op, kupl_shm_comm_h comm, kupl_shm_request_h *request)
{
    if (!g_shm_inited && kupl_shm_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    int ret = kupl_shm_allreduce_param_check(sendbuf, recvbuf, count, op, comm, request);
    if (ret == KUPL_ERROR) {
        g_is_abnormal_exit = true;
        return kupl_log_error_return(FATAL, "kupl shm allreduce param check failed");
    }
    *request = (kupl_shm_request *)kupl_malloc_inner(sizeof(kupl_shm_request));
    if (*request == nullptr) {
        g_is_abnormal_exit = true;
        return kupl_log_error_return(FATAL, "malloc failed, errno = %s", strerror(errno));
    }

    kupl_shm_win_h recv_win = kupl_shm_find_win(recvbuf, comm);
    kupl_shm_win_h send_win = kupl_shm_find_win(sendbuf, comm);
    if (recv_win == nullptr || send_win == nullptr) {
        kupl_safe_free(*request);
        g_is_abnormal_exit = true;
        return kupl_log_error_return(FATAL, "kupl_shm_allreduce_init recv win or send win not found!");
    }

    (*request)->comm = comm;
    (*request)->args.args_t.allreduce.wins.send_win = send_win;
    (*request)->args.args_t.allreduce.wins.recv_win = recv_win;
    (*request)->args.type = KUPL_SHM_COLL_TYPE_ALLREDUCE;
    (*request)->args.args_t.allreduce.sendbuf = sendbuf;
    (*request)->args.args_t.allreduce.recvbuf = recvbuf;
    (*request)->args.args_t.allreduce.count = count;
    (*request)->args.args_t.allreduce.op = op;
    (*request)->args.args_t.allreduce.datatype = datatype;
    kupl_shm_type_size(datatype, &((*request)->args.args_t.allreduce.type_size));
    if ((*request)->args.args_t.allreduce.type_size == -1) {
        kupl_safe_free(*request);
        return kupl_log_error_return(FATAL, "invalid typesize\n");
    }
    (*request)->args.algo_t.allreduce = allreduce_algo_select();
    if (comm->size == 1) {
        (*request)->args.algo_t.allreduce = KUPL_SHM_ALLREDUCE_ALGO_RB_RH_RD;
    }
    (*request)->s_offset = get_offset(sendbuf, send_win->base_ptr);
    (*request)->r_offset = get_offset(recvbuf, recv_win->base_ptr);
    kupl_list_insert_after(&comm->request_list, &(*request)->list);

    return KUPL_OK;
}

static kupl_always_inline int kupl_shm_bcast_param_check(void *buffer, int count, int root, kupl_shm_comm_h comm,
                                                         kupl_shm_request_h *request)
{
    if (count <= 0) {
        return kupl_log_error_return(ERROR, "count can not less than or equal 0!");
    }
    if (request == nullptr || buffer == nullptr || comm == nullptr) {
        return kupl_log_error_return(ERROR, "request, buffer or comm is nullptr");
    }
    if (root < 0 || root >= comm->size) {
        return kupl_log_error_return(ERROR, "invalid root");
    }
    return KUPL_OK;
}

static kupl_always_inline void kupl_shm_bcast_fill_request(void *buffer, int count, kupl_shm_datatype datatype,
                                                           int root, kupl_shm_comm_h comm, kupl_shm_request_h *request)
{
    (*request)->comm = comm;
    (*request)->args.type = KUPL_SHM_COLL_TYPE_BCAST;
    (*request)->args.args_t.bcast.buffer = buffer;
    (*request)->args.args_t.bcast.count = count;
    (*request)->args.args_t.bcast.datatype = datatype;
    kupl_shm_type_size(datatype, &((*request)->args.args_t.bcast.type_size));
    (*request)->args.args_t.bcast.root = root;
    (*request)->args.algo_t.bcast = bcast_algo_select();
    if ((*request)->args.algo_t.bcast == KUPL_SHM_BCAST_ALGO_TOPO_AWARE_LINEAR && root != 0) {
        kupl_warn("Topo_aware_linear algorithm not support root != 0, change to auto.");
        (*request)->args.algo_t.bcast = KUPL_SHM_BCAST_ALGO_AUTO;
    }
    if ((*request)->args.algo_t.bcast == KUPL_SHM_BCAST_ALGO_TOPO_AWARE_LINEAR_OPT && root != 0) {
        kupl_warn("Topo_aware_linear_opt algorithm not support root != 0, change to auto.");
        (*request)->args.algo_t.bcast = KUPL_SHM_BCAST_ALGO_AUTO;
    }
}

static kupl_always_inline int kupl_shm_bcast_request_args_set(void *buffer, int count, kupl_shm_datatype datatype,
                                                              int root, kupl_shm_comm_h comm,
                                                              kupl_shm_request_h *request)
{
    void *notify_root_buffer;
    void *notify_others_buffer;
    void *p2p_buffer;

    int ret = kupl_shm_win_alloc((size_t)comm->size * sizeof(int), comm, &notify_root_buffer,
                                 &((*request)->args.wins_t.bcast.notify_root_win));
    if (ret == KUPL_ERROR) {
        goto err;
    }
    memset(notify_root_buffer, 0, (size_t)comm->size * sizeof(int));

    ret = kupl_shm_win_alloc((size_t)comm->size * sizeof(int), comm, &notify_others_buffer,
                             &((*request)->args.wins_t.bcast.notify_others_win));
    if (ret == KUPL_ERROR) {
        goto err_notify_others;
    }
    memset(notify_others_buffer, 0, (size_t)comm->size * sizeof(int));

    ret = kupl_shm_win_alloc((size_t)comm->size * sizeof(int), comm, &p2p_buffer,
                             &((*request)->args.wins_t.bcast.p2p_win));
    if (ret == KUPL_ERROR) {
        goto err_p2p;
    }
    memset(p2p_buffer, 0, (size_t)comm->size * sizeof(int));

    kupl_shm_bcast_fill_request(buffer, count, datatype, root, comm, request);
    if ((*request)->args.args_t.bcast.type_size == -1) {
        goto err_bcast_fill;
    }
    return KUPL_OK;

err_bcast_fill:
    kupl_shm_win_free((*request)->args.wins_t.bcast.p2p_win);
err_p2p:
    kupl_shm_win_free((*request)->args.wins_t.bcast.notify_others_win);
err_notify_others:
    kupl_shm_win_free((*request)->args.wins_t.bcast.notify_root_win);
err:
    return KUPL_ERROR;
}

int kupl_shm_bcast_init(void *buffer, int count, kupl_shm_datatype datatype, int root, kupl_shm_comm_h comm,
                        kupl_shm_request_h *request)
{
    if (!g_shm_inited && kupl_shm_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    int ret = kupl_shm_bcast_param_check(buffer, count, root, comm, request);
    if (ret == KUPL_ERROR) {
        g_is_abnormal_exit = true;
        return kupl_log_error_return(FATAL, "kupl shm bcast param check failed");
    }
    *request = (kupl_shm_request *)kupl_malloc_inner(sizeof(kupl_shm_request));
    if (*request == nullptr) {
        g_is_abnormal_exit = true;
        return kupl_log_error_return(FATAL, "kupl_malloc failed, errno = %s", strerror(errno));
    }

    kupl_shm_win_h win = kupl_shm_find_win(buffer, comm);
    if (win == nullptr) {
        kupl_safe_free(*request);
        g_is_abnormal_exit = true;
        return kupl_log_error_return(FATAL, "kupl_shm_bcast_init win not found!");
    }
    (*request)->args.wins_t.bcast.win = win;

    ret = kupl_shm_bcast_request_args_set(buffer, count, datatype, root, comm, request);
    if (ret == KUPL_ERROR) {
        goto err_request_args_set;
    }

    kupl_list_insert_after(&comm->request_list, &(*request)->list);
    kupl_shm_fence((*request)->args.wins_t.bcast.win);

    return KUPL_OK;

err_request_args_set:
    kupl_safe_free(*request);
    g_is_abnormal_exit = true;
    return kupl_log_error_return(FATAL, "bcast init failed");
}

int kupl_shm_alltoall_init(const void *sendbuf, int sendcount, kupl_shm_datatype_t sendtype, void *recvbuf,
                           int recvcount, kupl_shm_datatype_t recvtype, kupl_shm_comm_h comm,
                           kupl_shm_request_h *request)
{
    if (!g_shm_inited && kupl_shm_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (sendcount <= 0 || recvcount <= 0) {
        return kupl_log_error_return(ERROR, "invalid counts");
    }
    if (request == nullptr || sendbuf == nullptr || comm == nullptr || recvbuf == nullptr) {
        return kupl_log_error_return(ERROR, "request, buffer or comm is nullptr");
    }

    *request = (kupl_shm_request *)kupl_malloc_inner(sizeof(kupl_shm_request));
    if (*request == nullptr) {
        return kupl_log_error_return(ERROR, "kupl_malloc failed, errno = %s", strerror(errno));
    }

    kupl_shm_win_h recv_win = kupl_shm_find_win(recvbuf, comm);
    kupl_shm_win_h send_win = kupl_shm_find_win(sendbuf, comm);
    kupl_shm_alltoall_algo_t algo = alltoall_algo_select();
    if ((recv_win == nullptr && send_win == nullptr) ||
        (recv_win == nullptr && algo == KUPL_SHM_ALLTOALL_ALGO_LINEAR_WRITE) ||
        (send_win == nullptr && algo == KUPL_SHM_ALLTOALL_ALGO_LINEAR_READ)) {
        kupl_safe_free(*request);
        return kupl_log_error_return(ERROR, "kupl_shm_alltoall_init recv win or send win not found!");
    }
    (*request)->comm = comm;
    (*request)->args.args_t.alltoall.wins.send_win = send_win;
    (*request)->args.args_t.alltoall.wins.recv_win = recv_win;
    (*request)->args.type = KUPL_SHM_COLL_TYPE_ALLTOALL;
    (*request)->args.args_t.alltoall.sendbuf = sendbuf;
    (*request)->args.args_t.alltoall.recvbuf = recvbuf;
    (*request)->args.args_t.alltoall.sendcount = sendcount;
    (*request)->args.args_t.alltoall.recvcount = recvcount;
    (*request)->args.args_t.alltoall.sendtype = sendtype;
    (*request)->args.args_t.alltoall.recvtype = recvtype;
    kupl_shm_type_size(sendtype, &((*request)->args.args_t.alltoall.type_size));
    if ((*request)->args.args_t.alltoall.type_size == -1) {
        kupl_safe_free(*request);
        return kupl_log_error_return(FATAL, "invalid typesize\n");
    }
    (*request)->args.algo_t.alltoall = algo;
    kupl_list_insert_after(&comm->request_list, &(*request)->list);
    return KUPL_OK;
}

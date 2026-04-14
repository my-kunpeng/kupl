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
#include "kupl_bcast.h"

static void notify_root(int *flags, int start, int end, int myid, int root)
{
    if (myid == root) {
        kupl_memory_cpu_store_fence();
        flags[myid]++;
        for (int i = start; i < end; i++) {
            if (myid != i) {
                do {
                    kupl_memory_cpu_load_fence();
                } while (flags[myid] > flags[i]);
            }
        }
    } else {
        kupl_memory_cpu_store_fence();
        flags[myid]++;
    }
}

static void notify_others(int *flags, int myid, int root)
{
    if (myid == root) {
        kupl_memory_cpu_store_fence();
        flags[root]++;
    } else {
        do {
            kupl_memory_cpu_load_fence();
        } while (flags[root] <= flags[myid]);
        kupl_memory_cpu_store_fence();
        flags[myid]++;
    }
}

static int do_bcast_linear(void *buffer, int data_size, int root, kupl_shm_comm_h comm, kupl_shm_win_h win,
                           kupl_shm_win_h notify_root_win, kupl_shm_win_h notify_others_win)
{
    int myid = comm->rank;
    int numprocs = comm->size;
    void *notify_others_flags;
    void *notify_root_flags;
    int ret = kupl_shm_win_query(notify_others_win, root, &notify_others_flags);
    if (kupl_unlikely(ret == KUPL_ERROR)) {
        return KUPL_ERROR;
    }
    notify_others((int *)notify_others_flags, myid, root);

    if (myid != root) {
        void *root_ptr;
        ret = kupl_shm_win_query(win, root, &root_ptr);
        if (kupl_unlikely(ret == KUPL_ERROR)) {
            return KUPL_ERROR;
        }
        root_ptr = (char *)root_ptr + win->offset;
        kupl_memcpy(buffer, root_ptr, (size_t)(data_size));
    }
    ret = kupl_shm_win_query(notify_root_win, root, &notify_root_flags);
    if (kupl_unlikely(ret == KUPL_ERROR)) {
        return KUPL_ERROR;
    }
    notify_root((int *)notify_root_flags, 0, numprocs, myid, root);
    return KUPL_OK;
}

static int do_bcast_linear_topo(void *buffer, int data_size, int root, kupl_shm_comm_h comm, kupl_shm_win_h win,
                                kupl_shm_win_h notify_root_win, kupl_shm_win_h notify_others_win)
{
    int split = kupl_config_get_value(KUPL_BCAST_TOPO_SPLIT);
    int myid = comm->rank;
    int numprocs = comm->size;
    int subsize = (numprocs + split - 1) / split;

    void *local_leader_ptr;
    void *notify_root_flags;
    void *notify_others_flags;

    int ngroup = myid / subsize;
    int local_leader_id = subsize * ngroup;

    kupl_shm_win_query(notify_others_win, root, &notify_others_flags);
    notify_others((int *)notify_others_flags, myid, root);

    if (myid > 0 && myid % subsize == 0) {
        void *root_ptr;
        kupl_shm_win_query(win, root, &root_ptr);
        root_ptr = (char *)root_ptr + win->offset;
        kupl_memcpy(buffer, root_ptr, (size_t)(data_size));
    }

    if (ngroup > 0) {
        kupl_shm_win_query(notify_others_win, local_leader_id, &notify_others_flags);
        notify_others((int *)notify_others_flags, myid, local_leader_id);
    }

    if (myid != local_leader_id) {
        kupl_shm_win_query(win, local_leader_id, &local_leader_ptr);
        local_leader_ptr = (char *)local_leader_ptr + win->offset;
        kupl_memcpy(buffer, local_leader_ptr, (size_t)(data_size));
    }
    kupl_shm_win_query(notify_root_win, local_leader_id, &notify_root_flags);
    notify_root((int *)notify_root_flags, local_leader_id, kupl_min(numprocs, (ngroup + 1) * subsize),
                myid, local_leader_id);
    return KUPL_OK;
}

static int do_bcast_linear_opt(void *buffer, int data_size, int root, kupl_shm_comm_h comm, kupl_shm_win_h win,
                               kupl_shm_win_h notify_root_win, kupl_shm_win_h notify_others_win,
                               kupl_shm_win_h p2p_win)
{
    int myid = comm->rank;
    int numprocs = comm->size;
    int root_memcpy_size = data_size / numprocs;
    int left_memcpy_size = data_size - root_memcpy_size;
    int p2p_win_val;

    void *notify_others_flags;
    void *notify_root_flags;
    void *i_ptr;
    void *p2p_ptr;

    kupl_shm_win_query(p2p_win, root, &p2p_ptr);
    if (myid == root) {
        ((int *)p2p_ptr)[root]++;
    }

    kupl_shm_win_query(notify_others_win, root, &notify_others_flags);
    notify_others((int *)notify_others_flags, myid, root);
    p2p_win_val = ((int *)p2p_ptr)[root];
    kupl_shm_win_query(notify_root_win, root, &notify_root_flags);
    notify_root((int *)notify_root_flags, 0, numprocs, myid, root);

    if (myid != root) {
        void *root_ptr;
        kupl_shm_win_query(win, root, &root_ptr);
        root_ptr = (char *)root_ptr + win->offset;
        kupl_memcpy((char *)buffer + root_memcpy_size, (char *)root_ptr + root_memcpy_size, (size_t)left_memcpy_size);
    } else {
        for (int i = 0; i < numprocs; i++) {
            if (i == root) {
                continue;
            }
            kupl_shm_win_query(win, i, &i_ptr);
            i_ptr = (char *)i_ptr + win->offset;
            kupl_memcpy(i_ptr, buffer, (size_t)root_memcpy_size);
            kupl_memory_cpu_store_fence();
            ((int *)p2p_ptr)[i]++;
        }
    }
    notify_root((int *)notify_root_flags, 0, numprocs, myid, root);

    if (myid != root) {
        do {
            kupl_memory_cpu_load_fence();
        } while (((int *)p2p_ptr)[myid] < p2p_win_val);
    }

    return KUPL_OK;
}

static int do_bcast_linear_topo_opt(void *buffer, int data_size, int root, kupl_shm_comm_h comm, kupl_shm_win_h win,
                                    kupl_shm_win_h notify_root_win, kupl_shm_win_h notify_others_win,
                                    kupl_shm_win_h p2p_win)
{
    int split = kupl_config_get_value(KUPL_BCAST_TOPO_SPLIT);
    int myid = comm->rank;
    int numprocs = comm->size;
    int subsize = (numprocs + split - 1) / split;
    int p2p_win_val;

    void *local_leader_ptr;
    void *notify_root_flags;
    void *notify_others_flags;

    void *child_ptr;
    void *p2p_ptr;

    int ngroup = myid / subsize;
    int local_leader_id = subsize * ngroup;
    int ret;

    ret = kupl_shm_win_query(p2p_win, root, &p2p_ptr);
    if (kupl_unlikely(ret == KUPL_ERROR)) {
        return KUPL_ERROR;
    }
    if (myid == root) {
        ((int *)p2p_ptr)[root]++;
    }
    p2p_win_val = ((int *)p2p_ptr)[root];

    ret = kupl_shm_win_query(notify_others_win, root, &notify_others_flags);
    if (kupl_unlikely(ret == KUPL_ERROR)) {
        return KUPL_ERROR;
    }
    notify_others((int *)notify_others_flags, myid, root);

    if (myid > 0 && myid % subsize == 0) {
        void *root_ptr;
        ret = kupl_shm_win_query(win, root, &root_ptr);
        if (kupl_unlikely(ret == KUPL_ERROR)) {
            return KUPL_ERROR;
        }
        root_ptr = (char *)root_ptr + win->offset;
        kupl_memcpy(buffer, root_ptr, (size_t)(data_size));
    }

    if (ngroup > 0) {
        ret = kupl_shm_win_query(notify_others_win, local_leader_id, &notify_others_flags);
        if (kupl_unlikely(ret == KUPL_ERROR)) {
            return KUPL_ERROR;
        }
        notify_others((int *)notify_others_flags, myid, local_leader_id);
    }

    int leader_memcpy_size;
    int left_memcpy_size;
    if (ngroup != split - 1) {
        leader_memcpy_size = data_size / subsize;
    } else {
        leader_memcpy_size = data_size / (numprocs - local_leader_id);
    }
    left_memcpy_size = data_size - leader_memcpy_size;

    ret = kupl_shm_win_query(notify_root_win, local_leader_id, &notify_root_flags);
    if (kupl_unlikely(ret == KUPL_ERROR)) {
        return KUPL_ERROR;
    }
    notify_root((int *)notify_root_flags, local_leader_id, kupl_min(numprocs, (ngroup + 1) * subsize),
                myid, local_leader_id);

    if (myid != local_leader_id) {
        ret = kupl_shm_win_query(win, local_leader_id, &local_leader_ptr);
        if (kupl_unlikely(ret == KUPL_ERROR)) {
            return KUPL_ERROR;
        }
        local_leader_ptr = (char *)local_leader_ptr + win->offset;
        kupl_memcpy((char *)buffer + leader_memcpy_size,
                    (char *)local_leader_ptr + leader_memcpy_size,
                    (size_t)left_memcpy_size);
    } else {
        for (int i = myid + 1; i < kupl_min(myid + subsize, numprocs); i++) {
            ret = kupl_shm_win_query(win, i, &child_ptr);
            if (kupl_unlikely(ret == KUPL_ERROR)) {
                return KUPL_ERROR;
            }
            child_ptr = (char *)child_ptr + win->offset;
            kupl_memcpy(child_ptr, buffer, (size_t)leader_memcpy_size);
            ret = kupl_shm_win_query(p2p_win, i, &p2p_ptr);
            if (kupl_unlikely(ret == KUPL_ERROR)) {
                return KUPL_ERROR;
            }
            kupl_memory_cpu_store_fence();
            ((int *)p2p_ptr)[i]++;
        }
    }

    ret = kupl_shm_win_query(notify_root_win, local_leader_id, &notify_root_flags);
    if (kupl_unlikely(ret == KUPL_ERROR)) {
        return KUPL_ERROR;
    }
    notify_root((int *)notify_root_flags, local_leader_id, kupl_min(numprocs, (ngroup + 1) * subsize),
                myid, local_leader_id);

    if (myid != local_leader_id) {
        ret = kupl_shm_win_query(p2p_win, myid, &p2p_ptr);
        if (kupl_unlikely(ret == KUPL_ERROR)) {
            return KUPL_ERROR;
        }
        do {
            kupl_memory_cpu_load_fence();
        } while (((int *)p2p_ptr)[myid] < p2p_win_val);
    }

    return KUPL_OK;
}

static int do_bcast_ring_pipeline(void *buffer, int count, int root, kupl_shm_comm_h comm, kupl_shm_win_h win,
                                  kupl_shm_win_h notify_root_win, kupl_shm_win_h notify_others_win)
{
    int myid = comm->rank;
    int numprocs = comm->size;
    int chunk_size = 8192;
    int num_chunks = (count + chunk_size - 1) / chunk_size;

    int next = (myid + 1) % numprocs;
    int prev = (myid - 1 + numprocs) % numprocs;

    void *notify_root_flags;
    void *notify_others_flags;

    kupl_shm_win_query(notify_others_win, root, &notify_others_flags);
    notify_others((int *)notify_others_flags, myid, root);

    kupl_shm_win_query(notify_root_win, root, &notify_root_flags);
    notify_root((int *)notify_root_flags, 0, numprocs, myid, root);

    void *next_ptr;
    kupl_shm_win_query(win, next, &next_ptr);
    next_ptr = (char *)next_ptr + win->offset;

    for (int i = 0; i < num_chunks; i++) {
        int curr_chuck_size = (i == num_chunks - 1) ? (count - i * chunk_size) : chunk_size;
        char *chunk_buffer = (char *)buffer + i * chunk_size;
        char *next_ptr_buffer = (char *)next_ptr + i * chunk_size;
        if (myid == root) {
            kupl_memcpy(next_ptr_buffer, chunk_buffer, (size_t)curr_chuck_size);
            kupl_shm_win_query(notify_others_win, root, &notify_others_flags);
            notify_others((int *)notify_others_flags, myid, myid);
        } else {
            kupl_shm_win_query(notify_others_win, prev, &notify_others_flags);
            notify_others((int *)notify_others_flags, myid, prev);

            if (myid != (root + numprocs - 1) % numprocs) {
                kupl_memcpy(next_ptr_buffer, chunk_buffer, (size_t)curr_chuck_size);
                kupl_shm_win_query(notify_others_win, myid, &notify_others_flags);
                notify_others((int *)notify_others_flags, myid, myid);
            }
        }
    }
    return KUPL_OK;
}

static kupl_always_inline
int calc_offset(int rank, int div, int rem)
{
    if (rank - 1 < rem) {
        return rank * (div + 1);
    }
    return rem * (div + 1) + (rank - rem) * div;
}

int do_bcast_linear_scatter_allgather(void *buffer, int data_size, int root, kupl_shm_comm_h comm, kupl_shm_win_h win,
                                      kupl_shm_win_h notify_others_win)
{
    int rank = comm->rank;
    int numprocs = comm->size;
    int div = data_size / numprocs;
    int rem = data_size % numprocs;
    int offset;
    void *notify_others_flags;

    kupl_shm_win_query(notify_others_win, root, &notify_others_flags);
    notify_others((int *)notify_others_flags, rank, root);

    // linear scatter
    if (rank != root) {
        void *root_ptr;
        kupl_shm_win_query(win, root, &root_ptr);
        root_ptr = (char *)root_ptr + win->offset;
        int block_size = div;
        if (rank < rem) {
            block_size = div + 1;
        }
        offset = calc_offset(rank, div, rem);
        kupl_memcpy((char *)buffer + offset, (char *)root_ptr + offset, (size_t)block_size);
    }
    kupl_shm_fence(win);

    // linear allgather
    for (int i = 0; i < numprocs; i++) {
        if (rank != i) {
            void *remote_buf;
            kupl_shm_win_query(win, i, &remote_buf);
            remote_buf = (char *)remote_buf + win->offset;
            int remote_block_size = div;
            if (i < rem) {
                remote_block_size = div + 1;
            }
            offset = calc_offset(i, div, rem);
            kupl_memcpy((char *)buffer + offset, (char *)remote_buf + offset, (size_t)remote_block_size);
        }
    }
    return KUPL_OK;
}

int kupl_do_bcast(kupl_shm_request_h request)
{
    kupl_shm_bcast_algo_t algo = request->args.algo_t.bcast;
    int root = request->args.args_t.bcast.root;
    void *buffer = request->args.args_t.bcast.buffer;
    int count = request->args.args_t.bcast.count;
    int type_size = request->args.args_t.bcast.type_size;
    kupl_shm_comm_h comm = request->comm;

    kupl_shm_bcast_wins_t wins = request->args.wins_t.bcast;
    kupl_shm_win_h win = wins.win;
    kupl_shm_win_h notify_root_win = wins.notify_root_win;
    kupl_shm_win_h notify_others_win = wins.notify_others_win;
    kupl_shm_win_h p2p_win = wins.p2p_win;

    int data_size = count * type_size;
    if (data_size <= 0) {
        return kupl_log_error_return(ERROR, "invalid count with type_size");
    }

    switch (algo) {
        case KUPL_SHM_BCAST_ALGO_AUTO:
            return do_bcast_ring_pipeline(buffer, data_size, root, comm, win, notify_root_win, notify_others_win);
        case KUPL_SHM_BCAST_ALGO_LINEAR:
            return do_bcast_linear(buffer, data_size, root, comm, win, notify_root_win, notify_others_win);
        case KUPL_SHM_BCAST_ALGO_LINEAR_OPT:
            return do_bcast_linear_opt(buffer, data_size, root, comm, win,
                                       notify_root_win, notify_others_win, p2p_win);
        case KUPL_SHM_BCAST_ALGO_TOPO_AWARE_LINEAR:
            return do_bcast_linear_topo(buffer, data_size, root, comm, win, notify_root_win, notify_others_win);
        case KUPL_SHM_BCAST_ALGO_TOPO_AWARE_LINEAR_OPT:
            return do_bcast_linear_topo_opt(buffer, data_size, root, comm, win,
                                            notify_root_win, notify_others_win, p2p_win);
        case KUPL_SHM_BCAST_ALGO_RING_PIPELINE:
            return do_bcast_ring_pipeline(buffer, data_size, root, comm, win, notify_root_win, notify_others_win);
        case KUPL_SHM_BCAST_ALGO_LINEAR_SCATTER_LINEAR_ALLGATHER:
            return do_bcast_linear_scatter_allgather(buffer, data_size, root, comm, win, notify_others_win);
        default:
            kupl_error("do bcast attempt to select algorithm %d when only 0-%d is valid?", algo,
                       KUPL_SHM_BCAST_ALGO_MAX - 1);
    }
    return KUPL_ERROR;
}
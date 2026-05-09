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
#include "kupl_allreduce.h"

static kupl_always_inline int kupl_shm_send(void *buf, void *destbuf, size_t offset, unsigned int count,
                                            unsigned int type_size)
{
    if (count == 0) {
        return KUPL_OK;
    }
    auto t_destbuf = static_cast<char *>(destbuf);
    auto t_buf = static_cast<char *>(buf);
    if (SIZE_MAX / count < type_size) {
        return kupl_log_error_return(ERROR, "kupl_shm_send size too big");
    }
    int ret = kupl_memcpy(t_destbuf + offset * type_size, t_buf + offset * type_size, count * type_size);
    if (ret != KUPL_OK) {
        return kupl_log_error_return(ERROR, "kupl_shm_send kupl_memcpy failed");
    }
    return KUPL_OK;
}

static kupl_always_inline int kupl_shm_recv(void *buf, void *source_buf, size_t offset, unsigned int count,
                                            unsigned int type_size)
{
    if (count == 0) {
        return KUPL_OK;
    }
    auto t_source_buf = static_cast<char *>(source_buf);
    auto t_buf = static_cast<char *>(buf);
    if (SIZE_MAX / count < type_size) {
        return kupl_log_error_return(ERROR, "kupl_shm_recv size too big");
    }
    int ret = kupl_memcpy(t_buf + offset * type_size, t_source_buf + offset * type_size, count * type_size);
    if (ret != KUPL_OK) {
        return kupl_log_error_return(ERROR, "kupl_shm_recv kupl_memcpy failed");
    }
    return KUPL_OK;
}

template <typename T>
static kupl_always_inline void reduce_op(void *result_buf, void *buf1, void *buf2, unsigned int offset,
                                         unsigned int copy_count)
{
    unsigned int start = offset;
    unsigned int end = offset + copy_count;
    auto *t_result_buf = static_cast<T *>(result_buf);
    auto *t_buf1 = static_cast<T *>(buf1);
    auto *t_buf2 = static_cast<T *>(buf2);
    for (unsigned int i = start; i < end; ++i) {
        t_result_buf[i] = t_buf1[i] + t_buf2[i];
    }
}

static kupl_always_inline void calc_offset(int rank, int count, unsigned int step, unsigned int *start,
                                           unsigned int *end)
{
    unsigned int rank_u = static_cast<unsigned int>(rank);
    *start = 0, *end = (unsigned int)(count - 1);
    for (unsigned int i = 0; i <= step; i++) {
        unsigned int mask = 1 << i;
        unsigned int dest = rank_u ^ mask;
        unsigned int mid = (*start + *end) / 2;
        if (rank_u < dest) {
            *end = mid;
        } else {
            *start = mid + 1;
        }
    }
}

static kupl_always_inline unsigned int calc_hibit(unsigned value)
{
    int start = 32;
    unsigned int mask;

    --start;
    mask = 1 << start;

    for (; start >= 0; --start, mask >>= 1) {
        if (value & mask) {
            break;
        }
    }
    return (unsigned int)start;
}

#ifdef TEST_ALGO
static kupl_always_inline void calc_linear_offset(int div, int rem, int i, int &offset, int &copy_count)
{
    if (i - 1 < rem) {
        offset = i * (div + 1);
    } else {
        offset = rem * (div + 1) + (i - rem) * div;
    }
    if (i < rem) {
        copy_count = div + 1;
    } else {
        copy_count = div;
    }
}
#endif

static kupl_always_inline int calc_vrank(int rank, int nprocs_rem)
{
    int vrank;
    if (rank < 2 * nprocs_rem) {
        if (rank % 2 != 0) {
            vrank = -1;
        } else {
            vrank = rank / 2;
        }
    } else {
        vrank = rank - nprocs_rem;
    }
    return vrank;
}

static kupl_always_inline void dispatch_by_datatype(const kupl_shm_datatype_t &datatype, void *result_buf, void *buf1,
                                                    void *buf2, unsigned int offset, unsigned int copy_count)
{
    switch (datatype) {
        case KUPL_SHM_DATATYPE_CHAR:
            reduce_op<char>(result_buf, buf1, buf2, offset, copy_count);
            break;
        case KUPL_SHM_DATATYPE_INT:
            reduce_op<int>(result_buf, buf1, buf2, offset, copy_count);
            break;
        case KUPL_SHM_DATATYPE_LONG:
            reduce_op<long>(result_buf, buf1, buf2, offset, copy_count);
            break;
        case KUPL_SHM_DATATYPE_FLOAT:
            reduce_op<float>(result_buf, buf1, buf2, offset, copy_count);
            break;
        case KUPL_SHM_DATATYPE_DOUBLE:
            reduce_op<double>(result_buf, buf1, buf2, offset, copy_count);
            break;
        default:
            kupl_error("dispatch_by_datatype get unknown datatype");
    }
}

#ifdef TEST_ALGO
static int do_allreduce_with_rb_linear_linear(kupl_shm_request_h request);
#endif

static kupl_always_inline int do_pre_reduce_with_partial_process(const kupl_shm_request *request)
{
    const char *sendbuf = (const char *)request->args.args_t.allreduce.sendbuf;
    char *recvbuf = (char *)request->args.args_t.allreduce.recvbuf;
    kupl_shm_win_h send_win = request->args.args_t.allreduce.wins.send_win;
    kupl_shm_win_h recv_win = request->args.args_t.allreduce.wins.recv_win;
    kupl_shm_datatype_t datatype = request->args.args_t.allreduce.datatype;
    int count = request->args.args_t.allreduce.count;
    int rank = request->comm->rank;

    int ret = KUPL_OK;
    if (rank % 2 != 0) {
        // write right half to dest
        int dest = rank - 1;
        void *dest_sendbuf;
        void *dest_recvbuf;
        ret = kupl_shm_win_query(send_win, dest, &dest_sendbuf);
        if (ret != KUPL_OK) {
            kupl_error("kupl_shm_win_query send_win failed");
            return ret;
        }
        ret = kupl_shm_win_query(recv_win, dest, &dest_recvbuf);
        if (ret != KUPL_OK) {
            kupl_error("kupl_shm_win_query recv_win failed");
            return ret;
        }
        dest_sendbuf = (char *)dest_sendbuf + request->s_offset;
        dest_recvbuf = (char *)dest_recvbuf + request->r_offset;
        unsigned int curr_start = 0;
        unsigned int curr_end = 0;
        calc_offset(rank, count, 0, &curr_start, &curr_end);
        kupl_shm_peer_fence(send_win, dest);
        dispatch_by_datatype(datatype, dest_recvbuf, const_cast<char *>(sendbuf), dest_sendbuf, curr_start,
                             curr_end - curr_start + 1);
    } else {
        // recv left half from dest
        int dest = rank + 1;
        void *destbuf;
        ret = kupl_shm_win_query(send_win, dest, &destbuf);
        if (ret != KUPL_OK) {
            kupl_error("kupl_shm_win_query send_win failed");
            return ret;
        }
        destbuf = (char *)destbuf + request->s_offset;
        unsigned int curr_start = 0;
        unsigned int curr_end = 0;
        calc_offset(rank, count, 0, &curr_start, &curr_end);
        kupl_shm_peer_fence(send_win, dest);
        dispatch_by_datatype(datatype, recvbuf, const_cast<char *>(sendbuf), destbuf, curr_start,
                             curr_end - curr_start + 1);
    }
    return ret;
}

static kupl_always_inline int do_reduce_scatter_with_rh_algo(const kupl_shm_request *request, int nprocs_pof2,
                                                             unsigned int step, int vrank, int nprocs_rem)
{
    const char *sendbuf = (const char *)request->args.args_t.allreduce.sendbuf;
    char *recvbuf = (char *)request->args.args_t.allreduce.recvbuf;
    kupl_shm_win_h send_win = request->args.args_t.allreduce.wins.send_win;
    kupl_shm_win_h recv_win = request->args.args_t.allreduce.wins.recv_win;
    kupl_shm_datatype_t datatype = request->args.args_t.allreduce.datatype;
    int count = request->args.args_t.allreduce.count;
    int rank = request->comm->rank;
    int ret = KUPL_OK;
    unsigned int nprocs_pof2_u = static_cast<unsigned int>(nprocs_pof2);
    unsigned int vrank_u = static_cast<unsigned int>(vrank);
    for (unsigned int mask = 1; mask < nprocs_pof2_u; mask <<= 1) {
        unsigned int vdest_u = vrank_u ^ mask;
        int vdest = static_cast<int>(vdest_u);
        int dest = vdest < nprocs_rem ? vdest * 2 : vdest + nprocs_rem;

        // Get dest buf
        void *destbuf;
        kupl_shm_win_h dest_win = recv_win;
        int rb_process_num = 2 * nprocs_rem;
        if (mask == 1 && dest >= rb_process_num) {
            dest_win = send_win;
        }

        ret = kupl_shm_win_query(dest_win, dest, &destbuf);
        if (ret != KUPL_OK) {
            kupl_error("kupl_shm_win_query dest_win failed");
            return ret;
        }
        if (dest_win == recv_win) {
            destbuf = (char *)destbuf + request->r_offset;
        } else {
            destbuf = (char *)destbuf + request->s_offset;
        }

        // Get local buf
        char *local_buf = recvbuf;
        if (mask == 1 && rank >= rb_process_num) {
            local_buf = const_cast<char *>(sendbuf);
        }

        // Local reduce
        // peer_fence need know odd/even to determine first send or recv
        kupl_shm_peer_fence(recv_win, dest);
        unsigned int curr_start = 0;
        unsigned int curr_end = 0;
        calc_offset(vrank, count, step - 1, &curr_start, &curr_end);
        dispatch_by_datatype(datatype, recvbuf, local_buf, destbuf, curr_start, curr_end - curr_start + 1);

        step++;
    }
    return ret;
}

static kupl_always_inline int do_allgather_with_rd_algo(const kupl_shm_request *request, int nprocs_pof2,
                                                        unsigned int step, int vrank, int nprocs_rem)
{
    char *recvbuf = (char *)request->args.args_t.allreduce.recvbuf;
    kupl_shm_win_h recv_win = request->args.args_t.allreduce.wins.recv_win;
    int type_size = request->args.args_t.allreduce.type_size;
    int count = request->args.args_t.allreduce.count;
    int ret = KUPL_OK;
    unsigned int nprocs_pof2_u = static_cast<unsigned int>(nprocs_pof2);
    unsigned int vrank_u = static_cast<unsigned int>(vrank);
    for (unsigned int mask = nprocs_pof2_u >> 1; mask > 0; mask >>= 1) {
        unsigned int vdest_u = vrank_u ^ mask;
        int vdest = static_cast<int>(vdest_u);
        int dest = vdest < nprocs_rem ? vdest * 2 : vdest + nprocs_rem;
        // read dest need fence before memcpy
        kupl_shm_peer_fence(recv_win, dest);
        unsigned int curr_start = 0;
        unsigned int curr_end = 0;
        calc_offset(vdest, count, step - 1, &curr_start, &curr_end);
        void *source_buf;
        ret = kupl_shm_win_query(recv_win, dest, &source_buf);
        if (ret != KUPL_OK) {
            kupl_error("kupl_shm_win_query dest_win failed");
            return ret;
        }

        source_buf = (char *)source_buf + request->r_offset;
        ret = kupl_shm_recv(recvbuf, source_buf, curr_start, curr_end - curr_start + 1, (unsigned)type_size);
        if (ret != KUPL_OK) {
            kupl_error("kupl_shm_recv failed");
            return ret;
        }
        step--;
    }
    return ret;
}

static kupl_always_inline int do_post_allgather_with_partial_process(const kupl_shm_request *request)
{
    char *recvbuf = (char *)request->args.args_t.allreduce.recvbuf;
    kupl_shm_win_h recv_win = request->args.args_t.allreduce.wins.recv_win;
    int type_size = request->args.args_t.allreduce.type_size;
    int count = request->args.args_t.allreduce.count;
    int rank = request->comm->rank;

    int ret = KUPL_OK;
    if (rank % 2 == 0) {
        // write sum to process which not join phase1 and phase2
        int dest = rank + 1;
        void *destbuf;
        ret = kupl_shm_win_query(recv_win, dest, &destbuf);
        if (ret != KUPL_OK) {
            kupl_error("kupl_shm_win_query recv_win failed");
            return ret;
        }
        destbuf = (char *)destbuf + request->r_offset;
        ret = kupl_shm_send(recvbuf, destbuf, 0, (unsigned)count, (unsigned)type_size);
        if (ret != KUPL_OK) {
            kupl_error("kupl_shm_send recvbuf failed");
            return ret;
        }
    }
    return ret;
}

static int do_allreduce_with_rb_rh_rd(kupl_shm_request_h request)
{
    const char *sendbuf = (const char *)request->args.args_t.allreduce.sendbuf;
    char *recvbuf = (char *)request->args.args_t.allreduce.recvbuf;
    int count = request->args.args_t.allreduce.count;
    int type_size = request->args.args_t.allreduce.type_size;
    kupl_shm_win_h send_win = request->args.args_t.allreduce.wins.send_win;
    int rank = request->comm->rank;
    int comm_size = request->comm->size;
    if (comm_size == 1) {
        return kupl_memcpy(recvbuf, sendbuf, (size_t)(count * type_size));
    }

    unsigned int nsteps = calc_hibit((unsigned int)comm_size);
    int nprocs_pof2 = 1 << nsteps;
    int ret = KUPL_OK;

    unsigned int step;
    /* *
     * Step 0. Preprocessing cases that are not powers of 2
     */
    int nprocs_rem = comm_size - nprocs_pof2;
    int vrank = calc_vrank(rank, nprocs_rem);
    int rb_process_num = 2 * nprocs_rem;
    if (rank < rb_process_num) {
        do_pre_reduce_with_partial_process(request);
    }
    ret = kupl_shm_fence(send_win);
    if (ret != KUPL_OK) {
        kupl_error("kupl_shm_fence error");
        return ret;
    }

    if (vrank != -1) {
        /* *
         * Step 1. Reduce-scatter implemented with recursive vector halving and recursive distance doubling.
         */
        step = 1;
        do_reduce_scatter_with_rh_algo(request, nprocs_pof2, step, vrank, nprocs_rem);

        /* *
         * Step 2. Allgather by the recursive doubling algorithm.
         */
        step = nsteps;
        do_allgather_with_rd_algo(request, nprocs_pof2, step, vrank, nprocs_rem);
    }

    /* *
     * Step 3. Synchronize partial sums to processes not participating in the calculation
     */
    if (rank < rb_process_num) {
        do_post_allgather_with_partial_process(request);
    }

    ret = kupl_shm_fence(send_win);
    if (ret != KUPL_OK) {
        kupl_error("kupl_shm_fence error");
        return ret;
    }

    return ret;
}

#ifdef TEST_ALGO
static int do_allreduce_with_rb_rh_rd_write(kupl_shm_request_h request)
{
    const char *sendbuf = (const char *)request->args.args_t.allreduce.sendbuf;
    char *recvbuf = (char *)request->args.args_t.allreduce.recvbuf;
    kupl_shm_comm_h comm = request->comm;
    kupl_shm_win_h send_win = request->args.args_t.allreduce.wins.send_win;
    kupl_shm_win_h recv_win = request->args.args_t.allreduce.wins.recv_win;
    int type_size = request->args.args_t.allreduce.type_size;
    kupl_shm_datatype_t datatype = request->args.args_t.allreduce.datatype;
    int count = request->args.args_t.allreduce.count;

    int rank = comm->rank;
    int comm_size = comm->size;
    unsigned int nsteps = calc_hibit((unsigned int)comm_size);

    int nprocs_pof2 = 1 << nsteps;
    int ret = KUPL_OK;

    unsigned int step;
    if (nprocs_pof2 != comm_size) {
        kupl_warn("not support process num, use default algo");
        return do_allreduce_with_rb_linear_linear(request);
    }

    /* *
     * Step 1. Reduce-scatter implemented with recursive vector halving and recursive distance doubling.
     */
    step = 1;
    for (unsigned int mask = 1; mask < nprocs_pof2; mask <<= 1) {
        int dest = rank ^ mask;

        // Get dest buf
        void *destbuf;
        kupl_shm_win_h dest_win = recv_win;
        if (mask == 1) {
            dest_win = send_win;
        }

        ret = kupl_shm_win_query(dest_win, dest, &destbuf);
        if (ret != KUPL_OK) {
            return kupl_log_error_return(ERROR, "kupl_shm_win_query failed");
        }
        if (dest_win == recv_win) {
            destbuf = (char *)destbuf + request->r_offset;
        } else {
            destbuf = (char *)destbuf + request->s_offset;
        }

        // Get local buf
        char *local_buf = recvbuf;
        if (mask == 1) {
            local_buf = const_cast<char *>(sendbuf);
        }

        // Local reduce
        // peer_fence need know odd/even to determine first send or recv
        kupl_shm_peer_fence(recv_win, dest);
        unsigned int curr_start = 0;
        unsigned int curr_end = 0;
        calc_offset(rank, count, step - 1, &curr_start, &curr_end);
        dispatch_by_datatype(datatype, recvbuf, local_buf, destbuf, curr_start, curr_end - curr_start + 1);

        step++;
    }

    /* *
     * Step 2. Allgather by the recursive doubling algorithm.
     */
    step = nsteps;
    unsigned int nprocs_pof2_u = static_cast<unsigned int>(nprocs_pof2);
    unsigned int vrank_u = static_cast<unsigned int>(vrank);
    for (unsigned int mask = nprocs_pof2_u >> 1; mask > 0; mask >>= 1) {
        unsigned int dest_u = vrank_u ^ mask;
        int dest = static_cast<int>(dest_u);
        // write dest need fence after memcpy
        unsigned int curr_start = 0;
        unsigned int curr_end = 0;
        calc_offset(rank, count, step - 1, &curr_start, &curr_end);
        void *destbuf;
        ret = kupl_shm_win_query(recv_win, dest, &destbuf);
        if (ret != KUPL_OK) {
            return kupl_log_error_return(ERROR, "kupl_shm_win_query failed");
        }
        destbuf = (char *)destbuf + request->r_offset;
        ret = kupl_shm_send(recvbuf, destbuf, curr_start, curr_end - curr_start + 1, type_size);
        kupl_shm_peer_fence(recv_win, dest);

        if (ret != KUPL_OK) {
            return KUPL_ERROR;
        }
        step--;
    }
    ret = kupl_shm_fence(send_win);
    if (ret != KUPL_OK) {
        return kupl_log_error_return(ERROR, "kupl_shm_fence error");
    }

    return KUPL_OK;
}

static int do_allreduce_with_rb_rh_linear(kupl_shm_request_h request)
{
    const char *sendbuf = (const char *)request->args.args_t.allreduce.sendbuf;
    char *recvbuf = (char *)request->args.args_t.allreduce.recvbuf;
    kupl_shm_comm_h comm = request->comm;
    kupl_shm_win_h send_win = request->args.args_t.allreduce.wins.send_win;
    kupl_shm_win_h recv_win = request->args.args_t.allreduce.wins.recv_win;
    int type_size = request->args.args_t.allreduce.type_size;
    kupl_shm_datatype_t datatype = request->args.args_t.allreduce.datatype;
    int count = request->args.args_t.allreduce.count;

    int rank = comm->rank;
    int comm_size = comm->size;

    unsigned int nsteps = calc_hibit((unsigned int)comm_size);
    int nprocs_pof2 = 1 << nsteps;
    int ret = KUPL_OK;

    unsigned int step;
    if (nprocs_pof2 != comm_size) {
        kupl_warn("not support process num, use default algo");
        return do_allreduce_with_rb_linear_linear(request);
    }

    /* *
     * Step 1. Reduce-scatter implemented with recursive vector halving and recursive distance doubling.
     */
    step = 1;
    for (unsigned int mask = 1; mask < nprocs_pof2; mask <<= 1) {
        int dest = rank ^ mask;

        // Get dest buf
        void *destbuf;
        kupl_shm_win_h dest_win = recv_win;
        if (mask == 1) {
            dest_win = send_win;
        }

        ret = kupl_shm_win_query(dest_win, dest, &destbuf);
        if (ret != KUPL_OK) {
            return kupl_log_error_return(ERROR, "kupl_shm_win_query failed");
        }
        if (dest_win == recv_win) {
            destbuf = (char *)destbuf + request->r_offset;
        } else {
            destbuf = (char *)destbuf + request->s_offset;
        }

        // Get local buf
        char *local_buf = recvbuf;
        if (mask == 1) {
            local_buf = const_cast<char *>(sendbuf);
        }

        // Local reduce
        // peer_fence need know odd/even to determine first send or recv
        kupl_shm_peer_fence(recv_win, dest);
        unsigned int curr_start = 0;
        unsigned int curr_end = 0;
        calc_offset(rank, count, step - 1, &curr_start, &curr_end);
        dispatch_by_datatype(datatype, recvbuf, local_buf, destbuf, curr_start, curr_end - curr_start + 1);

        step++;
    }

    /* *
     * Step 2. Allgather by the linear algorithm.
     */
    ret = kupl_shm_fence(recv_win);
    if (ret != KUPL_OK) {
        return kupl_log_error_return(ERROR, "kupl_shm_fence error");
    }
    step = nsteps;
    for (int i = 0; i < nprocs_pof2; i++) {
        if (rank == i) {
            continue;
        }
        int dest = i;
        void *destbuf;
        ret = kupl_shm_win_query(recv_win, dest, &destbuf);
        if (ret != KUPL_OK) {
            return kupl_log_error_return(ERROR, "kupl_shm_win_query failed");
        }
        destbuf = (char *)destbuf + request->r_offset;
        auto *t_destbuf = static_cast<char *>(destbuf);
        unsigned int curr_start = 0;
        unsigned int curr_end = 0;
        calc_offset(dest, count, step - 1, &curr_start, &curr_end);
        kupl_memcpy(recvbuf + curr_start * type_size, t_destbuf + curr_start * type_size,
                    (curr_end - curr_start + 1) * type_size);
    }
    ret = kupl_shm_fence(send_win);
    if (ret != KUPL_OK) {
        return kupl_log_error_return(ERROR, "kupl_shm_fence error");
    }

    return ret;
}

static int do_allreduce_with_rb_linear_linear(kupl_shm_request_h request)
{
    const char *sendbuf = (const char *)request->args.args_t.allreduce.sendbuf;
    char *recvbuf = (char *)request->args.args_t.allreduce.recvbuf;
    kupl_shm_comm_h comm = request->comm;
    kupl_shm_win_h send_win = request->args.args_t.allreduce.wins.send_win;
    kupl_shm_win_h recv_win = request->args.args_t.allreduce.wins.recv_win;
    int type_size = request->args.args_t.allreduce.type_size;
    kupl_shm_datatype_t datatype = request->args.args_t.allreduce.datatype;

    int rank = comm->rank;
    int comm_size = comm->size;
    int count = request->args.args_t.allreduce.count;
    int block_size;
    int div = count / comm_size;
    int rem = count % comm_size;
    if (rank < rem) {
        block_size = div + 1;
    } else {
        block_size = div;
    }

    int ret = KUPL_OK;
    int offset;
    int copy_count;

    /* *
     * Step 1. Reduce-scatter by the linear algorithm.
     */
    copy_count = block_size;
    if (rank - 1 < rem) {
        offset = rank * (div + 1);
    } else {
        offset = rem * (div + 1) + (rank - rem) * div;
    }
    kupl_memcpy(recvbuf + offset * type_size, sendbuf + offset * type_size, copy_count * type_size);
    ret = kupl_shm_fence(send_win);
    if (ret != KUPL_OK) {
        return kupl_log_error_return(ERROR, "kupl_shm_fence error");
    }

    for (int i = 0; i < comm_size; i++) {
        if (rank == i) {
            continue;
        }

        void *destbuf;
        ret = kupl_shm_win_query(send_win, i, &destbuf);
        if (ret != KUPL_OK) {
            return kupl_log_error_return(ERROR, "kupl_shm_win_query failed");
        }
        destbuf = (char *)destbuf + request->s_offset;
        dispatch_by_datatype(datatype, recvbuf, recvbuf, destbuf, offset, (unsigned)copy_count);
    }

    /* *
     * Step 2. Allgather by the linear algorithm.
     */
    ret = kupl_shm_fence(recv_win);
    if (ret != KUPL_OK) {
        return kupl_log_error_return(ERROR, "kupl_shm_fence error");
    }
    for (int i = 0; i < comm_size; i++) {
        if (rank == i) {
            continue;
        }
        void *destbuf;
        ret = kupl_shm_win_query(recv_win, i, &destbuf);
        if (ret != KUPL_OK) {
            return kupl_log_error_return(ERROR, "kupl_shm_win_query failed");
        }
        destbuf = (char *)destbuf + request->r_offset;
        auto *t_destbuf = static_cast<char *>(destbuf);
        calc_linear_offset(div, rem, i, offset, copy_count);
        kupl_memcpy(recvbuf + offset * type_size, t_destbuf + offset * type_size, copy_count * type_size);
    }
    ret = kupl_shm_fence(send_win);
    if (ret != KUPL_OK) {
        return kupl_log_error_return(ERROR, "kupl_shm_fence error");
    }

    return ret;
}
#endif

static int do_allreduce_with_linear(kupl_shm_request_h request)
{
    const char *sendbuf = (const char *)request->args.args_t.allreduce.sendbuf;
    char *recvbuf = (char *)request->args.args_t.allreduce.recvbuf;
    kupl_shm_comm_h comm = request->comm;
    kupl_shm_win_h send_win = request->args.args_t.allreduce.wins.send_win;
    int type_size = request->args.args_t.allreduce.type_size;
    kupl_shm_datatype_t datatype = request->args.args_t.allreduce.datatype;

    int rank = comm->rank;
    int comm_size = comm->size;
    int count = request->args.args_t.allreduce.count;
    int ret = KUPL_OK;

    ret = kupl_memcpy(recvbuf, sendbuf, (size_t)(count * type_size));
    if (ret != KUPL_OK) {
        return kupl_log_error_return(ERROR, "kupl_memcpy error");
    }
    ret = kupl_shm_fence(send_win);
    if (ret != KUPL_OK) {
        return kupl_log_error_return(ERROR, "kupl_shm_fence error");
    }

    for (int i = 0; i < comm_size; i++) {
        if (rank == i) {
            continue;
        }
        void *destbuf;
        ret = kupl_shm_win_query(send_win, i, &destbuf);
        if (ret != KUPL_OK) {
            return kupl_log_error_return(ERROR, "kupl_shm_win_query error");
        }
        destbuf = (char *)destbuf + request->s_offset;
        dispatch_by_datatype(datatype, recvbuf, recvbuf, destbuf, 0, (unsigned)count);
    }
    ret = kupl_shm_fence(send_win);
    if (ret != KUPL_OK) {
        return kupl_log_error_return(ERROR, "kupl_shm_fence error");
    }

    return KUPL_OK;
}

int kupl_do_allreduce(kupl_shm_request_h request)
{
    kupl_shm_allreduce_algo_t algo = request->args.algo_t.allreduce;
    switch (algo) {
        case KUPL_SHM_ALLREDUCE_ALGO_AUTO:
            return do_allreduce_with_rb_rh_rd(request);
        case KUPL_SHM_ALLREDUCE_ALGO_LINEAR:
            return do_allreduce_with_linear(request);
        case KUPL_SHM_ALLREDUCE_ALGO_RB_RH_RD:
            return do_allreduce_with_rb_rh_rd(request);
        default:
            g_is_abnormal_exit = true;
            kupl_fatal("do allreduce attempt to select algorithm %d when only 0-%d is valid?", algo,
                       KUPL_SHM_ALLREDUCE_ALGO_MAX - 1);
    }
    return KUPL_ERROR;
}
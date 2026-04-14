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
#include <unistd.h>
#include "gtest/gtest.h"
#include "mpi.h"
#include "kupl.h"

static int oob_allgather_callback(const void *sendbuf, void *recvbuf, int size, void *group,
    kupl_shm_datatype_t datatype)
{
    if (sendbuf == nullptr) {
        sendbuf = (void*)(1);
    }
    switch (datatype) {
        case KUPL_SHM_DATATYPE_CHAR:
            return MPI_Allgather(sendbuf, size, MPI_CHAR, recvbuf, size, MPI_CHAR, (MPI_Comm)group);
        default:
            printf("not support datatype");
            return KUPL_ERROR;
    }
}

static int oob_barrier_callback(void *group)
{
    return MPI_Barrier((MPI_Comm)group);
}

TEST(test_comm, kupl_comm_test_request_null)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(-1, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), -1);
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), nullptr, (void *)comm, &kupl_comm), -1);
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    void *baseptr;
    kupl_shm_win_h win;
    ASSERT_EQ(kupl_shm_win_alloc(-1, kupl_comm, &baseptr, &win), -1);
    ASSERT_EQ(kupl_shm_win_alloc(1, nullptr, &baseptr, &win), -1);
    kupl_shm_request_h request;
    ASSERT_EQ(kupl_shm_allreduce_init(nullptr, nullptr, 1, KUPL_SHM_DATATYPE_CHAR, KUPL_SHM_REDUCE_OP_SUM, kupl_comm,
                                      &request), -1);
    ASSERT_EQ(kupl_shm_allreduce_init(nullptr, nullptr, -1, KUPL_SHM_DATATYPE_CHAR, KUPL_SHM_REDUCE_OP_SUM, kupl_comm,
                                      &request), -1);
    int dummpy = 1;
    ASSERT_EQ(kupl_shm_allreduce_init(&dummpy, &dummpy, 1, KUPL_SHM_DATATYPE_CHAR, KUPL_SHM_REDUCE_OP_MIN, kupl_comm,
                                      &request), -1);
    ASSERT_EQ(kupl_shm_alltoall_init(nullptr, 1, KUPL_SHM_DATATYPE_CHAR, nullptr, 1, KUPL_SHM_DATATYPE_CHAR, kupl_comm,
                                     &request), -1);
    ASSERT_EQ(kupl_shm_alltoall_init(nullptr, -1, KUPL_SHM_DATATYPE_CHAR, nullptr, -1, KUPL_SHM_DATATYPE_CHAR,
                                     kupl_comm, &request), -1);
    ASSERT_EQ(kupl_shm_bcast_init(nullptr, -1, KUPL_SHM_DATATYPE_CHAR, 0, kupl_comm, &request), -1);
    ASSERT_EQ(kupl_shm_bcast_init(nullptr, -1, KUPL_SHM_DATATYPE_CHAR, 0, kupl_comm, &request), -1);
    ASSERT_EQ(kupl_shm_bcast_init(&dummpy, -1, KUPL_SHM_DATATYPE_CHAR, -1, kupl_comm, &request), -1);
    ASSERT_EQ(kupl_shm_request_start(nullptr), -1);
    ASSERT_EQ(kupl_shm_request_wait(nullptr), -1);

    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm, kupl_comm_test_win_alloc_base)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 0), 0);
    kupl_shm_win_h win;
    void *baseptr;
    ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm, &baseptr, &win), 0);
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_win_alloc(0, kupl_comm, &baseptr, &win), 0);
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_win_free(nullptr), -1);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(nullptr), -1);
}

TEST(test_comm, kupl_comm_test_info_config_invalid)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 2), -1);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm, kupl_comm_test_win_alloc_single_multiple_win)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 0), 0);
    kupl_shm_win_h win1, win2;
    void *baseptr1, *baseptr2;
    ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm, &baseptr1, &win1), 0);
    ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm, &baseptr2, &win2), 0);
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_win_free(win1), 0);
    ASSERT_EQ(kupl_shm_win_free(win2), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm, kupl_comm_test_win_alloc_multiple_comm_non_overlap)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;
    int color;
    if (myid % 2 == 0) {
        color = 0;
    } else {
        color = 1;
    }
    MPI_Comm subcomm;
    MPI_Comm_split(comm, color, 0, &subcomm);
    int sub_id, sub_size;
    MPI_Comm_size(subcomm, &sub_size);
    MPI_Comm_rank(subcomm, &sub_id);
    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(sub_size, sub_id, getpid(), oob_cbs_h, (void *)subcomm, &kupl_comm), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 0), 0);
    kupl_shm_win_h win;
    void *baseptr;
    if (myid % 2 == 0) {
        ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm, &baseptr, &win), 0);
    } else {
        ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm, &baseptr, &win), 0);
    }
    MPI_Barrier(subcomm);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm, kupl_comm_test_win_alloc_multiple_comm_overlap)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;
    int color;
    if (myid % 2 == 0) {
        color = 0;
    } else {
        color = 1;
    }
    MPI_Comm subcomm;
    MPI_Comm_split(comm, color, 0, &subcomm);
    int sub_id, sub_size;
    MPI_Comm_size(subcomm, &sub_size);
    MPI_Comm_rank(subcomm, &sub_id);
    kupl_shm_comm_h kupl_comm1, kupl_comm2;
    if (myid % 2 == 0) {
        ASSERT_EQ(kupl_shm_comm_create(sub_size, sub_id, getpid(), oob_cbs_h, (void *)subcomm, &kupl_comm1), 0);
    }
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm2), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 0), 0);
    kupl_shm_win_h win1, win2;
    void *baseptr1, *baseptr2;
    if (myid % 2 == 0) {
        ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm1, &baseptr1, &win1), 0);
    }
    ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm2, &baseptr2, &win2), 0);

    MPI_Barrier(comm);
    if (myid % 2 == 0) {
        ASSERT_EQ(kupl_shm_win_free(win1), 0);
        ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm1), 0);
    }
    ASSERT_EQ(kupl_shm_win_free(win2), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm2), 0);
}

TEST(test_comm, kupl_comm_test_win_query)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 0), 0);
    kupl_shm_win_h win;
    void *baseptr;
    ASSERT_EQ(kupl_shm_win_alloc(256, kupl_comm, &baseptr, &win), 0);
    MPI_Barrier(comm);
    void *attach_address;
    kupl_shm_win_query(win, 0, &attach_address);
    ((int *)attach_address)[myid] = myid + 1;
    MPI_Barrier(comm);
    if (myid == 0) {
        for (int i = 0; i < numprocs; i++) {
            ASSERT_EQ(((int *)attach_address)[i], i + 1);
        }
    }
    int ret = kupl_shm_win_query(win, 0, nullptr);
    ASSERT_EQ(ret, KUPL_ERROR);
    ret = kupl_shm_win_query(win, -1, &attach_address);
    ASSERT_EQ(ret, KUPL_ERROR);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm, kupl_comm_test_peer_fence)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 0), 0);
    kupl_shm_win_h win;
    void *baseptr;
    ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm, &baseptr, &win), 0);
    MPI_Barrier(comm);
    int win_rank;
    kupl_shm_comm_rank(nullptr, &win_rank);
    kupl_shm_comm_rank(kupl_comm, &win_rank);
    // barrier test
    ASSERT_EQ(kupl_shm_peer_fence(nullptr, 0), KUPL_ERROR);
    if (numprocs == 1) {
        ASSERT_EQ(kupl_shm_win_free(win), 0);
        ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
        return;
    }
    for (int i = 0; i < numprocs; i++) {
        for (int j = 1; j < numprocs; j++) {
            if (win_rank == 0) {
                kupl_shm_peer_fence(win, j);
            }
        }
        for (int j = 1; j < numprocs; j++) {
            if (win_rank == j) {
                kupl_shm_peer_fence(win, 0);
            }
        }
    }
    ASSERT_EQ(kupl_shm_peer_fence(win, -1), KUPL_ERROR);
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm, kupl_comm_test_fence)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 0), 0);
    kupl_shm_win_h win;
    void *baseptr, *otherptr;
    ASSERT_EQ(kupl_shm_fence(nullptr), KUPL_ERROR);
    ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm, &baseptr, &win), 0);

    ((int*)baseptr)[0] = 0;
    MPI_Barrier(comm);

    // barrier test
    int win_size;
    kupl_shm_comm_size(nullptr, &win_size);
    kupl_shm_comm_size(kupl_comm, &win_size);
    MPI_Barrier(comm);
    for (int i = 0; i < 5; i++) {
        ((int*)baseptr)[0]++;
        kupl_shm_fence(win);
        if (myid == 0) {
            for (int j = 0; j < win_size; j++) {
                kupl_shm_win_query(win, j, &otherptr);
                ASSERT_EQ(((int*)otherptr)[0], ((int*)baseptr)[0]);
            }
        }
        kupl_shm_fence(win);
        int cnt = 0;
        if (myid == 0) {
            while (cnt < 1000000) {
                cnt++;
            }
        }
        if (myid == 1) {
            while (cnt < 10000) {
                cnt++;
            }
        }
        if (myid == 2) {
            while (cnt < 100) {
                cnt++;
            }
        }
    }

    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm, kupl_comm_test_bcast_char)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    kupl_shm_win_h win;

    kupl_shm_request_h request;
    void *win_alloc_buffer;

    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 0), 0);
    size_t buf_size = 1000000;
    ASSERT_EQ(kupl_shm_win_alloc(buf_size, kupl_comm, &win_alloc_buffer, &win), 0);
    int count;

    count = 100000;
    if (myid == 0) {
        for (int i = 0; i < count; i++) {
            ((char *)win_alloc_buffer)[i] = (char)i;
        }
    }
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_bcast_init(win_alloc_buffer, count, KUPL_SHM_DATATYPE_CHAR, 0, kupl_comm, &request), 0);
    ASSERT_EQ(kupl_shm_request_start(request), 0);
    ASSERT_EQ(kupl_shm_request_wait(request), 0);
    for (int i = 0; i < count; i++) {
        ASSERT_EQ(((char *)win_alloc_buffer)[i], (char)i);
    }

    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_request_free(request), 0);
    ASSERT_EQ(kupl_shm_request_free(nullptr), -1);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm, kupl_comm_test_bcast_int)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    kupl_shm_win_h win;

    kupl_shm_request_h request;
    void *win_alloc_buffer;

    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 0), 0);
    size_t buf_size = 1000000;
    ASSERT_EQ(kupl_shm_win_alloc(buf_size, kupl_comm, &win_alloc_buffer, &win), 0);
    int count;

    count = 100000;
    if (myid == 0) {
        for (int i = 0; i < count; i++) {
            ((int *)win_alloc_buffer)[i] = (int)i;
        }
    }
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_bcast_init(win_alloc_buffer, count, KUPL_SHM_DATATYPE_INT, 0, kupl_comm, &request), 0);
    ASSERT_EQ(kupl_shm_request_start(request), 0);
    ASSERT_EQ(kupl_shm_request_wait(request), 0);
    for (int i = 0; i < count; i++) {
        ASSERT_EQ(((int *)win_alloc_buffer)[i], (int)i);
    }

    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_request_free(request), 0);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm, kupl_comm_test_bcast_long)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    kupl_shm_win_h win;

    kupl_shm_request_h request;
    void *win_alloc_buffer;

    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 0), 0);
    size_t buf_size = 1000000;
    ASSERT_EQ(kupl_shm_win_alloc(buf_size, kupl_comm, &win_alloc_buffer, &win), 0);
    int count;

    count = 100000;
    if (myid == 0) {
        for (int i = 0; i < count; i++) {
            ((long *)win_alloc_buffer)[i] = (long)i;
        }
    }
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_bcast_init(win_alloc_buffer, count, KUPL_SHM_DATATYPE_LONG, 0, kupl_comm, &request), 0);
    ASSERT_EQ(kupl_shm_request_start(request), 0);
    ASSERT_EQ(kupl_shm_request_wait(request), 0);
    for (int i = 0; i < count; i++) {
        ASSERT_EQ(((long *)win_alloc_buffer)[i], (long)i);
    }

    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_request_free(request), 0);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm, kupl_comm_test_bcast_root)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 0), 0);

    kupl_shm_comm_h kupl_comm;
    kupl_shm_win_h win;
    kupl_shm_request_h request;
    void *win_alloc_buffer;

    size_t buf_size = 100000;
    int count = 10000;

    for (int root = 0; root < numprocs - 1; root++) {
        ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
        ASSERT_EQ(kupl_shm_win_alloc(buf_size, kupl_comm, &win_alloc_buffer, &win), 0);
        if (myid == root) {
            for (int i = 0; i < count; i++) {
                ((char *)win_alloc_buffer)[i] = (char)root;
            }
        }
        MPI_Barrier(comm);
        ASSERT_EQ(
            kupl_shm_bcast_init(win_alloc_buffer, count, KUPL_SHM_DATATYPE_CHAR, root, kupl_comm, &request), 0
        );
        ASSERT_EQ(kupl_shm_request_start(request), 0);
        ASSERT_EQ(kupl_shm_request_wait(request), 0);
        for (int i = 0; i < count; i++) {
            ASSERT_EQ(((char *)win_alloc_buffer)[i], (char)root);
        }
        ASSERT_EQ(kupl_shm_request_free(request), 0);
        ASSERT_EQ(kupl_shm_win_free(win), 0);
        ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
    }
}

template<typename T>
static inline
void do_allreduce_test(kupl_shm_datatype datatype, MPI_Datatype mpi_datatype)
{
    MPI_Comm comm = MPI_COMM_WORLD;
    int numprocs;
    MPI_Comm_size(comm, &numprocs);
    int myid;
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    kupl_shm_win_h win_send;
    kupl_shm_win_h win_recv;
    kupl_shm_win_h ompi_win_recv;
    void *sendbuf;
    void *recvbuf;
    void *ompibuf;
    int count = 16000;
    size_t buf_size = sizeof(T) * count;
    kupl_shm_win_alloc(buf_size, kupl_comm, &sendbuf, &win_send);
    kupl_shm_win_alloc(buf_size, kupl_comm, &recvbuf, &win_recv);
    kupl_shm_win_alloc(buf_size, kupl_comm, &ompibuf, &ompi_win_recv);

    // set sendbuf
    auto t_sendbuf = static_cast<T *>(sendbuf);
    auto t_recvbuf = static_cast<T *>(recvbuf);
    auto t_ompibuf = static_cast<T *>(ompibuf);
    for (int i = 0; i < count; i++) {
        t_sendbuf[i] = myid + i;
    }

    kupl_shm_request_h request;
    MPI_Barrier(comm);
    kupl_shm_allreduce_init(sendbuf, recvbuf, count, datatype, KUPL_SHM_REDUCE_OP_SUM, kupl_comm,
                            &request);
    kupl_shm_request_start(request);
    MPI_Barrier(comm);
    MPI_Allreduce(sendbuf, ompibuf, count, mpi_datatype, MPI_SUM, comm);

    switch (datatype) {
        case KUPL_SHM_DATATYPE_CHAR:
        case KUPL_SHM_DATATYPE_INT:
        case KUPL_SHM_DATATYPE_LONG:
            for (int i = 0; i < count; i++) {
                ASSERT_EQ(t_recvbuf[i], t_ompibuf[i]);
            }
            break;
        case KUPL_SHM_DATATYPE_FLOAT:
            for (int i = 0; i < count; i++) {
                ASSERT_FLOAT_EQ(t_recvbuf[i], t_ompibuf[i]);
            }
            break;
        case KUPL_SHM_DATATYPE_DOUBLE:
            for (int i = 0; i < count; i++) {
                ASSERT_DOUBLE_EQ(t_recvbuf[i], t_ompibuf[i]);
            }
            break;
    }
    ASSERT_EQ(kupl_shm_request_free(request), 0);
    ASSERT_EQ(kupl_shm_win_free(win_send), 0);
    ASSERT_EQ(kupl_shm_win_free(win_recv), 0);
    ASSERT_EQ(kupl_shm_win_free(ompi_win_recv), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm, kupl_comm_test_allreduce_float)
{
    do_allreduce_test<float>(KUPL_SHM_DATATYPE_FLOAT, MPI_FLOAT);
}

TEST(test_comm, kupl_comm_test_allreduce_double)
{
    do_allreduce_test<double>(KUPL_SHM_DATATYPE_DOUBLE, MPI_DOUBLE);
}

TEST(test_comm, kupl_comm_test_allreduce_char)
{
    do_allreduce_test<char>(KUPL_SHM_DATATYPE_CHAR, MPI_CHAR);
}

TEST(test_comm, kupl_comm_test_allreduce_int)
{
    do_allreduce_test<int>(KUPL_SHM_DATATYPE_INT, MPI_INT);
}

TEST(test_comm, kupl_comm_test_allreduce_long)
{
    do_allreduce_test<long>(KUPL_SHM_DATATYPE_LONG, MPI_LONG);
}

TEST(test_comm, kupl_comm_test_in_place_allreduce)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    kupl_shm_win_h win_send;
    kupl_shm_win_h win_recv;
    void *sendbuf;
    void *recvbuf;
    int iter = 2;
    int unit_count = 8000;
    int count = unit_count * iter;
    size_t buf_size = sizeof(double) * count;
    size_t dtmp_buf_size = sizeof(double) * unit_count;
    kupl_shm_win_alloc(buf_size, kupl_comm, &sendbuf, &win_send);
    kupl_shm_win_alloc(dtmp_buf_size, kupl_comm, &recvbuf, &win_recv);

    // set sendbuf
    for (int i = 0; i < count; i++) {
        ((double *)sendbuf)[i] = (i + 1) * 1.0;
    }

    kupl_shm_request_h request;
    for (int i = 0; i < iter; i++) {
        auto *curr_sendbuf = static_cast<double *>(sendbuf) + unit_count * i;
        kupl_shm_allreduce_init(curr_sendbuf, recvbuf, unit_count, KUPL_SHM_DATATYPE_DOUBLE,
                                 KUPL_SHM_REDUCE_OP_SUM, kupl_comm, &request);
        kupl_shm_request_start(request);
        kupl_memcpy(curr_sendbuf, recvbuf, unit_count * sizeof(double));
        ASSERT_EQ(kupl_shm_request_free(request), 0);
    }

    for (int i = 0; i < count; i++) {
        double expected = (i + 1) * 1.0 * numprocs;
        ASSERT_DOUBLE_EQ(expected, ((double *)sendbuf)[i]);
    }

    ASSERT_EQ(kupl_shm_win_free(win_send), 0);
    ASSERT_EQ(kupl_shm_win_free(win_recv), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

template<typename T>
static inline
void do_alltoall_test(kupl_shm_datatype datatype, MPI_Datatype mpi_datatype)
{
    MPI_Comm comm = MPI_COMM_WORLD;
    int numprocs;
    MPI_Comm_size(comm, &numprocs);
    int myid;
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    kupl_shm_win_h win_send;
    kupl_shm_win_h win_recv;
    kupl_shm_win_h ompi_win_recv;
    void *sendbuf;
    void *recvbuf;
    void *ompibuf;
    int count = 1600;
    size_t buf_size = sizeof(T) * count;
    ASSERT_EQ(kupl_shm_win_alloc(buf_size * numprocs, kupl_comm, &sendbuf, &win_send), 0);
    ASSERT_EQ(kupl_shm_win_alloc(buf_size * numprocs, kupl_comm, &recvbuf, &win_recv), 0);
    ASSERT_EQ(kupl_shm_win_alloc(buf_size * numprocs, kupl_comm, &ompibuf, &ompi_win_recv), 0);

    // set sendbuf
    auto t_sendbuf = static_cast<T *>(sendbuf);
    auto t_recvbuf = static_cast<T *>(recvbuf);
    auto t_ompibuf = static_cast<T *>(ompibuf);
    for (int i = 0; i < count; i++) {
        t_sendbuf[i] = myid + i;
    }

    kupl_shm_request_h request;
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_alltoall_init(sendbuf, count, datatype, recvbuf, count, datatype, kupl_comm, &request), 0);
    ASSERT_EQ(kupl_shm_request_start(request), 0);
    MPI_Barrier(comm);
    MPI_Alltoall(sendbuf, count, mpi_datatype, ompibuf, count, mpi_datatype, comm);

    switch (datatype) {
        case KUPL_SHM_DATATYPE_CHAR:
        case KUPL_SHM_DATATYPE_INT:
        case KUPL_SHM_DATATYPE_LONG:
            for (int i = 0; i < count; i++) {
                ASSERT_EQ(t_recvbuf[i], t_ompibuf[i]);
            }
            break;
        case KUPL_SHM_DATATYPE_FLOAT:
            for (int i = 0; i < count; i++) {
                ASSERT_FLOAT_EQ(t_recvbuf[i], t_ompibuf[i]);
            }
            break;
        case KUPL_SHM_DATATYPE_DOUBLE:
            for (int i = 0; i < count; i++) {
                ASSERT_DOUBLE_EQ(t_recvbuf[i], t_ompibuf[i]);
            }
            break;
    }
    ASSERT_EQ(kupl_shm_request_free(request), 0);
    ASSERT_EQ(kupl_shm_win_free(win_send), 0);
    ASSERT_EQ(kupl_shm_win_free(win_recv), 0);
    ASSERT_EQ(kupl_shm_win_free(ompi_win_recv), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm, kupl_comm_test_alltoall_float)
{
    do_alltoall_test<float>(KUPL_SHM_DATATYPE_FLOAT, MPI_FLOAT);
}

TEST(test_comm, kupl_comm_test_alltoall_double)
{
    do_alltoall_test<double>(KUPL_SHM_DATATYPE_DOUBLE, MPI_DOUBLE);
}

TEST(test_comm, kupl_comm_test_alltoall_char)
{
    do_alltoall_test<char>(KUPL_SHM_DATATYPE_CHAR, MPI_CHAR);
}

TEST(test_comm, kupl_comm_test_alltoall_int)
{
    do_alltoall_test<int>(KUPL_SHM_DATATYPE_INT, MPI_INT);
}

TEST(test_comm, kupl_comm_test_alltoall_long)
{
    do_alltoall_test<long>(KUPL_SHM_DATATYPE_LONG, MPI_LONG);
}

TEST(test_comm_contig, kupl_comm_test_win_alloc_base)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 1), 0);
    kupl_shm_win_h win;
    void *baseptr;
    ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm, &baseptr, &win), 0);
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_win_alloc(0, kupl_comm, &baseptr, &win), 0);
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm_contig, kupl_comm_test_win_alloc_single_multiple_win)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 1), 0);
    kupl_shm_win_h win1, win2;
    void *baseptr1, *baseptr2;
    ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm, &baseptr1, &win1), 0);
    ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm, &baseptr2, &win2), 0);
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_win_free(win1), 0);
    ASSERT_EQ(kupl_shm_win_free(win2), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm_contig, kupl_comm_test_win_alloc_multiple_comm_non_overlap)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;
    int color;
    if (myid % 2 == 0) {
        color = 0;
    } else {
        color = 1;
    }
    MPI_Comm subcomm;
    MPI_Comm_split(comm, color, 0, &subcomm);
    int sub_id, sub_size;
    MPI_Comm_size(subcomm, &sub_size);
    MPI_Comm_rank(subcomm, &sub_id);
    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(sub_size, sub_id, getpid(), oob_cbs_h, (void *)subcomm, &kupl_comm), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 1), 0);
    kupl_shm_win_h win;
    void *baseptr;
    if (myid % 2 == 0) {
        ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm, &baseptr, &win), 0);
    } else {
        ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm, &baseptr, &win), 0);
    }
    MPI_Barrier(subcomm);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm_contig, kupl_comm_test_win_alloc_multiple_comm_overlap)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;
    int color;
    if (myid % 2 == 0) {
        color = 0;
    } else {
        color = 1;
    }
    MPI_Comm subcomm;
    MPI_Comm_split(comm, color, 0, &subcomm);
    int sub_id, sub_size;
    MPI_Comm_size(subcomm, &sub_size);
    MPI_Comm_rank(subcomm, &sub_id);
    kupl_shm_comm_h kupl_comm1, kupl_comm2;
    if (myid % 2 == 0) {
        ASSERT_EQ(kupl_shm_comm_create(sub_size, sub_id, getpid(), oob_cbs_h, (void *)subcomm, &kupl_comm1), 0);
    }
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm2), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 1), 0);
    kupl_shm_win_h win1, win2;
    void *baseptr1, *baseptr2;
    if (myid % 2 == 0) {
        ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm1, &baseptr1, &win1), 0);
    }
    ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm2, &baseptr2, &win2), 0);

    MPI_Barrier(comm);
    if (myid % 2 == 0) {
        ASSERT_EQ(kupl_shm_win_free(win1), 0);
        ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm1), 0);
    }
    ASSERT_EQ(kupl_shm_win_free(win2), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm2), 0);
}

TEST(test_comm_contig, kupl_comm_test_win_query)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 1), 0);
    kupl_shm_win_h win;
    void *baseptr;
    ASSERT_EQ(kupl_shm_win_alloc(256, kupl_comm, &baseptr, &win), 0);
    MPI_Barrier(comm);
    void *attach_address;
    kupl_shm_win_query(win, 0, &attach_address);
    ((int *)attach_address)[myid] = myid + 1;
    MPI_Barrier(comm);
    if (myid == 0) {
        for (int i = 0; i < numprocs; i++) {
            ASSERT_EQ(((int *)attach_address)[i], i + 1);
        }
    }
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm_contig, kupl_comm_test_peer_fence)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 1), 0);
    kupl_shm_win_h win;
    void *baseptr;
    ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm, &baseptr, &win), 0);
    MPI_Barrier(comm);
    // barrier test
    if (numprocs == 1) {
        ASSERT_EQ(kupl_shm_win_free(win), 0);
        ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
        return;
    }
    int win_rank;
    kupl_shm_comm_rank(nullptr, &win_rank);
    kupl_shm_comm_rank(kupl_comm, &win_rank);
    for (int i = 0; i < numprocs; i++) {
        for (int j = 1; j < numprocs; j++) {
            if (win_rank == 0) {
                kupl_shm_peer_fence(win, j);
            }
        }
        for (int j = 1; j < numprocs; j++) {
            if (win_rank == j) {
                kupl_shm_peer_fence(win, 0);
            }
        }
    }
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm_contig, kupl_comm_test_fence)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;
    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 1), 0);
    kupl_shm_win_h win;
    void *baseptr, *otherptr;
    ASSERT_EQ(kupl_shm_win_alloc(65536, kupl_comm, &baseptr, &win), 0);
    ((int*)baseptr)[0] = 0;
    MPI_Barrier(comm);
    MPI_Barrier(comm);
    int win_size;
    kupl_shm_comm_size(nullptr, &win_size);
    kupl_shm_comm_size(kupl_comm, &win_size);
    for (int i = 0; i < 5; i++) {
        ((int*)baseptr)[0]++;
        kupl_shm_fence(win);
        if (myid == 0) {
            for (int j = 0; j < win_size; j++) {
                kupl_shm_win_query(win, j, &otherptr);
                ASSERT_EQ(((int*)otherptr)[0], ((int*)baseptr)[0]);
            }
        }
        kupl_shm_fence(win);
        int cnt = 0;
        if (myid == 0) {
            while (cnt < 1000000) {
                cnt++;
            }
        }
        if (myid == 1) {
            while (cnt < 10000) {
                cnt++;
            }
        }
        if (myid == 2) {
            while (cnt < 100) {
                cnt++;
            }
        }
    }
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm_contig, kupl_comm_test_bcast_char)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    kupl_shm_win_h win;

    kupl_shm_request_h request;
    void *win_alloc_buffer;

    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 1), 0);
    size_t buf_size = 1000000;
    ASSERT_EQ(kupl_shm_win_alloc(buf_size, kupl_comm, &win_alloc_buffer, &win), 0);
    int count;

    count = 100000;
    if (myid == 0) {
        for (int i = 0; i < count; i++) {
            ((char *)win_alloc_buffer)[i] = (char)i;
        }
    }
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_bcast_init(win_alloc_buffer, count, KUPL_SHM_DATATYPE_CHAR, 0, kupl_comm, &request), 0);
    ASSERT_EQ(kupl_shm_request_start(request), 0);
    ASSERT_EQ(kupl_shm_request_wait(request), 0);
    for (int i = 0; i < count; i++) {
        ASSERT_EQ(((char *)win_alloc_buffer)[i], (char)i);
    }

    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_request_free(request), 0);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm_contig, kupl_comm_test_bcast_int)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    kupl_shm_win_h win;

    kupl_shm_request_h request;
    void *win_alloc_buffer;

    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 1), 0);
    size_t buf_size = 1000000;
    ASSERT_EQ(kupl_shm_win_alloc(buf_size, kupl_comm, &win_alloc_buffer, &win), 0);
    int count;

    count = 100000;
    if (myid == 0) {
        for (int i = 0; i < count; i++) {
            ((int *)win_alloc_buffer)[i] = (int)i;
        }
    }
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_bcast_init(win_alloc_buffer, count, KUPL_SHM_DATATYPE_INT, 0, kupl_comm, &request), 0);
    ASSERT_EQ(kupl_shm_request_start(request), 0);
    ASSERT_EQ(kupl_shm_request_wait(request), 0);
    for (int i = 0; i < count; i++) {
        ASSERT_EQ(((int *)win_alloc_buffer)[i], (int)i);
    }

    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_request_free(request), 0);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm_contig, kupl_comm_test_bcast_long)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;

    kupl_shm_comm_h kupl_comm;
    ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
    kupl_shm_win_h win;

    kupl_shm_request_h request;
    void *win_alloc_buffer;

    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 1), 0);
    size_t buf_size = 1000000;
    ASSERT_EQ(kupl_shm_win_alloc(buf_size, kupl_comm, &win_alloc_buffer, &win), 0);
    int count;

    count = 100000;
    if (myid == 0) {
        for (int i = 0; i < count; i++) {
            ((long *)win_alloc_buffer)[i] = (long)i;
        }
    }
    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_bcast_init(win_alloc_buffer, count, KUPL_SHM_DATATYPE_LONG, 0, kupl_comm, &request), 0);
    ASSERT_EQ(kupl_shm_request_start(request), 0);
    ASSERT_EQ(kupl_shm_request_wait(request), 0);
    for (int i = 0; i < count; i++) {
        ASSERT_EQ(((long *)win_alloc_buffer)[i], (long)i);
    }

    MPI_Barrier(comm);
    ASSERT_EQ(kupl_shm_request_free(request), 0);
    ASSERT_EQ(kupl_shm_win_free(win), 0);
    ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
}

TEST(test_comm_contig, kupl_comm_test_bcast_root)
{
    int myid, numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;
    ASSERT_EQ(kupl_shm_info_set(KUPL_SHM_INFO_IS_CONTIG, 1), 0);

    kupl_shm_comm_h kupl_comm;
    kupl_shm_win_h win;
    kupl_shm_request_h request;
    void *win_alloc_buffer;

    size_t buf_size = 100000;
    int count = 10000;

    for (int root = 0; root < numprocs - 1; root++) {
        ASSERT_EQ(kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm), 0);
        ASSERT_EQ(kupl_shm_win_alloc(buf_size, kupl_comm, &win_alloc_buffer, &win), 0);
        if (myid == root) {
            for (int i = 0; i < count; i++) {
                ((char *)win_alloc_buffer)[i] = (char)root;
            }
        }
        MPI_Barrier(comm);
        ASSERT_EQ(
            kupl_shm_bcast_init(win_alloc_buffer, count, KUPL_SHM_DATATYPE_CHAR, root, kupl_comm, &request), 0
        );
        ASSERT_EQ(kupl_shm_request_start(request), 0);
        ASSERT_EQ(kupl_shm_request_wait(request), 0);
        for (int i = 0; i < count; i++) {
            ASSERT_EQ(((char *)win_alloc_buffer)[i], (char)root);
        }
        ASSERT_EQ(kupl_shm_request_free(request), 0);
        ASSERT_EQ(kupl_shm_win_free(win), 0);
        ASSERT_EQ(kupl_shm_comm_destroy(kupl_comm), 0);
    }
}
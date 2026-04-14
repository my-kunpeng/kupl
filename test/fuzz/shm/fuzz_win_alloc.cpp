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
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "mpi.h"
#include "common/fuzz_common.h"

static const int RANGE_WIN_SIZE_MIN = 1;
static const int RANGE_WIN_SIZE_MAX = 2000000;

static int oob_allgather_callback(const void *sendbuf, void *recvbuf, int size, void *group,
                                  kupl_shm_datatype_t datatype)
{
    if (sendbuf == nullptr) {
        sendbuf = (void*)(1);
    }
    switch (datatype) {
        case KUPL_SHM_DATATYPE_CHAR:
            return MPI_Allgather(sendbuf, size, MPI_CHAR, recvbuf, size, MPI_CHAR, (MPI_Comm)group);
        case KUPL_SHM_DATATYPE_INT:
            return MPI_Allgather(sendbuf, size, MPI_INT, recvbuf, size, MPI_INT, (MPI_Comm)group);
        case KUPL_SHM_DATATYPE_LONG:
            return MPI_Allgather(sendbuf, size, MPI_LONG, recvbuf, size, MPI_LONG, (MPI_Comm)group);
        case KUPL_SHM_DATATYPE_FLOAT:
            return MPI_Allgather(sendbuf, size, MPI_FLOAT, recvbuf, size, MPI_FLOAT, (MPI_Comm)group);
        case KUPL_SHM_DATATYPE_DOUBLE:
            return MPI_Allgather(sendbuf, size, MPI_DOUBLE, recvbuf, size, MPI_DOUBLE, (MPI_Comm)group);
        default:
            fprintf(stderr, "not support datatype\n");
            return KUPL_ERROR;
    }
}

static int oob_barrier_callback(void *group)
{
    return MPI_Barrier((MPI_Comm)group);
}

void win_alloc_example(int test_count)
{
    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);
    int myid;
    int numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        kupl_shm_comm_h kupl_comm;
        kupl_shm_win_h win;
        void *baseptr;
        kupl_shm_win_h *win_p = &win;
        void **baseptr_p = &baseptr;
        kupl_shm_oob_cb_t oob_cbs;
        kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
        oob_cbs_h->oob_allgather = oob_allgather_callback;
        oob_cbs_h->oob_barrier = oob_barrier_callback;
        kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm);

        int alloc_size = *(int *)DT_SetGetNumberRange(&g_Element[0], RANGE_WIN_SIZE_MIN,
                                                      RANGE_WIN_SIZE_MIN, RANGE_WIN_SIZE_MAX);
        MPI_Bcast(&alloc_size, 1, MPI_INT, 0, comm);

        if (kupl_shm_win_alloc(alloc_size, kupl_comm, baseptr_p, win_p) == 0) {
            kupl_shm_win_free(win);
        }
        if (kupl_comm) {
            kupl_shm_comm_destroy(kupl_comm);
        }
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}

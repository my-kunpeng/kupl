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

static const int RANGE_COUNT_DEFAULT = 8000;
static const int RANGE_COUNT_MIN = 1;
static const int RANGE_COUNT_MAX = 10000;
static const int RANGE_DATATYPE_DEFAULT = 4;
static const int RANGE_DATATYPE_MIN = 0;
static const int RANGE_DATATYPE_MAX = 4;
static const int RANGE_DATA_DEFAULT = 10;
static const int RANGE_DATA_MIN = -100;
static const int RANGE_DATA_MAX = 100;

static const kupl_shm_datatype_t datatype_list[] = { KUPL_SHM_DATATYPE_CHAR, KUPL_SHM_DATATYPE_INT,
                                                     KUPL_SHM_DATATYPE_LONG, KUPL_SHM_DATATYPE_FLOAT,
                                                     KUPL_SHM_DATATYPE_DOUBLE };

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
            fprintf(stderr, "not support datatype");
            return KUPL_ERROR;
    }
}

static int oob_barrier_callback(void *group)
{
    return MPI_Barrier((MPI_Comm)group);
}

static void set_buffer(void *sendbuf, int count, int data, const kupl_shm_datatype_t &datatype)
{
    for (int i = 0; i < count; i++) {
        switch (datatype) {
            case KUPL_SHM_DATATYPE_CHAR:
                (static_cast<char *>(sendbuf))[i] = data;
                break;
            case KUPL_SHM_DATATYPE_INT:
                (static_cast<int *>(sendbuf))[i] = data;
                break;
            case KUPL_SHM_DATATYPE_LONG:
                (static_cast<long *>(sendbuf))[i] = data;
                break;
            case KUPL_SHM_DATATYPE_FLOAT:
                (static_cast<float *>(sendbuf))[i] = data;
                break;
            case KUPL_SHM_DATATYPE_DOUBLE:
                (static_cast<double *>(sendbuf))[i] = data;
                break;
        }
    }
}

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
    }
}

void alltoall_example(int test_count)
{
    printf("start -- %s\n", __func__);
    DT_Enable_Leak_Check(0, 0);
    int myid;
    int numprocs;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &myid);
    kupl_shm_oob_cb_t oob_cbs;
    kupl_shm_oob_cb_h oob_cbs_h = &oob_cbs;
    oob_cbs_h->oob_allgather = oob_allgather_callback;
    oob_cbs_h->oob_barrier = oob_barrier_callback;
    DT_FUZZ_START(0, test_count, (char *)__func__, 0)
    {
        kupl_shm_comm_h kupl_comm = nullptr;
        kupl_shm_win_h send_win = nullptr;
        kupl_shm_win_h recv_win = nullptr;
        void *sendbuf = nullptr;
        void *recvbuf = nullptr;
        kupl_shm_request_h request = nullptr;
        int ret_comm = kupl_shm_comm_create(numprocs, myid, getpid(), oob_cbs_h, (void *)comm, &kupl_comm);

        int cnt = 0;
        int count =
            *(int *)DT_SetGetNumberRange(&g_Element[cnt++], RANGE_COUNT_DEFAULT, RANGE_COUNT_MIN, RANGE_COUNT_MAX);
        int datatype_idx = *(int *)DT_SetGetNumberRange(&g_Element[cnt++], RANGE_DATATYPE_DEFAULT, RANGE_DATATYPE_MIN,
                                                        RANGE_DATATYPE_MAX);
        int data = *(int *)DT_SetGetNumberRange(&g_Element[cnt++], RANGE_DATA_DEFAULT, RANGE_DATA_MIN, RANGE_DATA_MAX);
        MPI_Bcast(&count, 1, MPI_INT, 0, comm);
        MPI_Bcast(&datatype_idx, 1, MPI_INT, 0, comm);
        MPI_Bcast(&data, 1, MPI_INT, 0, comm);

        if (ret_comm != KUPL_OK) {
            continue;
        }

        kupl_shm_datatype_t datatype = datatype_list[datatype_idx];
        int type_size;
        kupl_shm_type_size(datatype, &type_size);
        size_t buf_size = count * type_size;
        int ret_send = kupl_shm_win_alloc(buf_size * numprocs, kupl_comm, &sendbuf, &send_win);
        int ret_recv = kupl_shm_win_alloc(buf_size * numprocs, kupl_comm, &recvbuf, &recv_win);
        if (ret_send == KUPL_OK) {
            set_buffer(sendbuf, count, data, datatype);
        }

        if (ret_send == KUPL_OK && ret_recv == KUPL_OK) {
            int ret =
                kupl_shm_alltoall_init(sendbuf, count, datatype, recvbuf, count, datatype, kupl_comm, &request);
            if (ret == KUPL_OK) {
                kupl_shm_request_start(request);
                kupl_shm_request_wait(request);
                kupl_shm_request_free(request);
            }
        }

        if (ret_send == KUPL_OK) {
            kupl_shm_win_free(send_win);
        }
        if (ret_recv == KUPL_OK) {
            kupl_shm_win_free(recv_win);
        }
        kupl_shm_comm_destroy(kupl_comm);
    }
    DT_FUZZ_END();
    printf("end -- %s\n", __func__);
}

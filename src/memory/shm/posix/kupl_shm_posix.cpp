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
#include "kupl_shm_posix.h"
#include <cerrno>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "memory/mpool/kupl_mpool.h"
#include "utils/time/kupl_time.h"
#include "tools/struct/kupl_vla.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/sys/kupl_math.h"
#include "utils/sys/kupl_glibc_version.h"

static kupl_always_inline kupl_shm_info_posix_args *kupl_shm_posix_get_info(kupl_shm_base_info *info, int idx)
{
    return &(reinterpret_cast<kupl_shm_info_posix_args *>(info))[idx];
}

static kupl_always_inline int kupl_shm_posix_oob_allgather(kupl_shm_win_h win)
{
    char *mmap_filename_array;
    int myid = win->rank;
    int numprocs = win->size;

    mmap_filename_array = (char *)kupl_malloc_inner(sizeof(char) * KUPL_SHM_MMAP_PATH_MAX * (size_t)numprocs);
    if (mmap_filename_array == nullptr) {
        return kupl_log_error_return(ERROR, "kupl_malloc failed, errno = %s", strerror(errno));
    }
    kupl_shm_oob_allgather_cb_t oob_allgather = win->comm->oob_allgather;
    oob_allgather(kupl_shm_posix_get_info(win->info, myid)->mmap_filename, mmap_filename_array, KUPL_SHM_MMAP_PATH_MAX,
                  win->comm->group, KUPL_SHM_DATATYPE_CHAR);
    if (shm_info.is_contig) {
        for (int i = 1; i < numprocs; i++) {
            char *filename = mmap_filename_array + i * KUPL_SHM_MMAP_PATH_MAX;
            memcpy(filename, mmap_filename_array, KUPL_SHM_MMAP_PATH_MAX);
        }
    }
    kupl_shm_posix_get_info(win->info, myid)->mmap_filename_array = mmap_filename_array;

    return KUPL_OK;
}

static uint64_t kupl_shm_rand()
{
    unsigned int seed = (unsigned int)time(nullptr);
    return (uint64_t)rand_r(&seed);
}

static kupl_always_inline void display_shm_open_error_and_solution(int errnum)
{
    switch (errnum) {
        case EACCES:
            kupl_error("The specified mmap file has no permission to access. Please check the permissions of the "
                       "/dev/shm directory.\n");
            break;
        case ENOENT:
            kupl_error("The mmap file was accidentally deleted. Please check if there is a program deleting files in "
                       "the /dev/shm directory.");
            break;
        case ENOMEM:
            kupl_error("Insufficient shared memory available. Please try to clean up the files in the /dev/shm "
                       "directory and try again.\n");
            break;
        default:
            kupl_error("Error Reason: shm open failed: (%d: %s).\n", errnum, strerror(errnum));
            break;
    }
}

static kupl_always_inline void display_mmap_error_and_solution(int errnum)
{
    switch (errnum) {
        case ENOMEM:
            kupl_error("The number of mmaps has reached the upper limit of the system configuration. Please check the "
                       "system file /proc/sys/vm/max_map_count\n");
            break;
        default:
            kupl_error("Error Reason: shm mmap failed: (%d: %s).\n", errnum, strerror(errnum));
            break;
    }
}

static kupl_always_inline int kupl_shm_posix_alloc(size_t size, kupl_shm_win_h win)
{
    int ret = KUPL_ERROR;
    int pid = win->comm->pid;
    int myid = win->rank;
    char mmap_filename[KUPL_SHM_MMAP_PATH_MAX];
    void *base_ptr;

    memset(mmap_filename, 0, KUPL_SHM_MMAP_PATH_MAX);
    uint64_t shm_id = kupl_shm_rand();
    snprintf(mmap_filename, sizeof(mmap_filename), KUPL_SHM_MMAP_FILE_FMT, shm_id, kupl_now_ns(), pid);

    int seg_id = shm_open(mmap_filename, KUPL_SHM_MMAP_CREATE_FLAGS, KUPL_SHM_MMAP_OPEN_MODE);
    if (seg_id < 0) {
        display_shm_open_error_and_solution(errno);
        goto out;
    }
    if (ftruncate(seg_id, (long)size) == -1) {
        kupl_error("ftruncate failed, errno = %s", strerror(errno));
        goto out_unlink;
    }
    kupl_shm_posix_get_info(win->info, myid)->mmap_size = size;
    kupl_shm_posix_get_info(win->info, myid)->offset = 0;
    memcpy(kupl_shm_posix_get_info(win->info, myid)->mmap_filename, mmap_filename, KUPL_SHM_MMAP_PATH_MAX);

    base_ptr = mmap(nullptr, size, KUPL_SHM_MMAP_PROT, MAP_SHARED, seg_id, 0);
    if (base_ptr == MAP_FAILED) {
        display_mmap_error_and_solution(errno);
        goto out_unlink;
    }
    win->base_ptr = base_ptr;
    kupl_shm_posix_get_info(win->info, myid)->super.attach_address = base_ptr;
    ret = KUPL_OK;
    goto out_close;

out_unlink:
    if (shm_unlink(mmap_filename) == -1) {
        kupl_error("shm_unlink failed, errno = %s", strerror(errno));
    }
out_close:
    close(seg_id);
out:
    return ret;
}

static kupl_always_inline int kupl_shm_posix_attach(kupl_shm_win_h win, size_t *offset_list)
{
    int ret = KUPL_ERROR;
    int myid = win->rank;
    int numprocs = win->size;
    char *mmap_filename_array = kupl_shm_posix_get_info(win->info, myid)->mmap_filename_array;
    kupl_vla<size_t> seg_size((size_t)numprocs);
    if (kupl_unlikely(seg_size.get_data() == nullptr)) {
        return ret;
    }
    kupl_shm_oob_allgather_cb_t oob_allgather = win->comm->oob_allgather;
    oob_allgather(&(kupl_shm_posix_get_info(win->info, myid)->mmap_size), seg_size.get_data(), sizeof(size_t),
                  win->comm->group, KUPL_SHM_DATATYPE_CHAR);
    int err_idx;
    for (int i = 0; i < numprocs; i++) {
        if (((!shm_info.is_contig) && (i == myid)) || ((shm_info.is_contig) && (myid == 0) && (i == myid)) ||
            seg_size[i] == 0) {
            kupl_shm_posix_get_info(win->info, i)->mmap_size = seg_size[i];
            continue;
        }
        char *remote_filename = mmap_filename_array + i * KUPL_SHM_MMAP_PATH_MAX;
        memcpy(kupl_shm_posix_get_info(win->info, i)->mmap_filename, remote_filename, KUPL_SHM_MMAP_PATH_MAX);
        int seg_id = shm_open(remote_filename, KUPL_SHM_MMAP_ATTACH_FLAGS, KUPL_SHM_MMAP_OPEN_MODE);
        if (seg_id < 0) {
            display_shm_open_error_and_solution(errno);
            err_idx = i;
            goto err;
        }
        void *attach_address = mmap(nullptr, seg_size[i], KUPL_SHM_MMAP_PROT, MAP_SHARED, seg_id, 0);
        if (attach_address == MAP_FAILED) {
            display_mmap_error_and_solution(errno);
            close(seg_id);
            err_idx = i;
            goto err;
        }
        close(seg_id);
        kupl_shm_posix_get_info(win->info, i)->mmap_size = seg_size[i];
        kupl_shm_posix_get_info(win->info, i)->super.attach_address = attach_address;
        kupl_shm_posix_get_info(win->info, i)->offset = offset_list[i];
    }
    return KUPL_OK;
err:
    for (int j = 0; j < err_idx; j++) {
        if (j == myid) {
            continue;
        }
        kupl_shm_info_posix_args *item = kupl_shm_posix_get_info(win->info, j);
        munmap(item->super.attach_address, item->mmap_size);
    }
    return ret;
}

static kupl_always_inline int kupl_shm_posix_win_add(kupl_shm_comm_h comm, kupl_shm_win_h win, int flag)
{
    if (flag) {
        kupl_list_insert_after(&comm->win_list, &win->list);
    }
    return KUPL_OK;
}

static kupl_always_inline int kupl_shm_posix_win_del(kupl_shm_win_h win, int flag)
{
    if (flag) {
        kupl_list_del(&win->list);
    }
    return KUPL_OK;
}

int kupl_shm_posix_init()
{
    return KUPL_OK;
}

int kupl_shm_posix_finalize()
{
    return KUPL_OK;
}

void kupl_shm_get_aligned_size(size_t *size)
{
    if ((*size) <= UINT64_MAX - (uint64_t)PAGE_SIZE) {
        (*size) = kupl_shm_align_up_pow2(*size, (uint64_t)PAGE_SIZE);
    }
}

int kupl_shm_posix_win_alloc(size_t size, kupl_shm_comm_h comm, void **baseptr, kupl_shm_win_h *win, int flag,
                             size_t *offset_list)
{
    int ret = KUPL_ERROR;
    kupl_shm_win_h win_temp;
    kupl_shm_info_posix_args *item;

    win_temp = (kupl_shm_win *)kupl_malloc_inner(sizeof(kupl_shm_win));
    if (win_temp == nullptr) {
        kupl_error("kupl_malloc failed, errno = %s", strerror(errno));
        goto err;
    }
    int myid;
    int numprocs;
    myid = comm->rank;
    numprocs = comm->size;
    win_temp->rank = myid;
    win_temp->size = numprocs;
    win_temp->comm = comm;
    win_temp->info = (kupl_shm_base_info *)kupl_malloc_inner(sizeof(kupl_shm_info_posix_args) * (size_t)numprocs);
    if (win_temp->info == nullptr) {
        kupl_error("kupl_malloc failed, errno = %s", strerror(errno));
        goto err_free_win;
    }
    kupl_shm_get_aligned_size(&size);
    if ((!shm_info.is_contig || (win_temp->rank == 0)) && size) {
        if (kupl_shm_posix_alloc(size, win_temp) == KUPL_ERROR) {
            goto err_free_info;
        }
    } else {
        kupl_shm_posix_get_info(win_temp->info, myid)->offset = 0;
        kupl_shm_posix_get_info(win_temp->info, myid)->mmap_size = size;
    }
    if (kupl_shm_posix_oob_allgather(win_temp) == KUPL_ERROR) {
        goto err_mummap_unlink;
    }
    if (kupl_shm_posix_attach(win_temp, offset_list) == KUPL_ERROR) {
        goto err_free_mmap_filename_array;
    }
    if (kupl_shm_posix_win_add(comm, win_temp, flag) == KUPL_ERROR) {
        goto err_mummap_other;
    }
    *baseptr = ((char *)kupl_shm_posix_get_info((win_temp)->info, myid)->super.attach_address) +
               kupl_shm_posix_get_info((win_temp)->info, myid)->offset;
    win_temp->base_ptr = ((char *)kupl_shm_posix_get_info((win_temp)->info, myid)->super.attach_address) +
                         kupl_shm_posix_get_info((win_temp)->info, myid)->offset;
    *win = win_temp;
    return KUPL_OK;
err_mummap_other:
    for (int i = 0; i < numprocs; i++) {
        if (i != myid) {
            item = kupl_shm_posix_get_info((win_temp)->info, i);
            if (munmap(item->super.attach_address, item->mmap_size) == -1) {
                kupl_error("munmap failed, errno = %s", strerror(errno));
            }
        }
    }
err_free_mmap_filename_array:
    kupl_safe_free(kupl_shm_posix_get_info((win_temp)->info, myid)->mmap_filename_array);
err_mummap_unlink:
    item = kupl_shm_posix_get_info((win_temp)->info, myid);
    if (munmap(item->super.attach_address, item->mmap_size) == -1) {
        kupl_error("munmap failed, errno = %s", strerror(errno));
    }
    if (shm_unlink(item->mmap_filename) == -1) {
        kupl_error("shm_unlink failed, errno = %s", strerror(errno));
    }
err_free_info:
    kupl_safe_free((win_temp)->info);
err_free_win:
    kupl_safe_free(win_temp);
err:
    return ret;
}

int kupl_shm_posix_win_query(kupl_shm_win_h win, int remote_rank, void **baseptr)
{
    *baseptr = ((char *)kupl_shm_posix_get_info(win->info, remote_rank)->super.attach_address) +
               kupl_shm_posix_get_info(win->info, remote_rank)->offset;
    return KUPL_OK;
}

int kupl_shm_posix_win_free(kupl_shm_win_h win, int flag)
{
    int myid = win->rank;
    int numprocs = win->size;
    for (int i = 0; i < numprocs; i++) {
        if (!kupl_shm_posix_get_info(win->info, i)->mmap_size) {
            continue;
        }
        kupl_shm_info_posix_args *item = kupl_shm_posix_get_info(win->info, i);
        if (munmap(item->super.attach_address, item->mmap_size) == -1) {
            kupl_error("munmap failed, errno = %s", strerror(errno));
        }
    }
    if ((kupl_shm_posix_get_info(win->info, myid)->mmap_size) && ((!shm_info.is_contig) || (myid == 0))) {
        if (shm_unlink(kupl_shm_posix_get_info(win->info, myid)->mmap_filename) == -1) {
            kupl_error("shm_unlink failed, errno = %s", strerror(errno));
        }
    }

    kupl_shm_posix_win_del(win, flag);

    kupl_safe_free(kupl_shm_posix_get_info(win->info, myid)->mmap_filename_array);
    kupl_safe_free(win->info);
    kupl_safe_free(win);
    return KUPL_OK;
}

static const kupl_shm_ops_t g_kupl_shm_posix_ops = {.init = kupl_shm_posix_init,
                                                    .finalize = kupl_shm_posix_finalize,
                                                    .shm_win_alloc = kupl_shm_posix_win_alloc,
                                                    .shm_win_free = kupl_shm_posix_win_free,
                                                    .shm_win_query = kupl_shm_posix_win_query};

void kupl_shm_posix_reg_ops()
{
    kupl_shm_ops_set(KUPL_SHM_POSIX, &g_kupl_shm_posix_ops);
}

void kupl_shm_posix_dereg_ops()
{
    kupl_shm_ops_set(KUPL_SHM_POSIX, nullptr);
}
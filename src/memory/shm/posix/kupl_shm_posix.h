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
#ifndef KUPL_SHM_POSIX_H
#define KUPL_SHM_POSIX_H

#include <cinttypes>
#include "memory/shm/kupl_shm.h"

#ifdef __cplusplus
extern "C" {
#endif
#define KBYTE    (1uLL << 10)
#define MBYTE    (1uLL << 20)
#define GBYTE    (1uLL << 30)
#define TBYTE    (1uLL << 40)
#define KUPL_SHM_MMAP_PATH_MAX 64
#define KUPL_SHM_MMAP_FILE_FMT "/shm_mmap_%" PRIx64 "_%" PRIu64 "_%" PRId32
#define KUPL_SHM_MMAP_CREATE_FLAGS (O_CREAT | O_EXCL | O_RDWR)
#define KUPL_SHM_MMAP_ATTACH_FLAGS (O_RDWR)
#define KUPL_SHM_MMAP_OPEN_MODE 0600
#define KUPL_SHM_MMAP_PROT (PROT_READ | PROT_WRITE)
#define KUPL_SHM_MMAP_FLAGS (MAP_SHARED)

#define kupl_shm_min(_a, _b) \
({ \
    __typeof__(_a) _min_a = (_a); \
    __typeof__(_b) _min_b = (_b); \
    (_min_a < _min_b) ? _min_a : _min_b; \
})

typedef struct kupl_shm_info_posix_args {
    kupl_shm_base_info super;
    size_t mmap_size;
    size_t offset;
    char mmap_filename[KUPL_SHM_MMAP_PATH_MAX];
    char *mmap_filename_array;
} kupl_shm_info_posix_args_t;

void kupl_shm_posix_reg_ops(void);
void kupl_shm_posix_dereg_ops(void);

#ifdef __cplusplus
}
#endif

#endif
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
#ifndef KUPL_SDMA_H
#define KUPL_SDMA_H
#include <string>
#include <sched.h>
#include "utils/arch/kupl_atomic.h"
#include "utils/sys/kupl_dl_module.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDMA_FUNC_NUM 8
#define KUPL_SDMA_LIB "/usr/lib64/libsdma_dk.so"
#define KUPL_SDMA_START 1
#define KUPL_SDMA_FINISHED 0
#define KUPL_SDMA_UNFINISHED (-8)

#define SDMA_CHN_INDEX_INIT (-1)
#define KUPL_SDMA_RESERVED_CHN_SIZE 2
#define KUPL_MAX_SDMA_CHN_SIZE CPU_SETSIZE
#define KUPL_MAX_SDMA_DEVICE_SIZE 4
#define KUPL_SDMA_DEVICE_NAME_LENGTH 20

typedef void (*sdma_task_callback)(int task_status, void *task_data);

typedef struct sdma_sqe_task {
    uint64_t src_addr;
    uint64_t dst_addr;
    uint32_t src_process_id;
    uint32_t dst_process_id;
    uint32_t src_stride_len;
    uint32_t dst_stride_len;
    uint32_t stride_num;
    uint32_t length;
    uint8_t  opcode;
    uint8_t  mpam_partid;
    uint8_t  pmg : 2;
    uint8_t  resvd1 : 6;
    uint8_t  qos : 4;
    uint8_t  resvd2 : 4;
    sdma_task_callback task_cb;
    void *task_data;
    struct sdma_sqe_task *next_sqe;
} sdma_sqe_task_t;

typedef struct sdma_request {
    uint16_t	req_id;
    uint32_t	req_cnt;
    uint32_t	round_cnt;
} sdma_request_t;

typedef void* (*init_chn_sdma)(int, int);

typedef int (*get_process_id_sdma)(int, uint32_t *);

typedef int (*deinit_chn_sdma)(void *);

typedef int (*icopy_data_sdma)(void *, sdma_sqe_task_t *, uint32_t, sdma_request_t *);

typedef int (*iwait_chn_sdma)(void *, sdma_request_t *);

typedef int (*iquery_chn_sdma)(void *, sdma_request_t *);

typedef int (*pin_umem_sdma)(int, void *, uint32_t, uint64_t *);

typedef int (*unpin_umem_sdma)(int, uint64_t);

typedef struct sdma_func_list {
    init_chn_sdma kupl_sdma_init_chn;
    get_process_id_sdma kupl_sdma_get_process_id;
    deinit_chn_sdma kupl_sdma_deinit_chn;
    icopy_data_sdma kupl_sdma_icopy_data;
    iwait_chn_sdma kupl_sdma_iwait_chn;
    iquery_chn_sdma kupl_sdma_iquery_chn;
    pin_umem_sdma kupl_sdma_pin_umem;
    unpin_umem_sdma kupl_sdma_unpin_umem;
} sdma_func_list_t;

typedef void* kupl_sdma_chn_h;
extern kupl_sdma_chn_h g_sdma_chns[KUPL_MAX_SDMA_CHN_SIZE];
extern int g_sdma_fd[KUPL_MAX_SDMA_DEVICE_SIZE];
extern std::atomic<int> g_sdma_chn_num[KUPL_MAX_SDMA_DEVICE_SIZE];
extern uint32_t g_sdma_process_id[KUPL_MAX_SDMA_DEVICE_SIZE];
extern int core_number;
extern int cores_per_sdma;
extern bool g_sdma_func_init;

int kupl_sdma_module_init(void);

void kupl_sdma_module_fini(void);

/**
 * @brief get the sdma_func_list_t
 */
sdma_func_list_t get_sdma_dl_func_l(void);

#ifdef __cplusplus
}
#endif

#endif
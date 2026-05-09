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

#include "sdma_module.h"
#include <unistd.h>
#include <fcntl.h>
#include "utils/debug/kupl_log.h"
#include "utils/sys/kupl_hardware.h"
#include "executor/backend/kupl_executor_backend.h"

static bool isopen_sdma_dl = false;
static kupl_dl_module_t *g_kupl_sdma_module = nullptr;
static sdma_func_list_t sdma_dl_func_l;

kupl_sdma_chn_h g_sdma_chns[KUPL_MAX_SDMA_CHN_SIZE];
int g_sdma_fd[KUPL_MAX_SDMA_DEVICE_SIZE];
std::atomic<int> g_sdma_chn_num[KUPL_MAX_SDMA_DEVICE_SIZE];
uint32_t g_sdma_process_id[KUPL_MAX_SDMA_DEVICE_SIZE];
int core_number;
int cores_per_sdma;
bool g_sdma_func_init = false;

bool open_kupl_sdma_dl_module()
{
    if (isopen_sdma_dl) {
        return true;
    }
    const char *lib_sdma_path = KUPL_SDMA_LIB;
    int ret = access(lib_sdma_path, F_OK);
    if (ret == -1) {
        return false;
    }

    std::string sdma_func_name[SDMA_FUNC_NUM] = {"sdma_init_chn",   "sdma_get_process_id", "sdma_deinit_chn",
                                                 "sdma_icopy_data", "sdma_iwait_chn",      "sdma_iquery_chn",
                                                 "sdma_pin_umem",   "sdma_unpin_umem"};

    kupl_dl_module_sym_t sdma_reg_ops[SDMA_FUNC_NUM];
    for (int i = 0; i < SDMA_FUNC_NUM; i++) {
        sdma_reg_ops[i].sym = nullptr;
        sdma_reg_ops[i].sym_name = sdma_func_name[i].c_str();
    }

    g_kupl_sdma_module = kupl_dl_open(lib_sdma_path, sdma_reg_ops, SDMA_FUNC_NUM);
    if (g_kupl_sdma_module) {
        isopen_sdma_dl = true;
        sdma_dl_func_l = {
            .kupl_sdma_init_chn = (init_chn_sdma)sdma_reg_ops[0].sym,
            .kupl_sdma_get_process_id = (get_process_id_sdma)sdma_reg_ops[1].sym,
            .kupl_sdma_deinit_chn = (deinit_chn_sdma)sdma_reg_ops[2].sym,
            .kupl_sdma_icopy_data = (icopy_data_sdma)sdma_reg_ops[3].sym,
            .kupl_sdma_iwait_chn = (iwait_chn_sdma)sdma_reg_ops[4].sym,
            .kupl_sdma_iquery_chn = (iquery_chn_sdma)sdma_reg_ops[5].sym,
            .kupl_sdma_pin_umem = (pin_umem_sdma)sdma_reg_ops[6].sym,
            .kupl_sdma_unpin_umem = (unpin_umem_sdma)sdma_reg_ops[7].sym,
        };
        return true;
    } else {
        kupl_error("dlopen sdma failed");
        return false;
    }
}

bool kupl_sdma_func_init()
{
    if (!open_kupl_sdma_dl_module()) {
        return false;
    }
    if (sdma_dl_func_l.kupl_sdma_init_chn && sdma_dl_func_l.kupl_sdma_get_process_id &&
        sdma_dl_func_l.kupl_sdma_deinit_chn && sdma_dl_func_l.kupl_sdma_icopy_data &&
        sdma_dl_func_l.kupl_sdma_iwait_chn && sdma_dl_func_l.kupl_sdma_iquery_chn &&
        sdma_dl_func_l.kupl_sdma_pin_umem && sdma_dl_func_l.kupl_sdma_unpin_umem) {
        return true;
    } else {
        return false;
    }
}

sdma_func_list_t get_sdma_dl_func_l()
{
    return sdma_dl_func_l;
}

void close_kupl_sdma_dl_module()
{
    if (isopen_sdma_dl && g_kupl_sdma_module) {
        kupl_dl_close(g_kupl_sdma_module);
    }
    isopen_sdma_dl = false;
}

void kupl_sdma_func_fini()
{
    close_kupl_sdma_dl_module();
}

int kupl_sdma_dev_init()
{
    const kupl_host_info_t *info = kupl_get_host_info();
    core_number = info->pu_cnt;
    int num_executors = info->avail_pu_cnt;
    cores_per_sdma = kupl_config_get_value(KUPL_CORES_PER_SDMA);
    if (core_number <= 0 || cores_per_sdma <= 0) {
        return kupl_log_error_return(WARN, "get incorrect core_number or cores_per_sdma value");
    }
    char sdma_dev[KUPL_SDMA_DEVICE_NAME_LENGTH];
    uint32_t process_id;
    for (int i = 0; i < KUPL_MAX_SDMA_DEVICE_SIZE; i++) {
        g_sdma_fd[i] = -1;
    }
    for (int i = 0; i < core_number; i++) {
        g_sdma_chns[i] = nullptr;
    }
    for (int i = 0; i < KUPL_MAX_SDMA_DEVICE_SIZE; i++) {
        sprintf(sdma_dev, "/dev/sdma%d", i);
        g_sdma_fd[i] = open(sdma_dev, O_RDWR);
        if (g_sdma_fd[i] < 0) {
            return kupl_log_error_return(WARN, "Failed to create src_sdma_fd");
        }
        int ret = sdma_dl_func_l.kupl_sdma_get_process_id(g_sdma_fd[i], &process_id);
        if (ret != 0) {
            return kupl_log_error_return(WARN, "get process_id failed");
        }
        g_sdma_process_id[i] = process_id;
        g_sdma_chn_num[i] = kupl_get_self_affinity();
    }
    int fd_index = kupl_get_self_affinity() / cores_per_sdma % KUPL_MAX_SDMA_DEVICE_SIZE;
    for (int i = 0; i < num_executors * KUPL_SDMA_RESERVED_CHN_SIZE; i++) {
        if (g_sdma_chn_num[fd_index] + i >= (fd_index + 1) * cores_per_sdma) {
            break;
        }
        g_sdma_chns[g_sdma_chn_num[fd_index] + i] =
            sdma_dl_func_l.kupl_sdma_init_chn(g_sdma_fd[fd_index], (g_sdma_chn_num[fd_index] + i) % cores_per_sdma);
        if (g_sdma_chns[g_sdma_chn_num[fd_index] + i] == nullptr) {
            return kupl_log_error_return(WARN, "creat sdma channl failed");
        }
    }
    return KUPL_OK;
}

void kupl_sdma_dev_fini()
{
    for (int i = 0; i < core_number; i++) {
        if (g_sdma_chns[i] != nullptr) {
            sdma_dl_func_l.kupl_sdma_deinit_chn(g_sdma_chns[i]);
        }
    }
    for (int i = 0; i < KUPL_MAX_SDMA_DEVICE_SIZE; i++) {
        if (g_sdma_fd[i] >= 0) {
            close(g_sdma_fd[i]);
        }
    }
}

int kupl_sdma_module_init()
{
    if (kupl_arch_detect() != KUPL_CPU_HISILICOM_920F) {
        return KUPL_OK;
    }
    g_sdma_func_init = kupl_sdma_func_init();
    if (g_sdma_func_init) {
        int ret = kupl_sdma_dev_init();
        if (ret == KUPL_ERROR) {
            kupl_sdma_dev_fini();
            g_sdma_func_init = false;
        }
    }
    return KUPL_OK;
}

void kupl_sdma_module_fini()
{
    if (g_sdma_func_init) {
        kupl_sdma_dev_fini();
        kupl_sdma_func_fini();
        g_sdma_func_init = false;
    }
    return;
}
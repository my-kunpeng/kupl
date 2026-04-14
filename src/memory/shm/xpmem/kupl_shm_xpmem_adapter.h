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
#ifndef KUPL_SHM_XPMEM_ADAPTER_H
#define KUPL_SHM_XPMEM_ADAPTER_H

extern "C" {
    #include "xpmem.h"
}

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t kupl_shm_xpmem_segid_t;
typedef int64_t kupl_shm_xpmem_apid_t;

typedef struct kupl_shm_xpmem_addr {
    kupl_shm_xpmem_apid_t apid;
    off_t offset;
} kupl_shm_xpmem_addr_t;


/**
 * @brief get the XPMEM version
 *
 * @return int                      get the XPMEM version result,  greater than 0 for success, -1 for error
 */
int kupl_shm_xpmem_version(void);

/**
 * @brief share a memory block
 *
 * @param [in] vaddr                tarting address of region to share
 * @param [in] size                 number of bytes to share
 * @param [in] permit_type          only XPMEM_PERMIT_MODE currently defined
 * @param [in] permit_value         permissions mode expressed as an octal value
 * @return kupl_shm_xpmem_segid_t  make result, 64-bit segment ID for success, -1 for error
 */
kupl_shm_xpmem_segid_t kupl_shm_xpmem_make(void *vaddr, size_t size, int permit_type, void *permit_value);

/**
 * @brief revoke access to a shared memory block
 *
 * @param [in] segid                64-bit segment ID of the region to stop sharing
 * @return int                      remove result, 0 for success, -1 for error
 */
int kupl_shm_xpmem_remove(kupl_shm_xpmem_segid_t segid);

/**
 * @brief obtain permission to attach memory
 *
 * @param [in] segid                tarting address of region to share
 * @param [in] flags                number of bytes to share
 * @param [in] permit_type          only XPMEM_PERMIT_MODE currently defined
 * @param [in] permit_value         permissions mode expressed as an octal value
 * @return kupl_shm_xpmem_segid_t  make result, 64-bit segment ID for success, -1 for error
 */
kupl_shm_xpmem_apid_t kupl_shm_xpmem_get(kupl_shm_xpmem_segid_t segid,
    int flags, int permit_type, void *permit_value);

/**
 * @brief give up access to the segment
 *
 * @param [in] apid                 64-bit access permit ID to release
 * @return int                      release result, 0 for success, -1 for error
 */
int kupl_shm_xpmem_release(kupl_shm_xpmem_apid_t apid);

/**
 * @brief map a source address to own address space
 *
 * @param [in] addr                 a structure consisting of a kupl_shm_xpmem_apid_t apid and an off_t offset
 * @param [in] size                 number of bytes to map
 * @param [in] vaddr                address at which the mapping should be created, or NULL if the kernel should choose
 * @return void *                   attach result, virtual address at which the mapping
 *                                  was created for success, -1 for error
 */
void *kupl_shm_xpmem_attach(kupl_shm_xpmem_addr_t addr, size_t size, void *vaddr);

/**
 * @brief remove a mapping between consumer and source
 *
 * @param [in] vaddr                virtual address within an XPMEM mapping in the consumer's address space
 * @return int                      attach result, 0 for success, -1 for error
 */
int kupl_shm_xpmem_detach(void *vaddr);

#ifdef __cplusplus
}
#endif

#endif
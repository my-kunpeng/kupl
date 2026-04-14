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
#include "kupl_shm_xpmem_adapter.h"

int kupl_shm_xpmem_version()
{
    return xpmem_version();
}

kupl_shm_xpmem_segid_t kupl_shm_xpmem_make(void *vaddr, size_t size, int permit_type, void *permit_value)
{
    return xpmem_make(vaddr, size, permit_type, permit_value);
}

int kupl_shm_xpmem_remove(kupl_shm_xpmem_segid_t segid)
{
    return xpmem_remove(segid);
}

kupl_shm_xpmem_apid_t kupl_shm_xpmem_get(kupl_shm_xpmem_segid_t segid,
    int flags, int permit_type, void *permit_value)
{
    return xpmem_get(segid, flags, permit_type, permit_value);
}

int kupl_shm_xpmem_release(kupl_shm_xpmem_apid_t apid)
{
    return xpmem_release(apid);
}

void *kupl_shm_xpmem_attach(kupl_shm_xpmem_addr_t addr, size_t size, void *vaddr)
{
    struct xpmem_addr addr_temp;
    addr_temp.apid = addr.apid;
    addr_temp.offset = addr.offset;

    return xpmem_attach(addr_temp, size, vaddr);
}

int kupl_shm_xpmem_detach(void *vaddr)
{
    return xpmem_detach(vaddr);
}
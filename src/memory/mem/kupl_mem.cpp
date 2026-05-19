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

#include "kupl_mem.h"
#include <map>
#include <unordered_map>
#include "kupl.h"
#include "core/kupl_core.h"
#include "mt/kupl_queue.h"
#include "utils/sys/kupl_compiler.h"

using kupl_mem_t = struct {
    void *base_ddr;
    void *base_hbw;
    size_t size;
    int pin;
    KUPL_ATOMIC_INT ref_cnt; // for mem allocation
    int att_cnt;             // for is present
};

using kupl_mem_args_t = struct {
    kupl_queue_h queue;
    kupl_mem_t *mem;
    void *ddr;
    ptrdiff_t offset;
    size_t size;
    int flag;
    int ret;
};

static std::unordered_map<void *, kupl_mem_t *> *g_addr_map;
static std::map<void *, kupl_mem_t *> *g_range_map;
static kupl_lock_t *g_map_lock = nullptr;

int kupl_mem_init()
{
    g_addr_map = new (std::nothrow) std::unordered_map<void *, kupl_mem_t *>;
    if (kupl_unlikely(g_addr_map == nullptr)) {
        goto err;
    }
    g_range_map = new (std::nothrow) std::map<void *, kupl_mem_t *>;
    if (kupl_unlikely(g_range_map == nullptr)) {
        goto err;
    }
    g_map_lock = kupl_lock_create(PTHREAD_SPINLOCK);
    if (kupl_unlikely(g_map_lock == nullptr)) {
        goto err;
    }
    return KUPL_OK;
err:
    kupl_mem_fini();
    return KUPL_ERROR;
}

void kupl_mem_fini()
{
    if (g_addr_map != nullptr) {
        for (auto iter = g_addr_map->begin(); iter != g_addr_map->end(); iter++) {
            auto mem = iter->second;
            if (mem == nullptr) {
                continue;
            }
            if (mem->pin == KUPL_OK) {
                kupl_munlock(iter->first, mem->size);
            }
            kupl_hbw_free(const_cast<void *>(mem->base_hbw));
        }
        delete g_addr_map;
        g_addr_map = nullptr;
    }
    if (g_range_map != nullptr) {
        delete g_range_map;
        g_range_map = nullptr;
    }
    kupl_lock_cleanup(g_map_lock);
    g_map_lock = nullptr;
}

static kupl_mem_t *mem_query(void *ddr, ptrdiff_t &offset)
{
    kupl_mem_t *mem = nullptr;
    // find in addr_map
    g_map_lock->lock(g_map_lock);
    auto addr_iter = g_addr_map->find(ddr);
    if (addr_iter != g_addr_map->end()) {
        mem = addr_iter->second;
        if (kupl_likely(mem != nullptr)) {
            offset = 0;
            g_map_lock->unlock(g_map_lock);
            return mem;
        }
    }
    // find in range_map
    {
        auto iter = g_range_map->upper_bound(ddr);
        // should not be the first element
        if (iter != g_range_map->begin()) {
            --iter;
            mem = iter->second;
            // 1. mem availible
            // 2. ddr in mem range
            if ((char *)ddr > (char *)iter->first) {
                // diff > 0
                ptrdiff_t diff = ((char *)ddr - (char *)iter->first);
                if (static_cast<size_t>(diff) < mem->size) {
                    offset = diff;
                    g_map_lock->unlock(g_map_lock);
                    return mem;
                }
            }
            g_map_lock->unlock(g_map_lock);
            return nullptr;
        }
    }
    g_map_lock->unlock(g_map_lock);
    return nullptr;
}

bool kupl_mem_is_present(void *ddr)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return false;
    }
    if (kupl_unlikely(ddr == nullptr)) {
        kupl_error("kupl_mem_is_present invalid params ddr: %p", ddr);
        return false;
    }
    ptrdiff_t offset = 0;
    auto mem = mem_query(ddr, offset);
    if (kupl_unlikely(mem == nullptr)) {
        return false;
    } else {
        return mem->att_cnt > 0;
    }
}

void *kupl_mem_query(void *ddr)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return nullptr;
    }
    if (kupl_unlikely(ddr == nullptr)) {
        kupl_warn("kupl_mem_query invalid params ddr: %p", ddr);
        return nullptr;
    }
    ptrdiff_t offset = 0;
    auto mem = mem_query(ddr, offset);
    if (kupl_unlikely(mem == nullptr)) {
        return nullptr;
    } else {
        return (char *)mem->base_hbw + offset;
    }
}

static kupl_mem_t *mem_map(void *ddr, size_t size)
{
    kupl_mem_t *mem = new (std::nothrow) kupl_mem_t{ddr, nullptr, size, KUPL_ERROR, 0, 0};
    if (kupl_unlikely(mem == nullptr)) {
        return nullptr;
    }
    g_map_lock->lock(g_map_lock);
    (*g_addr_map)[ddr] = mem;
    (*g_range_map)[ddr] = mem;
    g_map_lock->unlock(g_map_lock);
    return mem;
}

static void mem_unmap(kupl_mem_t *mem)
{
    g_map_lock->lock(g_map_lock);
    g_addr_map->erase((void *)mem->base_ddr);
    g_range_map->erase((void *)mem->base_ddr);
    g_map_lock->unlock(g_map_lock);
    delete mem;
}

static int mem_alloc(kupl_mem_t *mem)
{
    if (mem->base_hbw != nullptr) {
        return KUPL_OK;
    }
    void *hbw = kupl_hbw_malloc(mem->size); // hbw_malloc will do mlock
    if (kupl_unlikely(hbw == nullptr)) {
        kupl_error("mem_alloc hbw failed.");
        return KUPL_ERROR;
    }
    int pin = KUPL_ERROR;
    if (mem->size >= (size_t)kupl_config_get_value(KUPL_SDMA_MEMCPY_THRESHOLD)) {
        pin = kupl_mlock(mem->base_ddr, mem->size);
    }
    mem->base_hbw = hbw;
    mem->pin = pin;
    mem->ref_cnt = 0;
    return KUPL_OK;
}

static void mem_free(kupl_mem_t *mem)
{
    if (mem->base_hbw == nullptr) {
        return;
    }
    if (mem->pin == KUPL_OK) {
        kupl_munlock(mem->base_ddr, mem->size);
        mem->pin = KUPL_ERROR;
    }
    kupl_hbw_free(const_cast<void *>(mem->base_hbw));
    mem->base_hbw = nullptr;
    mem->ref_cnt = 0;
}

static int mem_attach(kupl_mem_args_t *args)
{
    kupl_mem_t *mem = mem_query(args->ddr, args->offset);
    if (mem == nullptr) {
        mem = mem_map(args->ddr, args->size);
        if (kupl_unlikely(mem == nullptr)) {
            kupl_error("mem_attach map failed.");
            return KUPL_ERROR;
        }
    } else if (mem->att_cnt == 0) {
        // fix ddr address reuse
        // 1. force wait
        kupl_queue_wait_all();
        // 2. mem map
        mem = mem_map(args->ddr, args->size);
        if (kupl_unlikely(mem == nullptr)) {
            kupl_error("mem_attach map failed.");
            return KUPL_ERROR;
        }
        args->offset = 0;
    }

    args->mem = mem;
    switch (args->flag) {
        case KUPL_MEM_CREATE:
        case KUPL_MEM_IN:
            mem->att_cnt += 1;
            break;
        case KUPL_MEM_PUSH:
            break;
        default:
            kupl_error("mem_attach invalid flag.");
            return KUPL_ERROR;
    }
    return KUPL_OK;
}

static void copyin_task(void *args)
{
    auto mem_args = (kupl_mem_args_t *)args;
    auto mem = mem_args->mem;
    if (mem->base_hbw == nullptr) {
        if (kupl_unlikely(mem_alloc(mem) != KUPL_OK)) {
            mem_args->ret = KUPL_ERROR;
            kupl_error("copyin_task do mem_alloc fail.");
            return;
        }
    }
    void *hbw = (char *)mem->base_hbw + mem_args->offset;
    switch (mem_args->flag) {
        case KUPL_MEM_CREATE:
            mem->ref_cnt += 1;
            return;
        case KUPL_MEM_IN: {
            int ref_cnt = KUPL_ATOMIC_ADD(&mem->ref_cnt, 1);
            // mem->ref_cnt now 1, before add is 0
            if (ref_cnt == 0) {
                mem_args->ret = kupl_memcpy(hbw, mem_args->ddr, mem_args->size);
            }
            return;
        }
        case KUPL_MEM_PUSH: {
            mem_args->ret = kupl_memcpy(hbw, mem_args->ddr, mem_args->size);
            return;
        }
        default:
            mem_args->ret = KUPL_ERROR;
            return;
    }
}

int kupl_mem_copyin(void *ddr, size_t size, kupl_mem_copyin_flag_t flag, kupl_queue_h queue)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(ddr == nullptr || size == 0)) {
        kupl_warn("kupl_mem_copyin invalid params ddr: %p, size: %lu, queue: %p", ddr, size, queue);
        return KUPL_ERROR;
    }
    kupl_mem_args_t args = {
        .queue = queue,
        .mem = nullptr,
        .ddr = ddr,
        .offset = 0,
        .size = size,
        .flag = flag,
        .ret = KUPL_OK,
    };
    if (kupl_unlikely(mem_attach(&args) == KUPL_ERROR)) {
        kupl_error("mem_attach fail ddr: %p, size: %lu, queue: %p", ddr, size, queue);
        return KUPL_ERROR;
    }
    if (queue == nullptr || kupl_queue_is_sync(queue)) {
        copyin_task(&args);
        return args.ret;
    }
    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME | KUPL_QUEUE_ITEM_DESC_FIELD_ARGS_SIZE,
        .func = copyin_task,
        .args = &args,
        .name = "copyin_task",
        .args_size = sizeof(args),
    };
    return kupl_queue_submit(queue, &desc);
}

static int mem_detach(kupl_mem_args_t *args)
{
    kupl_mem_t *mem = mem_query(args->ddr, args->offset);
    if (kupl_unlikely(mem == nullptr)) {
        return KUPL_ERROR;
    }
    args->mem = mem;
    switch (args->flag) {
        case KUPL_MEM_DELETE:
        case KUPL_MEM_OUT:
            mem->att_cnt -= 1;
            break;
        case KUPL_MEM_PULL:
            break;
        case KUPL_MEM_DELETE_FINALIZE:
        case KUPL_MEM_OUT_FINALIZE:
            mem->att_cnt = 0;
            break;
        default:
            kupl_error("mem_detach invalid flag.");
            return KUPL_ERROR;
    }
    return KUPL_OK;
}

static void copyout_task(void *args)
{
    auto mem_args = (kupl_mem_args_t *)args;
    auto mem = mem_args->mem;
    if (kupl_unlikely(mem->base_hbw == nullptr)) {
        mem_args->ret = KUPL_ERROR;
        kupl_warn("copyout_task ddr: %p not found.", mem_args->ddr);
        return;
    }
    void *hbw = (char *)mem->base_hbw + mem_args->offset;
    int ref_cnt = -1;
    switch (mem_args->flag) {
        case KUPL_MEM_PULL: {
            mem_args->ret = kupl_memcpy(mem_args->ddr, hbw, mem_args->size);
            return;
        }
        case KUPL_MEM_DELETE:
            ref_cnt = KUPL_ATOMIC_ADD(&mem->ref_cnt, -1);
            break;
        case KUPL_MEM_OUT:
            ref_cnt = KUPL_ATOMIC_ADD(&mem->ref_cnt, -1);
            if (ref_cnt == 1) {
                mem_args->ret = kupl_memcpy(mem_args->ddr, hbw, mem_args->size);
            }
            break;
        case KUPL_MEM_OUT_FINALIZE: {
            KUPL_ATOMIC_ST(&mem->ref_cnt, 0);
            ref_cnt = 1;
            mem_args->ret = kupl_memcpy(mem_args->ddr, hbw, mem_args->size);
            break;
        }
        case KUPL_MEM_DELETE_FINALIZE:
            KUPL_ATOMIC_ST(&mem->ref_cnt, 0);
            ref_cnt = 1;
            break;
        default:
            mem_args->ret = KUPL_ERROR;
            return;
    }
    // dealloc
    if (ref_cnt == 1) {
        mem_free(mem);
        if (mem->att_cnt == 0) {
            mem_unmap(mem);
        }
    }
}

int kupl_mem_copyout(void *ddr, size_t size, kupl_mem_copyout_flag_t flag, kupl_queue_h queue)
{
    if (!g_core_inited && kupl_init() == KUPL_ERROR) {
        return KUPL_ERROR;
    }
    if (kupl_unlikely(ddr == nullptr)) {
        kupl_warn("kupl_mem_copyout invalid params ddr: %p, size: %lu, queue: %p", ddr, size, queue);
        return KUPL_ERROR;
    }
    kupl_mem_args_t args = {
        .queue = queue,
        .mem = nullptr,
        .ddr = ddr,
        .offset = 0,
        .size = size,
        .flag = flag,
        .ret = KUPL_OK,
    };
    if (kupl_unlikely(mem_detach(&args) == KUPL_ERROR)) {
        kupl_error("mem_detach fail ddr: %p, size: %lu, queue: %p", ddr, size, queue);
        return KUPL_ERROR;
    }
    if (queue == nullptr || kupl_queue_is_sync(queue)) {
        copyout_task(&args);
        return args.ret;
    }
    kupl_queue_item_desc_t desc = {
        .field_mask = KUPL_QUEUE_ITEM_DESC_FIELD_NAME | KUPL_QUEUE_ITEM_DESC_FIELD_ARGS_SIZE,
        .func = copyout_task,
        .args = &args,
        .name = "copyout_task",
        .args_size = sizeof(args),
    };
    return kupl_queue_submit(queue, &desc);
}
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
#include "kupl_sched_sspe.h"
#include "mt/scheduler/plugin/kupl_sched_plugin_api.h"
#include "executor/kupl_executor.h"
#include "executor/backend/kupl_executor_backend.h"
#include "memory/mpool/kupl_mpool.h"
#include "utils/arch/kupl_cache.h"
#include "mt/barrier/kupl_barrier.h"

#define KUPL_SLOT_ALIGN KUPL_ALIGN(128)

typedef struct executor_slot {
    kupl_taskbase_t *tb;
} KUPL_SLOT_ALIGN executor_slot_t;

typedef struct sched_data {
    executor_slot_t *slot;
    int slot_num;
} sched_data_t;

static int sched_init(kupl_sched_plugin_property_t *property)
{
    property->name = KUPL_SCHED_PLUGIN_SSPE_NAME;
    property->private_data_len = 0;
    property->score = KUPL_SCHED_PLUGIN_SSPE_SCORE;

    return KUPL_OK;
}

static void sched_fini() {}

static void sched_cleanup(void *_sched);

static void *sched_create()
{
    auto host_info = kupl_get_host_info();
    auto sched = static_cast<sched_data_t *>(kupl_calloc(1, sizeof(sched_data_t)));
    if (kupl_unlikely(sched == nullptr)) {
        return nullptr;
    }
    sched->slot = (executor_slot_t *)kupl_calloc((size_t)host_info->avail_pu_cnt, sizeof(executor_slot_t));
    if (sched->slot == nullptr) {
        sched_cleanup(sched);
        return nullptr;
    }
    sched->slot_num = host_info->avail_pu_cnt;
    return sched;
}

static void sched_cleanup(void *_sched)
{
    if (kupl_unlikely(_sched == nullptr)) {
        return;
    }
    sched_data_t *sched = (sched_data_t *)_sched;
    kupl_safe_free(sched->slot);
    kupl_safe_free(sched);
}

static int sched_add_tb(void *_sched, kupl_taskbase_t *tb)
{
    sched_data_t *sched = (sched_data_t *)_sched;
    auto eid = tb->executor_id;
    if (kupl_unlikely(eid <= KUPL_TB_EXECUTOR_ID_DEFAULT || eid >= sched->slot_num)) {
        return kupl_log_error_return(ERROR, "invalid executor id %d", eid);
    }
    sched->slot[eid].tb = tb;
    return KUPL_OK;
}

static kupl_taskbase_t *sched_get_tb(void *_sched, kupl_compute_place_t cp)
{
    (void)cp;
    sched_data_t *sched = (sched_data_t *)_sched;
    static thread_local int own_eid = kupl_get_executor_num();
    if (kupl_unlikely(own_eid == KUPL_EIDCID_INIT)) {
        kupl_error("invoke KUPL functions on threads not managed by KUPL");
        return nullptr;
    }

    static kupl::FlagBarrier &barrier = kupl::FlagBarrier::getInstance();
    if (barrier.arrive(own_eid)) {
        return sched->slot[own_eid].tb;
    }
    return nullptr;
}

static const kupl_sched_plugin_api_t KUPL_SCHED_PLUGIN_GLOBAL_VAR(sspe) = {
    .name = KUPL_SCHED_PLUGIN_SSPE_NAME,
    .init = sched_init,
    .fini = sched_fini,
    .create = sched_create,
    .expand = nullptr,
    .cleanup = sched_cleanup,
    .add_tb = sched_add_tb,
    .get_tb = sched_get_tb,
};

const kupl_sched_plugin_api_t *kupl_sched_sspe_get_instance()
{
    return &KUPL_SCHED_PLUGIN_GLOBAL_VAR(sspe);
}
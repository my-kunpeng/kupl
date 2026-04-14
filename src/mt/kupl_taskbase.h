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
#ifndef KUPL_TASKBASE_IMPL_H
#define KUPL_TASKBASE_IMPL_H

#include "kupl.h"
#include "executor/backend/kupl_executor_backend.h"
#include "utils/arch/kupl_atomic.h"
#include "utils/config/kupl_config.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/sys/kupl_hardware.h"

#ifdef __cplusplus
extern "C" {
#endif

enum kupl_tb_state_t {
    KUPL_TB_STATE_CREATED      = kupl_bit(0), // taskbase created
    KUPL_TB_STATE_INIT         = kupl_bit(1), // taskbase inited
    KUPL_TB_STATE_READY        = kupl_bit(2), // taskbase ready to execute
    KUPL_TB_STATE_EXECUTED     = kupl_bit(3), // taskbase executed
    KUPL_TB_STATE_FINISHED     = kupl_bit(4)  // taskbase finished
};

enum kupl_tb_type_t {
    KUPL_TB_TYPE_UNKNOW        = 0,

    KUPL_TB_TYPE_ULT           = kupl_bit(0),
    KUPL_TB_TYPE_TASK          = kupl_bit(1)
};

#define KUPL_TB_CORE_ID_DEFAULT        (-1)
#define KUPL_TB_EXECUTOR_ID_DEFAULT    (-1)
#define KUPL_TB_EXECUTOR_ID_PLACE      (-2)

typedef struct kupl_tb_ops kupl_tb_ops_t;

typedef void (*kupl_tb_func_t)(void *args);

typedef struct kupl_notify_args {
    int numeid;
    int *tb2eid;
} kupl_notify_args_t;

typedef struct kupl_taskbase {
    kupl_tb_type_t          type;               /**< taskbase type @ref kupl_tb_type_t */

    char                    *name;
    uint64_t                id;
    union {
        void                *void_graph;
        kupl_graph_h        graph;          /**< which graph this tb is belong */
        kupl_sgraph_h       sgraph;         /**< which static graph this tb is belong */
    };
    kupl_egroup_h           egroup;             /**< the executor group */

    // user desc
    kupl_tb_func_t          func;               /**< the CPU side tb routine */
    void                    *args;              /**< the args */
    int                     priority;           /**< the priority */
    int                     core_id;            /**< the affinity core user set */
    int                     executor_id;        /**< the executor id */
    uint64_t                flag;               /**< the flag @KUPL_TB_FLAG_XXX */
    const kupl_tb_ops_t     *ops;
    kupl_compute_place_t    cp;                 /**< the affinity core tb alloc */
    KUPL_ATOMIC_INT         ref;
    KUPL_ATOMIC_UINT32      state;
    KUPL_ATOMIC_UINT32      *count;             /**< group count of graph or sgraph */
} kupl_taskbase_t;

typedef void (*kupl_tb_ref_t)(kupl_taskbase_t *tb);

typedef void (*kupl_tb_deref_t)(kupl_taskbase_t *tb, int geid);

typedef int (*kupl_tb_invoke_t)(kupl_taskbase_t *tb);

struct kupl_tb_ops {
    kupl_tb_ref_t    ref;
    kupl_tb_deref_t  deref;
    kupl_tb_invoke_t invoke;
};

/** @brief the flag for tb */
typedef enum kupl_tb_flag {
    /**
     * Notify tb will be execute immediately
     */
    KUPL_TB_FLAG_IMM           = kupl_bit(0),
    /**
     * Notify tb is priority tb
     */
    KUPL_TB_FLAG_PRIORITY      = kupl_bit(1),
    /**
     * Notify tb will not be destroyed by rouine
     */
    KUPL_TB_FLAG_REUSE         = kupl_bit(2),
    /**
     * Notify tb will be traced
     */
    KUPL_TB_FLAG_TRACE         = kupl_bit(3),
    /**
     * Notify tb will not be stolen in the sched
     */
    KUPL_TB_FLAG_SCHED_STATIC  = kupl_bit(4),
    /**
     * Notify tb whether belonging to the outer layer of hybrid
     */
    KUPL_TB_FLAG_HYBRID_OUTER  = kupl_bit(5),
    /**
     * Notify tb whether belonging to the inner layer of hybrid
     */
    KUPL_TB_FLAG_HYBRID_INNER  = kupl_bit(6),
    /**
     * Notify tb in the reuse shced can be got again
     */
    KUPL_TB_FLAG_NOTIFY_REUSE  = kupl_bit(7),
} kupl_tb_flag_t;

/** @brief The @ref kupl_tb_desc_t-struct field mask */
enum kupl_tb_desc_field {
    KUPL_TB_DESC_FIELD_NAME        = kupl_bit(0),     /**< valid kupl_tb_desc.name member */
    KUPL_TB_DESC_FIELD_PRIORITY    = kupl_bit(1),     /**< valid kupl_tb_desc.priority member */
    KUPL_TB_DESC_FIELD_FLAG        = kupl_bit(2),     /**< valid kupl_tb_desc.flag member */
    KUPL_TB_DESC_FIELD_EGROUP      = kupl_bit(3),     /**< valid kupl_tb_desc.executor_id member */
    KUPL_TB_DESC_FIELD_EXECUTOR_ID = kupl_bit(4),     /**< valid kupl_tb_desc.executor_id member */
};

typedef void (*kupl_tb_func_t)(void *args);

/** @brief the description of kupl ult or task */
typedef struct kupl_tb_desc {
    uint64_t            field_mask;     /**< Mask fields of kupl_tb_desc, @ref kupl_tb_desc_field */
    kupl_tb_func_t      func;           /**< the tb func */
    void                *args;          /**< the arguments of func */
    const char          *name;          /**< the tb name */
    int                 priority;       /**< the priority of this tb, the bigger number will get a higher priority */
    uint32_t            flag;           /**< the flag of this tb */
    int                 executor_id;    /**< the executor's id for this tb to bind */
    kupl_egroup_h       egroup;
} kupl_tb_desc_t;

typedef struct kupl_tb_param {
    kupl_tb_type_t          type;
    kupl_tb_desc_t          *user_desc;
    union {
        void                *void_graph;
        kupl_graph_h        graph;          /**< which graph this tb is belong */
        kupl_sgraph_h       sgraph;         /**< which static graph this tb is belong */
    };
    KUPL_ATOMIC_UINT32      *count;
} kupl_tb_param_t;

static kupl_always_inline
void kupl_tb_init(kupl_taskbase_t *tb, kupl_tb_param_t *param, int geid)
{
    tb->type = param->type;
    tb->void_graph = param->void_graph;
    tb->core_id = KUPL_TB_CORE_ID_DEFAULT;
    tb->cp = kupl_get_compute_place(kupl_global_eid2cid(geid));
    tb->count = param->count;

    auto user_desc = param->user_desc;
    tb->func = user_desc->func;
    tb->args = user_desc->args;
    tb->executor_id = (user_desc->field_mask & KUPL_TB_DESC_FIELD_EXECUTOR_ID)
                      ? user_desc->executor_id : KUPL_TB_EXECUTOR_ID_DEFAULT;
    tb->flag = (user_desc->field_mask & KUPL_TB_DESC_FIELD_FLAG) ? user_desc->flag : 0;
    tb->name = (user_desc->field_mask & KUPL_TB_DESC_FIELD_NAME) ? strdup(user_desc->name) : nullptr;
    tb->egroup = (user_desc->field_mask & KUPL_TB_DESC_FIELD_EGROUP) ? user_desc->egroup : nullptr;
    if (user_desc->field_mask & KUPL_TB_DESC_FIELD_PRIORITY) {
        tb->flag |= KUPL_TB_FLAG_PRIORITY;
        tb->priority = user_desc->priority;
    }
    #ifdef ENABLE_KUPL_TRACE
    static int trace_tb = kupl_config_get_value(KUPL_TRACE_TASKBASE);
    if (trace_tb) {
        tb->flag |= KUPL_TB_FLAG_TRACE;
    }
    #endif
}

#define kupl_tb_is_finished(_tb) ((KUPL_ATOMIC_LD_ACQ(&((_tb)->state)) & KUPL_TB_STATE_FINISHED) != 0)

#define kupl_tb_count_add(_tb)                                  \
do {                                                            \
    if ((_tb)->count != nullptr) {                              \
        KUPL_ATOMIC_ADD_RLS((_tb)->count, 1);                   \
    }                                                           \
} while (0)

#define kupl_tb_count_sub(_tb)                                  \
do {                                                            \
    if ((_tb)->count != nullptr) {                              \
        KUPL_ATOMIC_SUB_RLS((_tb)->count, 1u);                  \
    }                                                           \
} while (0)

static kupl_always_inline
int kupl_tb_test(kupl_taskbase_t *tb)
{
    if (kupl_unlikely(tb == nullptr)) {
        return KUPL_ERROR;
    }

    if (kupl_tb_is_finished(tb)) {
        return KUPL_OK;
    }

    return KUPL_AGAIN;
}

#ifdef __cplusplus
}
#endif

#endif
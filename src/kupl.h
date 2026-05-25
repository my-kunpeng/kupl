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
#ifndef KUPL_H
#define KUPL_H

#include <stdio.h>
#include <stdint.h>
#include <sched.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief status in kupl library */
#define KUPL_OK 0
#define KUPL_ERROR (-1)

/** @brief exposed kupl symbol table */
#define kupl_export __attribute__((visibility("default")))

/** @brief the handle of kupl task */
typedef struct kupl_task *kupl_task_h;
/** @brief the handle of kupl graph */
typedef struct kupl_graph *kupl_graph_h;
/** @brief the handle of kupl sgraph (short of static graph) */
typedef struct kupl_sgraph *kupl_sgraph_h;
/** @brief the handle of kupl egroup (short of executor group) */
typedef struct kupl_egroup *kupl_egroup_h;
/** @brief the handle of kupl static graph node */
typedef struct kupl_sgraph_node *kupl_sgraph_node_h;
/** @brief the handle of kupl queue */
typedef struct kupl_queue *kupl_queue_h;
/** @brief the handle of kupl event */
typedef struct kupl_event *kupl_event_h;

/** @brief kupl version info */
typedef struct kupl_version {
    const char *product_name;
    const char *product_version;
    const char *component_name;
    const char *component_abi_version;
    const char *component_version;
    const char *component_appendinfo;
} kupl_version_t;

/**
 * @brief get the kupl version info
 * @param [out] version             the kupl version info
 *
 * @return  KUPL_OK for get version info success, other for failed.
 */
kupl_export int kupl_get_version(kupl_version_t *version);

/**
 * @brief get the wall clock time
 *
 * @return the wall clock time in seconds
 */
kupl_export double kupl_get_wtime(void);

#define KUPL_MAX_DIM_SIZE 3
#define KUPL_CONCURRENCY_DEFAULT (-1)
#define KUPL_BLOCKSIZE_DEFAULT 0
#define KUPL_ASYNC_SYNC (-1)

typedef struct kupl_range {
    int64_t lower;     /**< Pointer to loop lower bound in ult structure */
    int64_t upper;     /**< Pointer to loop upper bound in ult structure */
    int64_t step;      /**< the stride of this range, for normal range step = 1 */
    int64_t blocksize; /**< the blocksize of this range, default blocksize = 1 */
} kupl_range_t;

typedef struct kupl_nd_range {
    int dim; /**< the dimension of this range,
                                        the max dimension depends on KUPL_MAX_DIM_SIZE */
    kupl_range_t nd_range[KUPL_MAX_DIM_SIZE];
} kupl_nd_range_t;

/**
 * @brief MACRO to init ranges/stride ranges
 */
#define KUPL_1D_RANGE_INIT(_range, _col_begin, _col_end)         \
    do {                                                         \
        (_range).dim = 1;                                        \
        (_range).nd_range[0].lower = _col_begin;                 \
        (_range).nd_range[0].upper = _col_end;                   \
        (_range).nd_range[0].step = 1;                           \
        (_range).nd_range[0].blocksize = KUPL_BLOCKSIZE_DEFAULT; \
    } while (0)

#define KUPL_STRIDE_1D_RANGE_INIT(_range, _col_begin, _col_end, _col_step, _col_blocksize) \
    do {                                                                                   \
        (_range).dim = 1;                                                                  \
        (_range).nd_range[0].lower = _col_begin;                                           \
        (_range).nd_range[0].upper = _col_end;                                             \
        (_range).nd_range[0].step = _col_step;                                             \
        (_range).nd_range[0].blocksize = _col_blocksize;                                   \
    } while (0)

#define KUPL_2D_RANGE_INIT(_range, _row_begin, _row_end, _col_begin, _col_end) \
    do {                                                                       \
        (_range).dim = 2;                                                      \
        (_range).nd_range[1].lower = _row_begin;                               \
        (_range).nd_range[1].upper = _row_end;                                 \
        (_range).nd_range[1].step = 1;                                         \
        (_range).nd_range[1].blocksize = KUPL_BLOCKSIZE_DEFAULT;               \
        (_range).nd_range[0].lower = _col_begin;                               \
        (_range).nd_range[0].upper = _col_end;                                 \
        (_range).nd_range[0].step = 1;                                         \
        (_range).nd_range[0].blocksize = KUPL_BLOCKSIZE_DEFAULT;               \
    } while (0)

#define KUPL_STRIDE_2D_RANGE_INIT(_range, _row_begin, _row_end, _row_step, _row_blocksize, _col_begin, _col_end, \
                                  _col_step, _col_blocksize)                                                     \
    do {                                                                                                         \
        (_range).dim = 2;                                                                                        \
        (_range).nd_range[1].lower = _row_begin;                                                                 \
        (_range).nd_range[1].upper = _row_end;                                                                   \
        (_range).nd_range[1].step = _row_step;                                                                   \
        (_range).nd_range[1].blocksize = _row_blocksize;                                                         \
        (_range).nd_range[0].lower = _col_begin;                                                                 \
        (_range).nd_range[0].upper = _col_end;                                                                   \
        (_range).nd_range[0].step = _col_step;                                                                   \
        (_range).nd_range[0].blocksize = _col_blocksize;                                                         \
    } while (0)

#define KUPL_3D_RANGE_INIT(_range, _page_begin, _page_end, _row_begin, _row_end, _col_begin, _col_end) \
    do {                                                                                               \
        (_range).dim = 3;                                                                              \
        (_range).nd_range[2].lower = _page_begin;                                                      \
        (_range).nd_range[2].upper = _page_end;                                                        \
        (_range).nd_range[2].step = 1;                                                                 \
        (_range).nd_range[2].blocksize = KUPL_BLOCKSIZE_DEFAULT;                                       \
        (_range).nd_range[1].lower = _row_begin;                                                       \
        (_range).nd_range[1].upper = _row_end;                                                         \
        (_range).nd_range[1].step = 1;                                                                 \
        (_range).nd_range[1].blocksize = KUPL_BLOCKSIZE_DEFAULT;                                       \
        (_range).nd_range[0].lower = _col_begin;                                                       \
        (_range).nd_range[0].upper = _col_end;                                                         \
        (_range).nd_range[0].step = 1;                                                                 \
        (_range).nd_range[0].blocksize = KUPL_BLOCKSIZE_DEFAULT;                                       \
    } while (0)

#define KUPL_STRIDE_3D_RANGE_INIT(_range, _page_begin, _page_end, _page_step, _page_blocksize, _row_begin, _row_end, \
                                  _row_step, _row_blocksize, _col_begin, _col_end, _col_step, _col_blocksize)        \
    do {                                                                                                             \
        (_range).dim = 3;                                                                                            \
        (_range).nd_range[2].lower = _page_begin;                                                                    \
        (_range).nd_range[2].upper = _page_end;                                                                      \
        (_range).nd_range[2].step = _page_step;                                                                      \
        (_range).nd_range[2].blocksize = _page_blocksize;                                                            \
        (_range).nd_range[1].lower = _row_begin;                                                                     \
        (_range).nd_range[1].upper = _row_end;                                                                       \
        (_range).nd_range[1].step = _row_step;                                                                       \
        (_range).nd_range[1].blocksize = _row_blocksize;                                                             \
        (_range).nd_range[0].lower = _col_begin;                                                                     \
        (_range).nd_range[0].upper = _col_end;                                                                       \
        (_range).nd_range[0].step = _col_step;                                                                       \
        (_range).nd_range[0].blocksize = _col_blocksize;                                                             \
    } while (0)

#define KUPL_ALL_EXECUTORS nullptr

typedef enum kupl_datatype {
    KUPL_DATATYPE_INT,
    KUPL_DATATYPE_FLOAT,
    KUPL_DATATYPE_DOUBLE,
    KUPL_DATATYPE_FLOAT_COMPLEX,
    KUPL_DATATYPE_DOUBLE_COMPLEX,
} kupl_datatype_t;

typedef enum kupl_reduce_op {
    KUPL_RD_ADD,
    KUPL_RD_SUB,
    KUPL_RD_MAX,
    KUPL_RD_MIN,
} kupl_reduce_op_t;

typedef struct kupl_reduce_item {
    void *buffer;
    kupl_datatype_t type;
    kupl_reduce_op_t op;
} kupl_reduce_item_t;

typedef struct kupl_reduce_args {
    int num;
    kupl_reduce_item_t *items;
} kupl_reduce_args_t;

/**
 * @brief create a Graph
 *
 * @param [in]  egroup      the executor group this graph use,
 *                          it can be KUPL_ALL_EXECUTORS for use all availble executors
 *
 * @return                  the kupl graph handle, return NULL for failed
 */
kupl_export kupl_graph_h kupl_graph_create(kupl_egroup_h egroup);

/**
 * @brief destroy a Graph
 *
 * @param [in] graph        the graph
 */
kupl_export void kupl_graph_destroy(kupl_graph_h graph);

/**
 * @brief wait until all the ults and tasks in the graph are finished
 *
 * @param [in] graph        the graph waiting for
 */
kupl_export void kupl_graph_wait(kupl_graph_h graph);

/** @brief the flag for task */
enum kupl_task_flag {
    /**
     * Notify task will be execute immediately
     */
    KUPL_TASK_FLAG_IMM = 1uL << 0,
};

/** @brief The @ref kupl_task_desc_t-struct field mask */
enum kupl_task_desc_field {
    KUPL_TASK_DESC_FIELD_NAME = 1uL << 0,     /**< enable kupl_task_desc.name member */
    KUPL_TASK_DESC_FIELD_PRIORITY = 1uL << 1, /**< enable kupl_task_desc.priority member */
    KUPL_TASK_DESC_FIELD_FLAG = 1uL << 2,     /**< enable kupl_task_desc.flag member */
    KUPL_TASK_DESC_FIELD_DEP = 1uL << 3,      /**< enable kupl_task_desc.dep_list & ndep member */
};

typedef void (*kupl_task_func_t)(void *args);

typedef enum kupl_task_dep_type {
    KUPL_TASK_DEP_TYPE_IN = 0x1,    /**< IN for read depend */
    KUPL_TASK_DEP_TYPE_OUT = 0x2,   /**< OUT for write depend */
    KUPL_TASK_DEP_TYPE_INOUT = 0x3, /**< INOUT for read and write depend */
    KUPL_TASK_DEP_TYPE_ALL = 0x80,  /**< depend ALL task before */
} kupl_task_dep_type_t;

typedef struct kupl_task_dep {
    const void *base_addr;     /**< the address of this depend */
    kupl_task_dep_type_t type; /**< the type of this depend */
} kupl_task_dep_t;

/** @brief the description of kupl task */
typedef struct kupl_task_desc {
    uint64_t field_mask;       /**< the field mask of kupl_task_desc, @ref kupl_task_desc_field */
    kupl_task_func_t func;     /**< the task func */
    void *args;                /**< the arguments of func */
    const char *name;          /**< the task name */
    int priority;              /**< the task priority, the bigger number will get a higher priority */
    size_t ndep;               /**< the number of depend */
    kupl_task_dep_t *dep_list; /**< the list of depend */
    uint32_t flag;             /**< the task flag, @ref enum kupl_task_flag */
} kupl_task_desc_t;

/** @brief the flag for sgraph_task */
enum kupl_sgraph_task_flag {
    /**
     * Notify sgraph_task will be execute immediately
     */
    KUPL_SGRAPH_TASK_FLAG_IMM = 1uL << 0,
};

/** @brief The @ref kupl_sgraph_task_desc_t-struct field mask */
enum kupl_sgraph_task_desc_field {
    KUPL_SGRAPH_TASK_DESC_FIELD_NAME = 1uL << 0,     /**< enable kupl_sgraph_task_desc.name member */
    KUPL_SGRAPH_TASK_DESC_FIELD_PRIORITY = 1uL << 1, /**< enable kupl_sgraph_task_desc.priority member */
    KUPL_SGRAPH_TASK_DESC_FIELD_FLAG = 1uL << 2,     /**< enable kupl_sgraph_task_desc.flag member */
};

typedef struct kupl_sgraph_task_desc {
    uint64_t field_mask;  /**< the field mask of kupl_sgraph_task_desc,
                                                 @ref kupl_sgraph_task_desc_field */
    kupl_sgraph_h sgraph; /**< The static graph which will be executed. */
    const char *name;     /**< the sgraph task name */
    int priority;         /**< the task priority, the bigger number will get a higher priority */
    uint32_t flag;        /**< the task flag, @ref enum kupl_sgraph_task_flag */
} kupl_sgraph_task_desc_t;

typedef void (*kupl_taskloop_func_t)(kupl_nd_range_t *nd_range, void *args);

enum kupl_taskloop_desc_field {
    KUPL_TASKLOOP_DESC_FIELD_RANGE = 1uL << 0,  /**< enable kupl_taskloop_desc.range member */
    KUPL_TASKLOOP_DESC_FIELD_EGROUP = 1uL << 1, /**< enable kupl_taskloop_desc.egroup member */
    KUPL_TASKLOOP_DESC_FIELD_DEFAULT = KUPL_TASKLOOP_DESC_FIELD_RANGE | KUPL_TASKLOOP_DESC_FIELD_EGROUP,
};

typedef struct kupl_taskloop_desc {
    uint64_t field_mask;       /**< the field mask of kupl_taskloop_desc, @ref kupl_taskloop_desc_field */
    kupl_taskloop_func_t func; /**< the taskloop func */
    void *args;                /**< the arguments of func */
    kupl_nd_range_t *range;    /**< the taskloop range */
    kupl_egroup_h egroup;      /**< the taskloop egroup */
} kupl_taskloop_desc_t;

typedef enum kupl_task_type {
    KUPL_TASK_TYPE_SINGLE,
    KUPL_TASK_TYPE_SGRAPH,
    KUPL_TASK_TYPE_TASKLOOP,
} kupl_task_type_t;

/** @brief the information of kupl task */
typedef struct kupl_task_info {
    kupl_task_type_t type;
    void *desc;
} kupl_task_info_t;

/**
 * @brief add a task to graph
 *
 * @param [in] graph        the graph to which to add the task
 * @param [in] task_info    the information of the task that will be added
 *
 * @return                  KUPL_OK for success
 */
kupl_export int kupl_graph_submit(kupl_graph_h graph, kupl_task_info_t *info);

/** @brief the flag for sgraph node */
enum kupl_sgraph_node_flag {
    /**
     * Notify sgraph node will be execute immediately
     */
    KUPL_SGRAPH_NODE_FLAG_IMM = 1uL << 0,
};

/** @brief The @ref kupl_sgraph_node_desc_t-struct field mask */
enum kupl_sgraph_node_desc_field {
    KUPL_SGRAPH_NODE_DESC_FIELD_NAME = 1uL << 0,     /**< enable kupl_sgraph_node_desc.name member */
    KUPL_SGRAPH_NODE_DESC_FIELD_PRIORITY = 1uL << 1, /**< enable kupl_sgraph_node_desc.priority member */
    KUPL_SGRAPH_NODE_DESC_FIELD_FLAG = 1uL << 2,     /**< enable kupl_sgraph_node_desc.flag member */
    KUPL_SGRAPH_NODE_DESC_FIELD_EGROUP = 1uL << 3,   /**< enable kupl_sgraph_node_desc.egroup member */
};

typedef void (*kupl_sgraph_node_func_t)(void *args);

/** @brief the description of kupl sgraph node */
typedef struct kupl_sgraph_node_desc {
    uint64_t field_mask;          /**< Mask fields of kupl_sgraph_node_desc, @ref kupl_sgraph_node_desc_field */
    kupl_sgraph_node_func_t func; /**< the static graph node func */
    void *args;                   /**< the arguments of func */
    const char *name;             /**< the node name */
    int priority;                 /**< the priority of this node, the bigger number will get a higher priority */
    uint32_t flag;                /**< the flag of this node, @ref enum kupl_sgraph_node_flag */
    kupl_egroup_h egroup;
} kupl_sgraph_node_desc_t;

/**
 * @brief create a static graph
 *
 * @return                  the kupl static graph handle, return NULL for failed
 */
kupl_export kupl_sgraph_h kupl_sgraph_create(void);

/**
 * @brief create a static graph
 *
 * @param [in] sgraph       the static graph to which to add the node
 * @param [in] desc         the description of the node that will be added
 *
 * @return                  KUPL_OK for success
 */
kupl_export kupl_sgraph_node_h kupl_sgraph_add_node(kupl_sgraph_h sgraph, kupl_sgraph_node_desc_t *desc);

/**
 * @brief add a dependency between two nodes of a specified static graph
 *
 * @param [in] sgraph       the static graph to which to set
 * @param [in] precede      the precede node of the dependency
 * @param [in] succeed      the succeed node of the dependency
 *
 * @return                  KUPL_OK for success
 */
kupl_export int kupl_sgraph_add_dep(kupl_sgraph_node_h precede, kupl_sgraph_node_h succeed);

/**
 * @brief destroy a static graph
 *
 * @param [in] sgraph       the static graph
 */
kupl_export void kupl_sgraph_destroy(kupl_sgraph_h sgraph);

/**
 * @brief Get kupl executor num, used for kupl_egroup_create
 *
 * @return int              the kupl executor num
 */
kupl_export int kupl_get_num_executors(void);

/**
 * @brief Get the current kupl executor id, when invoker thread don't bind to kupl, this will return -1 always.
 *
 * @return int              the kupl executor id, or -1 for thread dont't bind to kupl
 */
kupl_export int kupl_get_executor_num(void);

/**
 * @brief create executor egroup
 *
 * @param [in] executors            the executor's index array start from 0
 * @param [in] executors_num        the number of executors
 * @return kupl_egroup_h   the egroup created
 */
kupl_export kupl_egroup_h kupl_egroup_create(int *executors, int executors_num);

/**
 * @brief destroy executor egroup
 *
 * @param [in] egroup       the egroup to be destroyed
 */
kupl_export void kupl_egroup_destroy(kupl_egroup_h egroup);

/**
 * @brief dest egroup borrow resource from src egroup, src egroup will lock
 *
 * @param [in] dest         dest egroup
 * @param [in] src          src egroup
 * @return int              dest egroup size, KUPL_ERROR for fail
 */
kupl_export int kupl_egroup_borrow(kupl_egroup_h dest, kupl_egroup_h src);

/**
 * @brief src egroup return resource to dest egroup, dest egroup will lock
 *
 * @param [in] dest         dest egroup
 * @param [in] src          src egroup
 * @return int              dest egroup size, KUPL_ERROR for fail
 */
kupl_export int kupl_egroup_return(kupl_egroup_h dest, kupl_egroup_h src);

/**
 * @brief egroup barrier
 *
 * @param [in] egroup       the egroup contain which executors to block
 */
kupl_export void kupl_egroup_barrier(kupl_egroup_h egroup);

/**
 * @brief egroup fork barrier
 *
 * @param [in] egroup       the egroup contain which executors to block
 */
kupl_export void kupl_egroup_fork_barrier(kupl_egroup_h egroup);

/**
 * @brief egroup join barrier
 *
 * @param [in] egroup       the egroup contain which executors to block
 */
kupl_export void kupl_egroup_join_barrier(kupl_egroup_h egroup);

/**
 * @brief Resets the egroup to its initial state
 *
 * @param [in] egroup       the egroup to reset
 */
kupl_export void kupl_egroup_reset(kupl_egroup_h egroup);

typedef enum kupl_loop_policy_type {
    KUPL_LOOP_POLICY_STATIC,  /**< policy that means
                                    the elements in each segment could be executed in parallel,
                                    the ult spilted in static */
    KUPL_LOOP_POLICY_DYNAMIC, /**< policy that means
                                    the elements in each segment could be executed in parallel,
                                    the ult spilted in dynamic */
    KUPL_LOOP_POLICY_TASK     /**< policy that means
                                    the elements in each segment could be executed in parallel,
                                    elements splited will be submiited as tasks */
} kupl_loop_policy_type_t;

/**
 * @brief The parallel for task routine
 *
 * @param [in] nd_range     the range of this routine
 * @param [in] args         the args of this routine
 * @param [in] tid          local thread id of this routine
 * @param [in] tnum         local thread num of this routine
 */
typedef void (*kupl_pf_func_t)(kupl_nd_range_t *nd_range, void *args, int tid, int tnum);

enum kupl_parallel_for_desc_field {
    KUPL_PARALLEL_FOR_DESC_FIELD_RANGE = 1uL << 0,       /**< enable kupl_parallel_for_desc.range member */
    KUPL_PARALLEL_FOR_DESC_FIELD_EGROUP = 1uL << 1,      /**< enable kupl_parallel_for_desc.egroup member */
    KUPL_PARALLEL_FOR_DESC_FIELD_CONCURRENCY = 1uL << 2, /**< enable kupl_parallel_for_desc.concurrency member */
    KUPL_PARALLEL_FOR_DESC_FIELD_POLICY = 1uL << 3,      /**< enable kupl_parallel_for_desc.policy member */
    KUPL_PARALLEL_FOR_DESC_FIELD_DEFAULT = KUPL_PARALLEL_FOR_DESC_FIELD_RANGE | KUPL_PARALLEL_FOR_DESC_FIELD_EGROUP |
                                           KUPL_PARALLEL_FOR_DESC_FIELD_CONCURRENCY |
                                           KUPL_PARALLEL_FOR_DESC_FIELD_POLICY,
};

typedef struct kupl_parallel_for_desc {
    uint64_t field_mask;            /**< @ref kupl_parallel_for_desc_field */
    kupl_nd_range_t *range;         /**< range for this parallel_for */
    kupl_egroup_h egroup;           /**< egroup for this parallel_for */
    int concurrency;                /**< concurrency for this parallel_for */
    kupl_loop_policy_type_t policy; /**< loop policy this parallel_for will use */
} kupl_parallel_for_desc_t;

/**
 * @brief create a parallel for structure based on the parallel for description.
 *
 * @param [in] desc            the description of this parallel for
 * @param [in] func            the task routine for this parallel for
 * @param [in] args            the args for this parallel for func
 *
 * @return  KUPL_OK for success, other for failed.
 */
kupl_export int kupl_parallel_for(kupl_parallel_for_desc_t *desc, kupl_pf_func_t func, void *args);

/**
 * @brief The parallel for reduce task routine
 *
 * @param [in] nd_range         the range of this routine
 * @param [in] args             the args of this routine
 * @param [in] tid              local thread id of this routine
 * @param [in] tnum             local thread num of this routine
 * @param [in] rd_args          the args for reduce
 */
typedef void (*kupl_pf_reduce_func_t)(kupl_nd_range_t *nd_range, void *args, int tid, int tnum,
                                      kupl_reduce_args_t *rd_args);

/**
 * @brief create a parallel for reduce structure based on the parallel for description and reduce args.
 *
 * @param [in] desc            the description of this parallel for
 * @param [in] func            the task routine for this parallel for
 * @param [in] args            the args for this parallel for reduce func
 * @param [in] rd_args         the args for reduce
 *
 * @return  KUPL_OK for success, other for failed.
 */
kupl_export int kupl_parallel_for_reduce(kupl_parallel_for_desc_t *desc, kupl_pf_reduce_func_t func, void *args,
                                         kupl_reduce_args_t *rd_args);

/**
 * @brief check if in parallel
 *
 * @return true if in parallel, false when not
 */
kupl_export bool kupl_in_parallel(void);

/**
 * @brief set the number of threads used by kupl kernel，such as memcpy
 *
 * @param [in] num  the number of threads
 */
kupl_export void kupl_set_kernel_concurrency(int num);

/**
 * @brief get the number of threads used by kupl kernel，such as memcpy
 *
 * @return the number of threads used now
 */
kupl_export int kupl_get_kernel_concurrency(void);

/**
 * @brief set the number of threads used by current thread，such as memcpy
 *        specifies the concurrency of the current thread.
 * @param [in] num  the number of threads
 */
kupl_export void kupl_set_kernel_concurrency_local(int num);

/**
 * @brief get the number of threads used by current thread，such as memcpy
 *        get the concurrency of the current thread.
 * @return              the number of threads , 0 for use all threads
 */
kupl_export int kupl_get_kernel_concurrency_local(void);

typedef enum kupl_mem_kind {
    KUPL_MEM_DEFAULT,
    KUPL_MEM_LARGE_CAP,
    KUPL_MEM_HIGH_BW
} kupl_mem_kind_t;

/**
 * @brief Allocates size bytes of Page-locked memory and
 *        returns a void * pointer to the start of that memory
 *
 * @param [in] kind         the memory spaces or allocation policy of the allocated memory
 * @param [in] size         the size of the allocated memory
 *
 * @return the ptr of the memory
 */
kupl_export void *kupl_malloc(kupl_mem_kind_t kind, size_t size);

/**
 * @brief Deallocates memory
 *
 * @param [in] kind         the memory spaces or allocation policy of the allocated memory
 * @param [in] ptr          free the Page-locked memory ptr
 */
kupl_export void kupl_free(kupl_mem_kind_t kind, void *ptr);

/**
 * @brief Allocates size bytes of Page-locked memory in high bandwidth memory
 *        and returns a void * pointer to the start of that memory
 *
 * @param [in] size         the size of the allocated memory
 *
 * @return the ptr of the memory
 */
kupl_export void *kupl_hbw_malloc(size_t size);

/**
 * @brief Deallocates memory in high bandiwidth memory
 *
 * @param [in] ptr          free the Page-locked memory ptr
 */
kupl_export void kupl_hbw_free(void *ptr);

/*
 * Flags for kupl_hbw_verify function
 */
typedef enum kupl_hbw_verify_flag {
    KUPL_HBW_TOUCH_PAGES = (1 << 0)
} kupl_hbw_verify_flag_t;

typedef enum kupl_hbw_verify_returns {
    KUPL_HBW_VERIFY_ERROR = -1,
    KUPL_IS_NOT_HBW_MEMORY = 0,
    KUPL_IS_HBW_MEMORY = 1
} kupl_hbw_verify_returns_t;

/**
 * @brief Verifies if allocated memory fully fall into high bandwidth memory.
 *
 * @param [in] addr            the begin address of the memory that needs to be verified
 * @param [in] size            the size of the memory that needs to be verified
 * @param [in] flags           the flag for this verification
 *
 * @return  1 for address range from "addr" to "addr" + "size" is allocated in high bandwidth memory
 * 0 for any region of memory was not allocated in high bandwidth memory
 * -1 for verification failure
 */
kupl_export int kupl_hbw_verify(void *addr, size_t size, int flags);

/**
 * @brief test if high bandwidth is available
 *
 * @return  0 for high bandwidth is unavailable, 1 for available
 */
kupl_export int kupl_hbw_check_available(void);

typedef enum kupl_hbw_policy {
    KUPL_HBW_POLICY_BIND = 0 /* currently only support BIND */
} kupl_hbw_policy_t;

/**
 * @brief get current global high bandwidth memory allocation policy
 *
 * @return  current allocation policy
 */
kupl_export kupl_hbw_policy_t kupl_hbw_get_policy(void);

/**
 * @brief set the global high bandwidth memory allocation policy
 *
 * @param [in] policy  the policy needs to be set
 *
 * @return  KUPL_OK for success
 */
kupl_export int kupl_hbw_set_policy(kupl_hbw_policy_t policy);

typedef enum kupl_mem_copyin_flag {
    KUPL_MEM_CREATE,
    KUPL_MEM_IN,
    KUPL_MEM_PUSH,
} kupl_mem_copyin_flag_t;

/**
 * @brief Copy the ddr memory to hbw.
 *
 * @param [in] ddr_addr     the ddr address of memory
 * @param [in] size         the amount of ddr memory
 * @param [in] flag         copyin operation flag @ref kupl_mem_copyin_flag_t
 * @param [in] queue        queue to submit async item
 *
 * @return  KUPL_OK for success
 */
kupl_export int kupl_mem_copyin(void *ddr_addr, size_t size, kupl_mem_copyin_flag_t flag, kupl_queue_h queue);

typedef enum kupl_mem_copyout_flag {
    KUPL_MEM_DELETE,
    KUPL_MEM_OUT,
    KUPL_MEM_PULL,
    KUPL_MEM_DELETE_FINALIZE,
    KUPL_MEM_OUT_FINALIZE,
} kupl_mem_copyout_flag_t;

/**
 * @brief Copy the hbw memory to ddr.
 *
 * @param [in] ddr_addr     the ddr address of memory
 * @param [in] size         the amount of hbw memory
 * @param [in] flag         copyout operation flag @ref kupl_mem_copyout_flag_t
 * @param [in] queue        queue to submit async item
 *
 * @return  KUPL_OK for success
 */
kupl_export int kupl_mem_copyout(void *ddr_addr, size_t size, kupl_mem_copyout_flag_t flag, kupl_queue_h queue);

/**
 * @brief Query the hbw address correspond to ddr.
 *
 * @param [in] ddr_addr     the ddr address of memory
 *
 * @return  the hbw address of memory
 */
kupl_export void *kupl_mem_query(void *ddr_addr);

/**
 * @brief Test whether a variable is accessible form the hbw.
 *
 * @param [in] ddr_addr     the ddr address of memory
 *
 * @return  the accessiblity of the variable on hbw
 */
kupl_export bool kupl_mem_is_present(void *ddr_addr);

/**
 * @brief pin the page table of the buffer
 *
 * @param [in] buffer     the buffer need to pin
 * @param [in] count      the size of the buffer
 *
 * @return                KUPL_OK for success
 */
kupl_export int kupl_mlock(void *buffer, size_t count);

/**
 * @brief unpin the page table of the buffer
 *
 * @param [in] buffer     the buffer need to unpin
 * @param [in] count      the size of the buffer
 *
 * @return                KUPL_OK for success
 */
kupl_export int kupl_munlock(void *buffer, size_t count);

/**
 * @brief copy n bytes from memory area src to memory area dst
 *
 * @param [in] dst      the destination area ptr
 * @param [in] src      the source area ptr
 * @param [in] count    the size of the copy memory
 *
 * @return              KUPL_OK for success
 */
kupl_export int kupl_memcpy(void *dst, const void *src, size_t count);

/**
 * @brief copy a 2d buff from memory area src to memory area dst
 *
 * @param [in] dst      the destination area ptr
 * @param [in] dpitch   pitch of destination memory
 * @param [in] src      the source area ptr
 * @param [in] spitch   pitch of source memory
 * @param [in] width    width of matrix transfer (columns in bytes)
 * @param [in] height   height of matrix transfer (rows)
 *
 * @return              KUPL_OK for success
 */
kupl_export int kupl_memcpy2d(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height);

/**
 * @brief copy n bytes from memory area src to memory area dst asynchronously
 *
 * @param [in] dst      the destination area ptr
 * @param [in] src      the source area ptr
 * @param [in] count    the size of the copy memory
 * @param [in] queue    the queue to submit the memcpy event
 * @param [out] event   the memcpy event handle which is used for synchronize
 *
 * @return              KUPL_OK for success
 */
kupl_export int kupl_memcpy_async(void *dst, const void *src, size_t count, kupl_queue_h queue, kupl_event_h event);

/**
 * @brief copy a 2d buff from memory area src to memory area dst asynchronously
 *
 * @param [in] dst      the destination area ptr
 * @param [in] dpitch   pitch of destination memory
 * @param [in] src      the source area ptr
 * @param [in] spitch   pitch of source memory
 * @param [in] width    width of matrix transfer (columns in bytes)
 * @param [in] height   height of matrix transfer (rows)
 * @param [in] queue    the queue to submit the memcpy event
 * @param [out] event   the memcpy event handle which is used for synchronize
 *
 * @return              KUPL_OK for success
 */
kupl_export int kupl_memcpy2d_async(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width,
                                    size_t height, kupl_queue_h queue, kupl_event_h event);

/**
 * @brief   Create queue
 *
 * @return              the handler of queue
 */
kupl_export kupl_queue_h kupl_queue_create(void);

/**
 * @brief   Get the range of queue priority
 *
 * @param [out] least_priority      the least priority
 * @param [out] greatest_priority   the greatest priority
 *
 * @return              KUPL_OK for success
 */
kupl_export int kupl_get_queue_priority_range(int *least_priority, int *greatest_priority);

/**
 * @brief   Create queue with priority
 *
 * @param [in] priority     the priority of queue
 *
 * @return                  the handler of queue
 */
kupl_export kupl_queue_h kupl_queue_create_with_priority(int priority);

/**
 * @brief   Destroy queue
 *
 * @param [in]          the handler of queue
 */
kupl_export void kupl_queue_destroy(kupl_queue_h queue);

/**
 * @brief   Wait events in queue finish
 *
 * @param [in]          the handler of queue
 * @return int          return KUPL_OK for success
 */
kupl_export int kupl_queue_wait(kupl_queue_h queue);

/**
 * @brief Make the specified compute queue wait for an event.
 *
 * @param [in] queue    Queue to make wait
 * @param [in] event    Event to wait on
 * @return int      return KUPL_OK for success
 */
kupl_export int kupl_queue_wait_event(kupl_queue_h queue, kupl_event_h event);

typedef void (*kupl_queue_item_func_t)(void *args);

/** @brief The @ref kupl_queue_item_desc_t-struct field mask */
enum kupl_queue_item_desc_field {
    KUPL_QUEUE_ITEM_DESC_FIELD_NAME = 1uL << 0,      /**< enable queue_item_desc.name member */
    KUPL_QUEUE_ITEM_DESC_FIELD_EGROUP = 1uL << 1,    /**< enable queue_item_desc.egroup member */
    KUPL_QUEUE_ITEM_DESC_FIELD_ARGS_SIZE = 1uL << 2, /**< enable queue_item_desc.args_size member */
};

/** @brief the description of kupl kernel */
typedef struct kupl_queue_item_desc {
    uint64_t field_mask;         /**< Mask fields of kupl_queue_item_desc, @ref kupl_queue_item_desc_field */
    kupl_queue_item_func_t func; /**< the queue item func */
    void *args;                  /**< the arguments of func */
    const char *name;            /**< the kernel name */
    kupl_egroup_h egroup;
    size_t args_size; /**< the arguments size */
} kupl_queue_item_desc_t;

/**
 * @brief   Queue submit kernel
 *
 * @param [in] queue    The queue to execute the kernel
 * @param [in] desc     The description of the kernel that will be submited
 * @return int          Return KUPL_OK for success
 */
kupl_export int kupl_queue_submit(kupl_queue_h queue, kupl_queue_item_desc_t *desc);

/**
 * @brief   Create queue with index
 *
 * @param [in] index    The index of queue to create
 * @return kupl_queue_h         Return the handler of queue
 */
kupl_export kupl_queue_h kupl_queue_acquire(int index);

/**
 * @brief   Wait all queues created by kupl_queue_acquire
 *
 * @return int          return KUPL_OK for success
 */
kupl_export int kupl_queue_wait_all();

/** @brief the status for event */
typedef enum kupl_event_status {
    KUPL_EVENT_STATUS_CREATED,
    KUPL_EVENT_STATUS_SUBMITTED,
    KUPL_EVENT_STATUS_COMPLETE,
} kupl_event_status_t;

/**
 * @brief   Create event
 *
 * @return  the handler of event
 */
kupl_export kupl_event_h kupl_event_create(void);

/**
 * @brief   Destroy event
 *
 * @param [in] event    the handler of event
 */
kupl_export void kupl_event_destroy(kupl_event_h event);

/**
 * @brief   Record an event in the specified queue.
 *
 * @param [in] event    event to record.
 * @param [in] queue    queue used to record events.
 * @return int      return KUPL_OK for success
 */
kupl_export int kupl_event_record(kupl_event_h event, kupl_queue_h queue);

/**
 * @brief   Wait for an event to complete.
 *
 * @param [in] event    Event on which to wait.
 * @return int      return KUPL_OK for success
 */
kupl_export int kupl_event_wait(kupl_event_h event);

/**
 * @brief   Query event status.
 *
 * @param [in] event    Event to query.
 * @return int      return event status, @ref kupl_event_status_t. return KUPL_ERROR for invalid event.
 */
kupl_export int kupl_event_query(kupl_event_h event);

typedef struct kupl_shm_win *kupl_shm_win_h;
typedef struct kupl_shm_comm *kupl_shm_comm_h;
typedef struct kupl_shm_request *kupl_shm_request_h;

typedef enum kupl_shm_reduce_op {
    KUPL_SHM_REDUCE_OP_MAX = 0,
    KUPL_SHM_REDUCE_OP_MIN,
    KUPL_SHM_REDUCE_OP_SUM
} kupl_shm_reduce_op_t;

typedef enum kupl_shm_datatype {
    KUPL_SHM_DATATYPE_CHAR = 0,
    KUPL_SHM_DATATYPE_INT,
    KUPL_SHM_DATATYPE_LONG,
    KUPL_SHM_DATATYPE_FLOAT,
    KUPL_SHM_DATATYPE_DOUBLE
} kupl_shm_datatype_t;

typedef enum kupl_info_flag {
    KUPL_SHM_INFO_IS_CONTIG
} kupl_info_flag_t;

typedef int (*kupl_shm_oob_allgather_cb_t)(const void *sendbuf, void *recvbuf, int size, void *group,
                                           kupl_shm_datatype_t datatype);

typedef int (*kupl_shm_oob_barrier_cb_t)(void *group);

typedef struct kupl_shm_oob_cb {
    kupl_shm_oob_allgather_cb_t oob_allgather;
    kupl_shm_oob_barrier_cb_t oob_barrier;
} kupl_shm_oob_cb_t, *kupl_shm_oob_cb_h;

/**
 * @brief implement the function of allocating memory
 *
 * @param [in] size             the size of allocating memory
 * @param [in] comm             the kupl comm
 * @param [out] baseptr         the base address for allocating memory
 * @param [out] win             the win of allocating memory
 * @return int                  alloc result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_win_alloc(size_t size, kupl_shm_comm_h comm, void **baseptr, kupl_shm_win_h *win);

/**
 * @brief free the comm win
 *
 * @param [in] win              the win to free
 * @return int                  free result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_win_free(kupl_shm_win_h win);

/**
 * @brief get the virtual address of the target process
 *
 * @param [in] win              the win to query in
 * @param [in] remote_rank      the rank to query
 * @param [out] baseptr         the address of the target process
 * @return int                  query result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_win_query(kupl_shm_win_h win, int remote_rank, void **baseptr);

typedef struct kupl_shm_addr {
    void *src_addr; /**< the src virtual addr of another process */
    int src_pid;    /**< the src pid of another process */
    int dst_pid;    /**< the pid of local process */
} kupl_shm_addr_t;

/**
 * @brief alloc and attach memory to src_addr in desc
 *
 * @param [in] addr             The description of the src memory info that will be attached
 * @param [in] size             the memory size to attach
 * @return  the dst ptr of the memory that attached to src addr
 */

kupl_export void *kupl_shm_attach(kupl_shm_addr_t addr, size_t size);

/**
 * @brief detach and free memory that attached to src_addr
 *
 * @param [in] ptr              The dst ptr of the memory that attached to src addr
 */

kupl_export void kupl_shm_detach(void *ptr);

/**
 * @brief implement inter-process barrier in win
 *
 * @param [in] win              the win to barrier in
 * @return int                  fence result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_fence(kupl_shm_win_h win);

/**
 * @brief implement a barrier with the specified rank in win
 *
 * @param [in] win              the win to barrier in
 * @param [in] remote_rank      the target rank to barrier
 * @return int                  fence result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_peer_fence(kupl_shm_win_h win, int remote_rank);

/**
 * @brief create kupl communication domain through MPI communication domain
 *
 * @param [in] size              the MPI comm size
 * @param [in] rank              the MPI comm rank
 * @param [in] pid               the MPI comm pid
 * @param [in] oob_allgather     the MPI allgather func pointer
 * @param [in] group             the MPI comm group
 * @param [out] comm             the created kupl comm
 * @return int                   create result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_comm_create(int size, int rank, int pid, kupl_shm_oob_cb_h oob_cbs, void *group,
                                     kupl_shm_comm_h *comm);

/**
 * @brief get kupl comm process number
 *
 * @param [in] comm              the kupl comm
 * @param [out] rank             the kupl process number
 * @return int                   get result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_comm_rank(kupl_shm_comm_h comm, int *rank);

/**
 * @brief get the size of the kupl communication domain
 *
 * @param [in] comm              the kupl comm
 * @param [out] size             the kupl comm size
 * @return int                   get result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_comm_size(kupl_shm_comm_h comm, int *size);

/**
 * @brief execute the collection operation stored in the request
 *
 * @param [in] request           the request to start
 * @return int                   start result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_request_start(kupl_shm_request_h request);

/**
 * @brief check the request status until it is completed, which is a blocking operation
 *
 * @param [in] request           the request to wait
 * @return int                   wait result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_request_wait(kupl_shm_request_h request);

/**
 * @brief free the request
 *
 * @param [in] request           the request to free
 * @return int                   wait result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_request_free(kupl_shm_request_h request);

/**
 * @brief destroy kupl communication domain
 *
 * @param [in] comm              the kupl comm to destroy
 * @return int                   destory result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_comm_destroy(kupl_shm_comm_h comm);

/**
 * @brief implement inter-process allreduce within comm
 *
 * @param [in] sendbuf           the send buffer
 * @param [out] recvbuf          the receive buffer
 * @param [in] count             the send count
 * @param [in] datatype          data type of send data
 * @param [in] op                the op handle indicating the global reduction operation to perform
 * @param [in] comm              the kupl communicator handle
 * @param [out] request          the kupl communicator request
 * @return int                   allreduce result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_allreduce_init(const void *sendbuf, void *recvbuf, int count, kupl_shm_datatype_t datatype,
                                        kupl_shm_reduce_op_t op, kupl_shm_comm_h comm, kupl_shm_request_h *request);

/**
 * @brief implement inter-process broadcast within comm
 *
 * @param [in,out] buffer        the broadcast buffer
 * @param [in] count             the send count
 * @param [in] datatype          data type of send data
 * @param [in] root              the rank of the process that is sending the data
 * @param [in] comm              the kupl communicator handle
 * @param [out] request          the kupl communicator request
 * @return int                   bcast result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_bcast_init(void *buffer, int count, kupl_shm_datatype_t datatype, int root,
                                    kupl_shm_comm_h comm, kupl_shm_request_h *request);

/**
 * @brief implement inter-process alltoall within comm
 *
 * @param [in] sendbuf            the pointer to the data to be sent to all processes in the group
 * @param [in] sendcount          the number of data elements that this process sends in the buffer
 * @param [in] sendtype           the data type of the elements in the send buffer
 * @param [out] recvbuf           the pointer to a buffer that contains the data that are received from each process
 * @param [in] recvcount          the number of data elements from each communicator process in the receive buffer
 * @param [in] recvtype           the data type of each element in the receive buffer
 * @param [in] comm               the kupl communicator handle
 * @param [out] request           the kupl communicator request
 * @return int                    alltoall result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_alltoall_init(const void *sendbuf, int sendcount, kupl_shm_datatype_t sendtype, void *recvbuf,
                                       int recvcount, kupl_shm_datatype_t recvtype, kupl_shm_comm_h comm,
                                       kupl_shm_request_h *request);

/**
 * @brief set kupl shm parameters
 *
 * @param [in] info_flag          parameter type
 * @param [in] value              parameter value
 * @return int                    set parameter result, 0 for success, -1 for error
 */
kupl_export int kupl_shm_info_set(kupl_info_flag_t info_flag, uint32_t value);

/** @brief The @ref kupl_queue_kernel_desc_t-struct field mask */
enum kupl_queue_kernel_desc_field {
    KUPL_QUEUE_KERNEL_DESC_FIELD_NAME = 1uL << 0, /**< enable queue_kernel_desc.name member */
};

typedef struct kupl_queue_kernel_desc {
    uint64_t field_mask;
    kupl_nd_range_t *range;
    kupl_egroup_h egroup;
    const char *name;
} kupl_queue_kernel_desc_t;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <functional>

namespace kupl {
kupl_export int graph_submit(kupl_graph_h graph, kupl_task_desc_t *desc, const std::function<void(void)> &func);

kupl_export int graph_submit(kupl_graph_h graph, kupl_taskloop_desc_t *desc,
                             const std::function<void(const kupl_nd_range_t *)> &func);

kupl_export kupl_sgraph_node_h sgraph_add_node(kupl_sgraph_h sgraph, kupl_sgraph_node_desc_t *desc,
                                               const std::function<void(void)> &func);

kupl_export int queue_submit(kupl_queue_h queue, kupl_queue_kernel_desc_t *desc,
                             const std::function<void(const kupl_nd_range_t *)> &kernel);

kupl_export int queue_submit(kupl_queue_h queue, kupl_queue_item_desc_t *desc, const std::function<void(void)> &func);

using pf_lambda = std::function<void(const kupl_nd_range_t *nd_range, const int tid, const int tnum)>;

kupl_export int parallel_for(kupl_parallel_for_desc_t *desc, const pf_lambda &func);
} // namespace kupl

#endif

#endif

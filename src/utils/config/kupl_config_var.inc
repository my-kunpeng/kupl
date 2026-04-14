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

/* Check all macro is define before include this file */
#ifndef KUPL_CONFIG_INT
#error KUPL_CONFIG_INT is not define
#endif

#ifndef KUPL_CONFIG_STR
#error KUPL_CONFIG_STR is not define
#endif

/**
 * @brief KUPL_CONFIG_INT(env_name, default_value, lower_boundary, upper_boundary, doc)
 *        define a int type configure
 *
 * @param env_name          the environment name of this configure
 * @param default_value     the default value of this configure
 * @param lower_boundary    the lower boundary of this configure
 * @param upper_boundary    the upper boundary of this configure
 * @param doc               the document of this configure
 *
 * @note                    the valid range of value is [lower_boundary, upper_boundary]
 */
KUPL_CONFIG_INT(KUPL_ENABLE_VERBOSE, 0, 0, 1,
                "print the configure")
KUPL_CONFIG_INT(KUPL_ENABLE_HARDWARE_VERBOSE, 0, 0, 1,
                "print the hardward topology infomation")
KUPL_CONFIG_INT(KUPL_LOG_LEVEL, 2, 0, 4,
                "the min log level (0:debug 1:info 2:warn 3:error 4:fatal)")
KUPL_CONFIG_INT(KUPL_MAX_GCID, 1024, 16, 10240,
                "the max gcid and is the max threads number the runtime can use")
KUPL_CONFIG_INT(KUPL_ENABLE_PRIORITY, 0, 0, 1,
                "enable the sched priority")
KUPL_CONFIG_INT(KUPL_SCHED_QUEUE_LENGTH, 512, 64, 102400,
                "the max ult numbers in ready queue")
KUPL_CONFIG_INT(KUPL_EXECUTOR_COUNT, 1024, 1, 1024,
                "the executor thread's count (the valid max count is the CPU core count).")
KUPL_CONFIG_INT(KUPL_ENABLE_YIELD, 1, 0, 1,
                "enable the executor thread yield while it cannot get ult for multiple times")
KUPL_CONFIG_INT(KUPL_YIELD_COUNT, 2147483647, 1, 2147483647,
                "the number of times not getting ult will the thread be yielded")
KUPL_CONFIG_INT(KUPL_TRACE_TASKBASE, 1, 0, 1,
                "trace taskbase")
KUPL_CONFIG_INT(KUPL_MEMCPY_MT_THRESHOLD, 524288, 0, 2147483647,
                "the threshold size of memcpy and memcpy2d when to use mutil threads memcpy, default is 512KB")
KUPL_CONFIG_INT(KUPL_SDMA_MEMCPY_THRESHOLD, 524288, 0, 2147483647,
                "the threshold size of memcpy when to use sdma memcpy, default is 2M")
KUPL_CONFIG_INT(KUPL_CORES_PER_SDMA, 152, 1, 200,
                "the number of the cores for each SDMA device")
KUPL_CONFIG_INT(KUPL_KERNEL_CONCURRENCY, 1, 1, 1024,
                "the number of threads used by memcpy")
KUPL_CONFIG_INT(KUPL_BCAST_TOPO_SPLIT, 16, 1, 512,
                "the split number of bcast algorithm topo_aware_linear")
KUPL_CONFIG_INT(KUPL_PTRACE_THREAD_BUFFER_SIZE, 3355443, 1, 10000000,
                "the event number of the single thread profile trace")
KUPL_CONFIG_INT(KUPL_ENABLE_HUGEPAGES, 0, 0, 1,
                "enable for hugepages management in mpool")
KUPL_CONFIG_INT(KUPL_MPOOL_ALIGN_SIZE, 8, 2, 2048,
                "the alignment for memory allocation in mpool")

/**
 * @brief KUPL_CONFIG_STR(env_name, default_str_value, doc)
 *        define a string type configure
 *
 * @param env_name          the environment name of this configure
 * @param default_value     the default string(const char *) value of this configure
 * @param doc               the document of this configure
 *
 * @note                    an empty string means there are no valid configure
 */
KUPL_CONFIG_STR(KUPL_SCHED_POLICY,
                "", "select the scheduler policy")
KUPL_CONFIG_STR(KUPL_EXECUTOR_WAIT_POLICY,
                "active", "the executor wait policy, now support active, passive")
KUPL_CONFIG_STR(KUPL_EXECUTOR_BACKEND,
                "omp", "the executor backend, now support omp, pthread")
KUPL_CONFIG_STR(KUPL_SHM_TYPE,
                "posix", "the shm type, now support posix, xpmem, sls")
KUPL_CONFIG_STR(KUPL_SCHED_MQ_PLACEQ_AFFINITY,
                "0-1023", "scheduler mq place queue affinity")
KUPL_CONFIG_STR(KUPL_BARRIER_ALGORITHM,
                "", "select the thread barrier algorithm")
KUPL_CONFIG_STR(KUPL_SHM_FENCE_ALGORITHM,
                "", "select the process barrier algorithm")
KUPL_CONFIG_STR(KUPL_SCHED_HYBRID_POLICY,
                "", "select the scheduler plugin for hybrid plugin")
KUPL_CONFIG_STR(KUPL_SHM_BCAST_ALGORITHM,
                "", "select the shm bcast algorithm")
KUPL_CONFIG_STR(KUPL_SHM_ALLREDUCE_ALGORITHM,
                "", "select the shm allreduce algorithm")
KUPL_CONFIG_STR(KUPL_SHM_ALLTOALL_ALGORITHM,
                "", "select the shm alltoall algorithm")
KUPL_CONFIG_STR(KUPL_PTRACE_PATH,
                "./ptrace/", "kupl profile trace file save path")
KUPL_CONFIG_STR(KUPL_PROF_LEVEL,
                "", "kupl profile trace enable flag")
KUPL_CONFIG_STR(KUPL_SHM_ON_PACKAGE,
                "n", "kupl shm win alloc use on_package memory")
KUPL_CONFIG_STR(KUPL_SHM_ENABLE_HUGEPAGE,
                "n", "kupl shm win alloc use hugepage")
KUPL_CONFIG_STR(KUPL_ENABLE_MPOOL,
                "n", "kupl enable inner mpool malloc/free")
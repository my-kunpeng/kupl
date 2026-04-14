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
#include "kupl_hardware.h"
#include <pthread.h>
#include <unistd.h>
#include <string>
#include "utils/config/kupl_config.h"
#include "utils/sys/kupl_glibc_version.h"
#ifdef KUPL_USE_HWLOC
#include <hwloc.h>
#include "kupl.h"
#include "kupl_compiler.h"
#include "utils/arch/kupl_atomic.h"
#include "utils/config/kupl_config.h"
#include "utils/debug/kupl_assert.h"
#include "utils/debug/kupl_log.h"

static kupl_host_info_t g_host_info = {
    .init = false,
};

kupl_compute_place_t *g_compute_place;

#define KUPL_CPU_NUMA_INIT (-1)
#define HWLOC_NUMA_DISTANCE_BASE 10
#define HWLOC_VERSION_DIVIDE_BY_NUMA 0x00010b00
#define HWLOC_VERSION_DIVIDE_BY_NUMA_DIS 0x00020000

#if HWLOC_API_VERSION < HWLOC_VERSION_DIVIDE_BY_NUMA
#define HWLOC_NUMA_ALIAS HWLOC_OBJ_NODE
#else
#define HWLOC_NUMA_ALIAS HWLOC_OBJ_NUMANODE
#endif

#ifndef HWLOC_OBJ_PACKAGE
#define HWLOC_OBJ_PACKAGE HWLOC_OBJ_SOCKET
#endif

static int kupl_host_info_init_topo_count(hwloc_topology_t *topology)
{
    if (kupl_unlikely(hwloc_topology_init(topology) < 0)) {
        return KUPL_ERROR;
    }

    if (kupl_unlikely(hwloc_topology_load(*topology)) < 0) {
        goto topology_destory;
    }

    g_host_info.socket_cnt = hwloc_get_nbobjs_by_type(*topology, HWLOC_OBJ_PACKAGE);
    if (kupl_unlikely(g_host_info.socket_cnt <= 0)) {
        goto topology_destory;
    }

    g_host_info.numa_cnt = hwloc_get_nbobjs_by_type(*topology, HWLOC_NUMA_ALIAS);
    if (kupl_unlikely(g_host_info.numa_cnt <= 0)) {
        goto topology_destory;
    }

    g_host_info.core_cnt = hwloc_get_nbobjs_by_type(*topology, HWLOC_OBJ_CORE);
    if (kupl_unlikely(g_host_info.core_cnt <= 0)) {
        goto topology_destory;
    }

    g_host_info.pu_cnt = hwloc_get_nbobjs_by_type(*topology, HWLOC_OBJ_PU);
    if (kupl_unlikely(g_host_info.pu_cnt <= 0)) {
        goto topology_destory;
    }

    g_host_info.numas = (kupl_numa_node_t *)kupl_calloc(g_host_info.numa_cnt, sizeof(kupl_numa_node_t));
    if (kupl_unlikely(g_host_info.numas == nullptr)) {
        goto topology_destory;
    }

    g_host_info.cpus = (kupl_compute_place_t *)kupl_calloc(g_host_info.pu_cnt, sizeof(kupl_compute_place_t));
    if (kupl_unlikely(g_host_info.cpus == nullptr)) {
        goto numas_free;
    }

    g_host_info.numa_distance = (uint64_t *)kupl_calloc(g_host_info.numa_cnt * g_host_info.numa_cnt,
                                                        sizeof(uint64_t));
    if (kupl_unlikely(g_host_info.numa_distance == nullptr)) {
        goto cpus_free;
    }

    return KUPL_OK;

cpus_free:
    kupl_safe_free(g_host_info.cpus);
numas_free:
    kupl_safe_free(g_host_info.numas);
topology_destory:
    hwloc_topology_destroy(*topology);
    kupl_log_error_return(ERROR, "kupl host info init by hwloc failed");
}

static kupl_always_inline
void kupl_init_every_process(hwloc_topology_t topology)
{
    for (int i = 0; i < g_host_info.pu_cnt; ++i) {
        auto obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PU, i);
        kupl_assert(obj != nullptr);

#if HWLOC_API_VERSION >= HWLOC_VERSION_DIVIDE_BY_NUMA_DIS
        hwloc_obj_t numa = nullptr;
        auto ancestor = obj->parent;
        while (ancestor != nullptr && ancestor->memory_arity == 0) {
            ancestor = ancestor->parent;
        }

        kupl_assert(ancestor != nullptr);
        for (size_t mem_child = 0; mem_child < ancestor->memory_arity; mem_child++) {
            if (mem_child == 0) {
                numa = ancestor->memory_first_child;
            } else {
                kupl_assert(numa != nullptr);
                numa = numa->next_sibling;
            }

            // Check that the object is actually a NUMA node
            // Check that the NUMA node is local to the PU
            if (hwloc_obj_type_is_memory(numa->type) && hwloc_bitmap_isset(obj->nodeset, numa->os_index)) {
                break;
            }
        }
#else
        auto numa = hwloc_get_ancestor_obj_by_type(topology, HWLOC_NUMA_ALIAS, obj);
#endif
        size_t numa_id = (numa == nullptr) ? 0 : numa->logical_index;
        size_t cpu_id = (g_host_info.core_cnt * obj->sibling_rank) + obj->parent->logical_index;
        if (cpu_id >= 0 && cpu_id < g_host_info.pu_cnt) {
            g_host_info.cpus[cpu_id].numa_id = numa_id;
            g_host_info.cpus[cpu_id].cpu_id = cpu_id;
        }
    }
}

static kupl_always_inline
void kupl_init_numa_distance(hwloc_topology_t topology)
{
    if (g_host_info.numa_cnt > 1) {
    /* get numa distance */
#if HWLOC_API_VERSION >= HWLOC_VERSION_DIVIDE_BY_NUMA_DIS
        hwloc_distances_s *distances;
        unsigned nr = 1;
        hwloc_distances_get(topology, &nr, &distances, HWLOC_DISTANCES_KIND_FROM_OS, 0);
        kupl_assert(distances->nbobjs == (unsigned)g_host_info.numa_cnt);
        for (int i = 0; i < g_host_info.numa_cnt; ++i) {
            auto numa_i = distances->objs[i];
            for (int j = 0; j < g_host_info.numa_cnt; ++j) {
                auto numa_j = distances->objs[j];
                hwloc_uint64_t di2j = 0;
                hwloc_uint64_t dj2i = 0;
                hwloc_distances_obj_pair_values(distances, numa_i, numa_j, &di2j, &dj2i);
                g_host_info.numa_distance[i * g_host_info.numa_cnt + j] = di2j;
                g_host_info.numa_distance[j * g_host_info.numa_cnt + i] = dj2i;
            }
        }
        hwloc_distances_release(topology, distances);
#else
        for (int i = 0; i < g_host_info.numa_cnt; ++i) {
            auto numa_i = hwloc_get_obj_by_type(topology, HWLOC_NUMA_ALIAS, i);
            kupl_assert(numa_i != nullptr && numa_i->type == HWLOC_NUMA_ALIAS);

            for (int j = i; j < g_host_info.numa_cnt; ++j) {
                auto numa_j = hwloc_get_obj_by_type(topology, HWLOC_NUMA_ALIAS, i);
                kupl_assert(numa_j != nullptr && numa_j->type == HWLOC_NUMA_ALIAS);
                float di2j;
                float dj2i;
                hwloc_get_latency(topology, numa_i, numa_j, &di2j, &dj2i);
                g_host_info.numa_distance[i * g_host_info.numa_cnt + j] = di2j * HWLOC_NUMA_DISTANCE_BASE;
                g_host_info.numa_distance[j * g_host_info.numa_cnt + i] = dj2i * HWLOC_NUMA_DISTANCE_BASE;
            }
        }
#endif
    }
}

int kupl_host_info_init()
{
    if (g_host_info.init == true) {
        return KUPL_OK;
    }

    hwloc_topology_t topology;

    /* init all hardware count and memory allocate */
    if (kupl_unlikely(kupl_host_info_init_topo_count(&topology) != KUPL_OK)) {
        kupl_host_info_fini();
        return KUPL_ERROR;
    }

    g_compute_place = (kupl_compute_place_t*)kupl_malloc_inner(sizeof(kupl_compute_place_t) *
                                                           g_host_info.pu_cnt);
    if (kupl_unlikely(g_compute_place == nullptr)) {
        kupl_host_info_fini();
        return KUPL_ERROR;
    }
    for (int i = 0; i < g_host_info.pu_cnt; ++i) {
        g_compute_place[i].cpu_id = KUPL_CPU_NUMA_INIT;
        g_compute_place[i].numa_id = KUPL_CPU_NUMA_INIT;
    }

    kupl_init_every_process(topology);

    kupl_init_numa_distance(topology);

    hwloc_topology_destroy(topology);
    g_host_info.init = true;

    kupl_host_info_print();
    return KUPL_OK;
}

void kupl_host_info_fini()
{
    if (g_host_info.init != true) {
        return;
    }

    g_host_info.init = false;

    kupl_safe_free(g_host_info.numa_distance);
    kupl_safe_free(g_host_info.numas);
    kupl_safe_free(g_host_info.cpus);
    kupl_safe_free(g_compute_place);
}

const kupl_host_info_t* kupl_get_host_info()
{
    kupl_assert(g_host_info.init);
    return &g_host_info;
}

kupl_compute_place_t kupl_get_compute_place(int gcid)
{
    if (gcid <= 0 || gcid >= g_host_info.pu_cnt) {
        return {0, 0};
    }

    kupl_compute_place_t &cp = g_compute_place[gcid];
    if (cp.cpu_id >= 0) {
        return cp;
    }

    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);

    int affinity_core_count = 0;
    for (int i = 0; i < g_host_info.pu_cnt; ++i) {
        if (CPU_ISSET(i, &cpu_set)) {
            ++affinity_core_count;
        }
    }

    /* User don't set affinity or system don't support affinity, so we use seq affinity */
    static KUPL_ATOMIC_INT g_cpu_id(0);
    if (affinity_core_count > 1) {
        cp.cpu_id = KUPL_ATOMIC_ADD(&g_cpu_id, 1) % (g_host_info.pu_cnt / g_host_info.numa_cnt);
        cp.numa_id = 0;
        kupl_info("use seq cp [cpu_id :%d numa_id:%d]", cp.cpu_id, cp.numa_id);
        return cp;
    }

    for (int i = 0; i < g_host_info.pu_cnt; ++i) {
        if (CPU_ISSET(i, &cpu_set)) {
            cp = g_host_info.cpus[i];
            kupl_info("use affinity cp [cpu:%d numa:%d]", cp.cpu_id, cp.numa_id);
            return cp;
        }
    }

    kupl_warn("use default cp");
    cp = g_host_info.cpus[0];
    return cp;
}

void kupl_host_info_print()
{
    if (!g_host_info.init) {
        kupl_warn("kupl host info is not initialize");
        return;
    }

    if (kupl_config_get_value(KUPL_ENABLE_HARDWARE_VERBOSE) == 0) {
        return;
    }

    printf("Socket count: %d NUMA count: %d Core count: %d PU count: %d\n",
        g_host_info.socket_cnt, g_host_info.numa_cnt, g_host_info.core_cnt, g_host_info.pu_cnt);
    printf("NUMA distance\n");
    for (int i = 0; i < g_host_info.numa_cnt; ++i) {
        const int buf_len = 1024;
        char strbuf[buf_len];
        size_t offset = 0;
        size_t tot_offset = 0;
        char* ptr = strbuf;
        for (int j = 0; j < g_host_info.numa_cnt; ++j) {
            offset = sprintf(ptr, "\t%lu", g_host_info.numa_distance[i * g_host_info.numa_cnt + j]);
            tot_offset += offset;
            if (tot_offset >= buf_len) {
                kupl_error("the numa info length is out of bound");
                return;
            }
            ptr += offset;
        }
        printf("%s\n", strbuf);
    }

    printf("PU information\n");
    for (int i = 0; i < g_host_info.pu_cnt; ++i) {
        printf("[cpu_id %3d numa_id %3d]\n", g_host_info.cpus[i].cpu_id, g_host_info.cpus[i].numa_id);
    }

    return;
}

#else
#include <omp.h>
#include <dlfcn.h>
#include <numa.h>
#include "utils/sys/kupl_compiler.h"

static kupl_host_info_t g_host_info = {
    .init = false,
    .socket_cnt = 0,
    .numa_cnt = 0,
    .core_cnt = 0,
    .pu_cnt = 0,
    .pu_conf = 0,
    .avail_pu_cnt = 0,
    .numa_distance = nullptr,
    .numas = nullptr,
    .cpus = nullptr,
};

static kupl_compute_place_t g_cp = {
    .cpu_id = 0,
    .numa_id = 0,
};

int kupl_host_info_init()
{
    if (g_host_info.init) {
        return KUPL_OK;
    }
    /* pu_cnt gets the number of all ONLINE cpus in the system,
       pu_conf gets the number of all ONLINE+OFFLINE cpus in the system. */
    g_host_info.pu_cnt = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    if ((g_host_info.pu_cnt <= 0) || (g_host_info.pu_cnt > CPU_SETSIZE)) {
        return KUPL_ERROR;
    }
    g_host_info.pu_conf = static_cast<int>(sysconf(_SC_NPROCESSORS_CONF));
    if ((g_host_info.pu_conf <= 0) || (g_host_info.pu_conf > CPU_SETSIZE)) {
        return KUPL_ERROR;
    }

    CPU_ZERO(&g_host_info.avail_set);
    std::string backend_type = kupl_config_get_value_str(KUPL_EXECUTOR_BACKEND);
    if (backend_type == std::string("pthread")) {
        pthread_getaffinity_np(pthread_self(), sizeof(g_host_info.avail_set), &g_host_info.avail_set);
    } else {
        int max_threads = omp_get_max_threads();
        for (int i = 0; (i < max_threads) && i < CPU_SETSIZE ; i++) {
            CPU_SET(i, &g_host_info.avail_set);
        }
    }

    g_host_info.avail_pu_cnt = 0;
    for (int i = 0; i < g_host_info.pu_conf; i++) {
        if (CPU_ISSET(i, &g_host_info.avail_set)) {
            g_host_info.avail_pu_cnt++;
        }
    }

    g_host_info.numa_cnt = numa_max_node() + 1;
    if ((g_host_info.numa_cnt <= 0) || (g_host_info.numa_cnt > KUPL_NUMA_MAX)) {
        return KUPL_ERROR;
    }

    g_host_info.init = true;
    return KUPL_OK;
}

void kupl_host_info_fini()
{
    return;
}

const kupl_host_info_t* kupl_get_host_info()
{
    return &g_host_info;
}

kupl_compute_place_t kupl_get_compute_place(int gcid)
{
    (void)gcid;
    return g_cp;
}

void kupl_host_info_print()
{
    return;
}

kupl_arch_type_t kupl_arch_detect()
{
    kupl_arch_type_t arch_type;
    unsigned long long cpu_id;
    __asm__ volatile("mrs %0, MIDR_EL1":"=r"(cpu_id));

    unsigned long long vendor = (cpu_id >> 0x18) & 0xFF;
    unsigned long long part_id = (cpu_id >> 0x4) & 0xFFF;
    if ((vendor == 0x48) && (part_id == 0xD01)) {
        arch_type = KUPL_CPU_HISILICOM_TSV110;
    } else if ((vendor == 0x48) && (part_id == 0xD02)) {
        arch_type = KUPL_CPU_HISILICOM_920B;
    } else if ((vendor == 0x48) && (part_id == 0xD03)) {
        arch_type = KUPL_CPU_HISILICOM_920C;
    } else if (part_id == 0xD22) {
        arch_type = KUPL_CPU_HISILICOM_920F;
    } else {
        arch_type = KUPL_CPU_UNKNOW;
    }
    return arch_type;
}

#endif
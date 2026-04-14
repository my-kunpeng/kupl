# Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
#
# KUPL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#        http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

include(CheckIncludeFiles)

function(libkupl_check_tcmalloc LIB_PATH KUPL_TCMALLOC_LIB)
    if (IS_DIRECTORY ${LIB_PATH})
        find_file(TCMALLOC_HEADER_FOUND NAMES tcmalloc.h HINTS ${LIB_PATH}/include/gperftools)
        find_library(TCMALLOC_LIB_FOUND
            NAMES tcmalloc libtcmalloc
            HINTS ${LIB_PATH}/lib
        )
        if (TCMALLOC_HEADER_FOUND AND TCMALLOC_LIB_FOUND)
            include_directories(${LIB_PATH}/include)
            link_directories(${LIB_PATH}/lib)
            set(KUPL_TCMALLOC_LIB ${TCMALLOC_LIB_FOUND} PARENT_SCOPE)
            return()
        endif()
    endif()
    message(FATAL_ERROR "-- KUPL_BUILD_WITH_TCMALLOC is a invalid path")
endfunction()

function(libkupl_check_jemalloc LIB_PATH KUPL_JEMALLOC_LIB)
    if (IS_DIRECTORY ${LIB_PATH})
        find_file(JEMALLOC_HEADER_FOUND NAMES jemalloc.h HINTS ${LIB_PATH}/include/jemalloc)
        find_library(JEMALLOC_LIB_FOUND
            NAMES jemalloc libjemalloc
            HINTS ${LIB_PATH}/lib
        )
        if (JEMALLOC_HEADER_FOUND AND JEMALLOC_LIB_FOUND)
            include_directories(${LIB_PATH}/include)
            link_directories(${LIB_PATH}/lib)
            set(KUPL_JEMALLOC_LIB ${JEMALLOC_LIB_FOUND} PARENT_SCOPE)
            return()
        endif()
    endif()
    message(FATAL_ERROR "-- KUPL_BUILD_WITH_JEMALLOC is a invalid path")
endfunction()

function(libkupl_check_kqmalloc LIB_PATH KUPL_KQMALLOC_LIB)
    if (IS_DIRECTORY ${LIB_PATH})
        find_library(KQMALLOC_LIB_FOUND
            NAMES kqmallocmt libkqmallocmt
            HINTS ${LIB_PATH}/lib
        )
        if (KQMALLOC_LIB_FOUND)
            link_directories(${LIB_PATH}/lib)
            set(KUPL_KQMALLOC_LIB ${KQMALLOC_LIB_FOUND} PARENT_SCOPE)
            return()
        endif()
    endif()
    message(FATAL_ERROR "-- KUPL_BUILD_WITH_KQMALLOC is a invalid path")
endfunction()

function(libkupl_check_xpmem LIB_PATH KUPL_USE_XPMEM KUPL_XPMEM_LIB)
    if (IS_DIRECTORY ${LIB_PATH})
        find_file(XPMEM_HEADER_FOUND NAMES xpmem.h HINTS ${LIB_PATH}/include)
        find_library(XPMEM_LIB_FOUND
            NAMES xpmem libxpmem
            HINTS ${LIB_PATH}/lib
        )
        if (XPMEM_HEADER_FOUND AND XPMEM_LIB_FOUND)
            include_directories(${LIB_PATH}/include)
            link_directories(${LIB_PATH}/lib)
            set(KUPL_XPMEM_LIB ${XPMEM_LIB_FOUND} PARENT_SCOPE)
            set(KUPL_USE_XPMEM ON PARENT_SCOPE)
            return()
        endif()
    endif()
    message(FATAL_ERROR "-- KUPL_BUILD_WITH_XPMEM is a invalid path")
endfunction()

function(libkupl_check_hwloc LIB_PATH KUPL_HWLOC_LIB)
    if (IS_DIRECTORY ${LIB_PATH})
        find_file(HWLOC_HEADER_FOUND NAMES hwloc.h HINTS ${LIB_PATH}/include)
        find_library(HWLOC_LIB_FOUND
            NAMES hwloc libhwloc
            HINTS ${LIB_PATH}/lib
        )
        if (HWLOC_HEADER_FOUND AND HWLOC_LIB_FOUND)
            include_directories(${LIB_PATH}/include)
            link_directories(${LIB_PATH}/lib)
            set(KUPL_HWLOC_LIB ${HWLOC_LIB_FOUND} PARENT_SCOPE)
            return()
        endif()
    endif()
    message(FATAL_ERROR "-- KUPL_BUILD_WITH_HWLOC is a invalid path")
endfunction()
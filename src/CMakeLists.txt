# Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
#
# KUPL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#        http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# options
option(KUPL_USE_TCMALLOC       "Kupl use tcmalloc"          OFF)
option(KUPL_USE_JEMALLOC       "Kupl use jemalloc"          OFF)
option(KUPL_USE_KQMALLOC       "Kupl use kqmalloc"          OFF)
option(KUPL_USE_XPMEM          "Kupl use xpmem"             OFF)
option(KUPL_USE_HWLOC          "Kupl use hwloc"             OFF)
option(ENABLE_KUPL_PROFILE     "Enable kupl profile"        OFF)
option(ENABLE_KUPL_TRACE       "Enable kupl trace"          OFF)
option(ENABLE_KUPL_DEBUG       "Enable debug mode"          OFF)
option(ENABLE_KUPL_ASSERT      "Enable assert mode"         OFF)
option(ENABLE_KUPL_GLIBC_MALLOC "kupl will disable the memory pool, performance maybe bad" OFF)

find_package(OpenMP REQUIRED)
if(OPENMP_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

# Check debug mode
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(ENABLE_KUPL_DEBUG ON)
    set(ENABLE_KUPL_ASSERT ON)
    add_compile_options(-O0 -g -gdwarf-4)
    add_definitions(-DENABLE_KUPL_DEBUG)
    message("-- Caution : using debug mode, performance is bad, just for develop")
elseif(CMAKE_BUILD_TYPE MATCHES "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -Wfloat-equal -fno-common -Weffc++")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual -Wnon-virtual-dtor -Wdelete-non-virtual-dtor")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat=2 -Wunused -Wswitch-default -Wundef -Wdate-time -Wshadow")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wcast-align -Wcast-qual -pipe -Wconversion")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wframe-larger-than=16384 -Wvla -fstack-protector-strong")

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wtrampolines -freg-struct-return -Wstack-usage=32768")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wstrict-prototypes")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 19.0)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-designated-field-initializers")
        endif()
    endif()
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack")
endif()

if(KUPL_BUILD_KIND MATCHES "src")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
elseif(KUPL_BUILD_KIND MATCHES "test")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    link_libraries(gcov)
elseif(KUPL_BUILD_KIND MATCHES "cida")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    link_libraries(gcov)
elseif(KUPL_BUILD_KIND MATCHES "fuzz")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage -fsanitize-coverage=trace-pc")
    link_libraries(gcov)
endif()

if(CMAKE_BUILD_TYPE MATCHES "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FORTIFY_SOURCE=2")
endif()

include_directories(${KUPL_ROOT_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

include(${KUPL_CMAKE_INCLUDE_PATH}/libkupl_check_libs.cmake)
include(${KUPL_CMAKE_INCLUDE_PATH}/libkupl_source_files.cmake)

if(ENABLE_KUPL_PROFILE)
    message("-- Enable kupl profile")
    add_definitions(-DENABLE_KUPL_PROFILE)
endif()

if(ENABLE_KUPL_TRACE)
    message("-- Enable kupl trace")
    add_definitions(-DENABLE_KUPL_TRACE)
endif()

libkupl_source_files(KUPL_SRCS)
add_library(${LIBKUPL_NAME} SHARED ${KUPL_SRCS})

if(ENABLE_KUPL_MMA)
    message("-- Enable kupl mma")
    add_definitions(-DENABLE_KUPL_MMA)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set_source_files_properties(mma/kupl_mma.cpp PROPERTIES COMPILE_OPTIONS "-mcpu=linxicore6100;--rtlib=compiler-rt;-Wno-undefined-arm-za;-Wno-undefined-arm-streaming")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(mma/kupl_mma.cpp PROPERTIES COMPILE_OPTIONS "-march=armv9-a+sme-f64f64+sve")
endif()

if(ENABLE_KUPL_GLIBC_MALLOC)
    message("-- Caution: kupl will disable the memory pool, performance maybe bad!")
    add_definitions(-DENABLE_KUPL_GLIBC_MALLOC)
endif()

if(KUPL_BUILD_WITH_TCMALLOC)
    libkupl_check_tcmalloc(${KUPL_BUILD_WITH_TCMALLOC} KUPL_TCMALLOC_LIB)
    target_link_libraries(${LIBKUPL_NAME} ${KUPL_TCMALLOC_LIB})
    add_definitions(-DKUPL_USE_TCMALLOC)
    message("-- use tcmalloc")
elseif(KUPL_BUILD_WITH_JEMALLOC)
    libkupl_check_jemalloc(${KUPL_BUILD_WITH_JEMALLOC} KUPL_JEMALLOC_LIB)
    target_link_libraries(${LIBKUPL_NAME} ${KUPL_JEMALLOC_LIB})
    add_definitions(-DKUPL_USE_JEMALLOC)
    message("-- use jemalloc")
elseif(KUPL_BUILD_WITH_KQMALLOC)
    libkupl_check_kqmalloc(${KUPL_BUILD_WITH_KQMALLOC} KUPL_KQMALLOC_LIB)
    target_link_libraries(${LIBKUPL_NAME} ${KUPL_KQMALLOC_LIB})
    add_definitions(-DKUPL_USE_KQMALLOC)
    message("-- use kqmalloc")
endif()

# Add libhwloc
if(KUPL_BUILD_WITH_HWLOC)
    libkupl_check_hwloc(${KUPL_BUILD_WITH_HWLOC} KUPL_HWLOC_LIB)
    target_link_libraries(${LIBKUPL_NAME} ${KUPL_HWLOC_LIB})
    add_definitions(-DKUPL_USE_HWLOC)
    message("-- use hwloc")
endif()

# Add xpmem
if(KUPL_BUILD_WITH_XPMEM)
    libkupl_check_xpmem(${KUPL_BUILD_WITH_XPMEM} KUPL_USE_XPMEM KUPL_XPMEM_LIB)
    if(KUPL_USE_XPMEM)
        libkupl_shm_xpmem_source_files(KUPL_SHM_XPMEM_SRCS)
        set(LIBKUPL_SHM_XPMEM_NAME "kupl_shm_xpmem")
        add_library(${LIBKUPL_SHM_XPMEM_NAME} SHARED ${KUPL_SHM_XPMEM_SRCS})
        target_link_libraries(${LIBKUPL_SHM_XPMEM_NAME} ${KUPL_XPMEM_LIB})
        install(TARGETS ${LIBKUPL_SHM_XPMEM_NAME} DESTINATION ${KUPL_INSTALL_LIBDIR}/shm)
    endif()
endif()

find_library(LIB_RT librt.so.1)
if(NOT LIB_RT)
    find_library(LIB_RT librt.so)
    if(NOT LIB_RT)
        message(FATAL_ERROR "library rt not found")
    endif()
endif()
target_link_libraries(${LIBKUPL_NAME} ${LIB_RT} pthread dl numa)

set_target_properties(${LIBKUPL_NAME} PROPERTIES VERSION 1.26.0.RC1 SOVERSION 1)

#Install
libkupl_header_install()
install(TARGETS ${LIBKUPL_NAME} DESTINATION ${KUPL_INSTALL_LIBDIR})
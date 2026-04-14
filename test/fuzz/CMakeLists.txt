# Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
#
# KUPL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#        http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

cmake_minimum_required(VERSION 3.14)

project(kupl_fuzz VERSION 1.0.0)

set(KUPL_INSTALL_BIN_PATH ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
include_directories(${CMAKE_INSTALL_PREFIX}/include ${CMAKE_CURRENT_SOURCE_DIR})
link_directories(${CMAKE_INSTALL_PREFIX}/lib)

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_options(-O0 -g -gdwarf-4)
    message("-- Caution : using debug mode, performance is bad, just for develop")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../conf/comp.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../conf/comp.cmake)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp \
    -fprofile-arcs -ftest-coverage -Wall -fsanitize-coverage=trace-pc -Wno-unused-value")

if(${KUPL_BUILD_ASAN} MATCHES "ON")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
        -Lasan -fsanitize=address -fsanitize-recover=address -fno-omit-frame-pointer")
endif()

set(CMAKE_EXE_LINKER_FLAGS "-fprofile-arcs -ftest-coverage")
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

add_subdirectory(omp)
add_subdirectory(shm)
add_subdirectory(mma)
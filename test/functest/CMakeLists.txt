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

project(kupl_test VERSION 1.0.0)

set(KUPL_INSTALL_BIN_PATH ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
include_directories(${CMAKE_INSTALL_PREFIX}/include)
link_directories(${CMAKE_INSTALL_PREFIX}/lib)

set(LIBRARY_PATH_ENV "$ENV{LIBRARY_PATH}")
string(REPLACE ":" ";" LIBRARY_PATH_LIST "${LIBRARY_PATH_ENV}")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(COMPILER_FILTER "gcc")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(COMPILER_FILTER "clang")
else()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

if(ENABLE_KUPL_MMA)
    message("-- DT/UT enable kupl mma")
    add_definitions(-DENABLE_KUPL_MMA)
endif()

set(FILTERED_PATHS "")
foreach(PATH_ITEM IN LISTS LIBRARY_PATH_LIST)
    if(NOT "${PATH_ITEM}" STREQUAL "")
        if("${PATH_ITEM}" MATCHES "${COMPILER_FILTER}")
            list(APPEND FILTERED_PATHS "${PATH_ITEM}")
        endif()
        if("${PATH_ITEM}" MATCHES "gtest")
            list(APPEND FILTERED_PATHS "${PATH_ITEM}")
        endif()
    endif()
endforeach()

find_package(GTest
    HINTS ${FILTERED_PATHS}
)

if("${GTest_FOUND}")
    set(GTEST_LIB "GTest::gtest")
else()
    set(GTEST_LIB "gtest")
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(COV_LIB "gcov")
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_options(-O0 -g -gdwarf-4)
    message("-- Caution : using debug mode, performance is bad, just for develop")
endif()

add_subdirectory(omp)
add_subdirectory(pthread)
add_subdirectory(shm)
add_subdirectory(mma)

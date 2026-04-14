cmake_minimum_required(VERSION 3.14.1)
# Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
#
# KUPL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#        http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

project(kupl VERSION 1.0.0)

# colored output
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
set(${COLOR_FLAGS} "-fdiagnostics-color=always")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
set(${COLOR_FLAGS} "-fcolor-diagnostics")
endif ()

# Set library's name
set(LIBKUPL_NAME "kupl")

# Set kupl root path
set(KUPL_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Set cmake include directories
set(KUPL_CMAKE_INCLUDE_PATH ${KUPL_ROOT_DIR}/cmake)

# Set installation directories
set(KUPL_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_PREFIX}/include)
set(KUPL_INSTALL_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib)

# Set compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COLOR_FLAGS} -std=c++17")

add_subdirectory(src)
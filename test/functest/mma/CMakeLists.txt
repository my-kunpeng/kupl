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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
set(CMAKE_EXE_LINKER_FLAGS "-fprofile-arcs -ftest-coverage")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv9-a+sme-f64f64+sve")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv9-a+sme-f64f64+sve")
endif()

file(GLOB_RECURSE TEST_MMA_FILES *.cpp)

add_executable(test_mma_main ${TEST_MMA_FILES})
target_link_libraries(test_mma_main
    -Wl,--start-group
    ${GTEST_LIB} kupl dl pthread ${COV_LIB}
    -Wl,--end-group
)
install(TARGETS test_mma_main DESTINATION ${KUPL_INSTALL_BIN_PATH})
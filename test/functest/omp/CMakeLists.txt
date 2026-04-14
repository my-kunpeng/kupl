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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp -fprofile-arcs -ftest-coverage")
set(CMAKE_EXE_LINKER_FLAGS "-fprofile-arcs -ftest-coverage")

if (KUPL_BUILD_KIND MATCHES "test")
    file(GLOB_RECURSE TEST_OMP_FILES ../common/*.cpp *.cpp ../internals/common/*.cpp ../internals/omp/*.cpp)
elseif (KUPL_BUILD_KIND MATCHES "cida")
    file(GLOB_RECURSE TEST_OMP_FILES ../common/*.cpp *.cpp)
endif()

add_executable(test_omp_main ${TEST_OMP_FILES})
target_link_libraries(test_omp_main
    -Wl,--start-group
    ${GTEST_LIB} kupl dl pthread ${COV_LIB}
    -Wl,--end-group
)
install(TARGETS test_omp_main DESTINATION ${KUPL_INSTALL_BIN_PATH})
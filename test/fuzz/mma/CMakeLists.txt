# Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
#
# KUPL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#        http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set(KUPL_INSTALL_BIN_PATH ${CMAKE_INSTALL_PREFIX}/bin)
set(FUZZ_BIN fuzz_mma_main)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv9-a+sme-f64f64+sve")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv9-a+sme-f64f64+sve")
endif()

file(GLOB_RECURSE FUZZ_FILES *.h *.cpp)

add_executable(${FUZZ_BIN} ${FUZZ_FILES})

target_link_libraries(${FUZZ_BIN}
    -Wl,--start-group
    kupl dl pthread gcov Secodefuzz
    -Wl,--end-group
)

install(TARGETS ${FUZZ_BIN} DESTINATION ${KUPL_INSTALL_BIN_PATH})

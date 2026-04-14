# Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
#
# KUPL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#        http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# find all source files to compiler kupl library
function(libkupl_source_files src_files)
    file(GLOB_RECURSE kupl_srcs
        *.cpp
    )
    file(GLOB remove_srcs memory/shm/xpmem/*.cpp)
    list(REMOVE_ITEM kupl_srcs ${remove_srcs})

    set(${src_files} ${kupl_srcs} PARENT_SCOPE)
endfunction()

function(libkupl_shm_xpmem_source_files shm_xpmem_src_files)
    file(GLOB srcs ${srcs} memory/shm/xpmem/*.cpp)
    set(${shm_xpmem_src_files} ${srcs} PARENT_SCOPE)
endfunction()

# install header
function(libkupl_header_install)
    # install kupl.h
    file (GLOB BASE_HEADER *.h)

    install(FILES ${BASE_HEADER} DESTINATION ${KUPL_INSTALL_INCLUDEDIR} PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)

    if (KUPL_BUILD_KIND MATCHES "test")
        # install kupl dm
        install(
            DIRECTORY dm/
            DESTINATION ${KUPL_INSTALL_INCLUDEDIR}/dm
            FILES_MATCHING PATTERN "*.h"
        )

        # install kupl executor
        install(
            DIRECTORY executor/
            DESTINATION ${KUPL_INSTALL_INCLUDEDIR}/executor
            FILES_MATCHING PATTERN "*.h"
        )

        # install kupl memory
        install(
            DIRECTORY memory/
            DESTINATION ${KUPL_INSTALL_INCLUDEDIR}/memory
            FILES_MATCHING PATTERN "*.h"
        )

        # install kupl mt
        install(
            DIRECTORY mt/
            DESTINATION ${KUPL_INSTALL_INCLUDEDIR}/mt
            FILES_MATCHING PATTERN "*.h"
        )

        # install kupl utils
        install(
            DIRECTORY utils/
            DESTINATION ${KUPL_INSTALL_INCLUDEDIR}/utils
            FILES_MATCHING PATTERN "*.h" PATTERN "utils/config/*.inc"
        )

        # install kupl tools
        install(
            DIRECTORY tools/
            DESTINATION ${KUPL_INSTALL_INCLUDEDIR}/tools
            FILES_MATCHING PATTERN "*.h"
        )

        # install kupl mma
        install(
            DIRECTORY mma/
            DESTINATION ${KUPL_INSTALL_INCLUDEDIR}/mma
            FILES_MATCHING PATTERN "*.h"
        )
    endif()

endfunction()
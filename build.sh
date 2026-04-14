#!/bin/bash
# Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
#
# KUPL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#        http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set -e

print_help(){
    echo "build.sh -- A tool for building KUPL"
    echo "Parameters:"
    echo "--compiler=[gcc[default]|clang]                               |  build with different compiler"
    echo "--cleanup=[off|on(default)]                                   |  cleanup build and install path before build"
    echo "--build_kind=[src(default)|test|cida|fuzz|cida-only]          |  build src or test or cida or fuzz suite"
    echo "--build_type=[Release(default)|Debug|RelWithDebInfo]          |  build release or debug version"
    echo "--build_ninja=[off(default)|on]                               |  build using ninja"
    echo "--enable_prof=[off(default)|on]                               |  build with profile or trace module"
    echo "--enable_mma=[on(default)|off]                                |  build with mma module"
    echo "--install_path=absolute_path                                  |  an absolute path for creating install folder"
    echo "--help                                                        |  print this help message"
    return
}

export KUPL_PROJ_PATH=$(cd "$(dirname ${0})"; pwd -P)
export KUPL_CLEANUP="on"
export KUPL_COMPILER="gcc"
export KUPL_BUILD_TYPE="Release"
export ENABLE_KUPL_PROFILE="OFF"
export ENABLE_KUPL_TRACE="OFF"
export ENABLE_KUPL_MMA="ON"
export KUPL_BUILD_KIND="src"
export KUPL_INSTALL_PATH="$KUPL_PROJ_PATH/install"
export KUPL_GENERATOR="Unix Makefiles"

function check_glibc()
{
    local result=$(nm -gD "${KUPL_INSTALL_PATH}/lib/libkupl.so" | grep GLIBC_ | grep -v 2.17)
    if [ ! -z "$result" ]; then
        echo ""
        echo "Check the compatibility of the following functions:"
        echo $result
        exit 1
    fi
}

function build_src()
{
    echo ""
    echo "build src"
    local source_path=$KUPL_PROJ_PATH
    local build_path=$KUPL_PROJ_PATH/build/src
    mkdir -p ${build_path} && cd ${build_path}
    cmake -G "${KUPL_GENERATOR}"                        \
        -S ${source_path} -B ${build_path}              \
        -DCMAKE_C_COMPILER=${KUPL_C_COMPILER}           \
        -DCMAKE_CXX_COMPILER=${KUPL_CXX_COMPILER}       \
        -DENABLE_KUPL_PROFILE=${ENABLE_KUPL_PROFILE}    \
        -DENABLE_KUPL_TRACE=${ENABLE_KUPL_TRACE}        \
        -DENABLE_KUPL_MMA=${ENABLE_KUPL_MMA}            \
        -DCMAKE_BUILD_TYPE=${KUPL_BUILD_TYPE}           \
        -DKUPL_BUILD_KIND=${KUPL_BUILD_KIND}            \
        -DCMAKE_INSTALL_PREFIX=${KUPL_INSTALL_PATH}
    cmake --build ${build_path} -j --target install
    check_glibc
}

function build_test()
{
    echo ""
    echo "build test"
    local source_path=$KUPL_PROJ_PATH/test/functest
    local build_path=$KUPL_PROJ_PATH/build/functest
    mkdir -p ${build_path} && cd ${build_path}
    cmake -G "${KUPL_GENERATOR}"                        \
        -S ${source_path} -B ${build_path}              \
        -DCMAKE_C_COMPILER=$(which mpicc)               \
        -DCMAKE_CXX_COMPILER=$(which mpicxx)            \
        -DENABLE_KUPL_MMA=${ENABLE_KUPL_MMA}            \
        -DCMAKE_BUILD_TYPE=${KUPL_BUILD_TYPE}           \
        -DKUPL_BUILD_KIND=${KUPL_BUILD_KIND}            \
        -DCMAKE_INSTALL_PREFIX=${KUPL_INSTALL_PATH}
    cmake --build ${build_path} -j --target install
}

function build_cida()
{
    echo ""
    echo "build cida"
    local source_path=$KUPL_PROJ_PATH/test/functest
    local build_path=$KUPL_PROJ_PATH/build/cida
    mkdir -p ${build_path} && cd ${build_path}
    cmake -G "${KUPL_GENERATOR}"                        \
        -S ${source_path} -B ${build_path}              \
        -DCMAKE_C_COMPILER=$(which mpicc)               \
        -DCMAKE_CXX_COMPILER=$(which mpicxx)            \
        -DENABLE_KUPL_MMA=${ENABLE_KUPL_MMA}            \
        -DCMAKE_BUILD_TYPE=${KUPL_BUILD_TYPE}           \
        -DKUPL_BUILD_KIND=${KUPL_BUILD_KIND}            \
        -DCMAKE_INSTALL_PREFIX=${KUPL_INSTALL_PATH}
    cmake --build ${build_path} -j --target install
}

function build_fuzz()
{
    echo ""
    echo "build fuzz"
    local source_path=$KUPL_PROJ_PATH/test/fuzz
    local build_path=$KUPL_PROJ_PATH/build/fuzz
    mkdir -p ${build_path} && cd ${build_path}
    cmake -G "${KUPL_GENERATOR}"                        \
        -S ${source_path} -B ${build_path}              \
        -DCMAKE_C_COMPILER=$(which mpicc)               \
        -DCMAKE_CXX_COMPILER=$(which mpicxx)            \
        -DCMAKE_BUILD_TYPE=${KUPL_BUILD_TYPE}           \
        -DCMAKE_INSTALL_PREFIX=${KUPL_INSTALL_PATH}
    cmake --build ${build_path} -j --target install
}

function cleanup()
{
    echo ""
    echo "do cleanup"
    local build_path=$KUPL_PROJ_PATH/build
    if [ -d "${build_path}" ];then
        rm -rf ${build_path}
        echo "cleanup ${build_path}"
    fi
    if [ -d "${KUPL_INSTALL_PATH}" ];then
        rm -rf ${KUPL_INSTALL_PATH}
        echo "cleanup ${KUPL_INSTALL_PATH}"
    fi
}

function set_compiler()
{
    case "$1" in
        gcc)
            export CC=$(which gcc)
            export CXX=$(which g++)
            KUPL_C_COMPILER=$(which gcc)
            KUPL_CXX_COMPILER=$(which g++)
            ;;
        clang)
            export CC=$(which clang)
            export CXX=$(which clang++)
            KUPL_C_COMPILER=$(which clang)
            KUPL_CXX_COMPILER=$(which clang++)
            ;;
        *)
            echo "Unsupported compiler $1."
            exit 1
            ;;
    esac
    KUPL_COMPILER="$1"
}

function set_build_type()
{
    case "$1" in
        r|release|Release)
            KUPL_BUILD_TYPE="Release"
            ;;
        d|debug|Debug)
            KUPL_BUILD_TYPE="Debug"
            ;;
        rwdi|RelWithDebInfo)
            KUPL_BUILD_TYPE="RelWithDebInfo"
            ;;
        *)
            echo "Unsupported build type: $1."
            exit 1
            ;;
    esac
}

function parse_args()
{
    for i in "$@"; do
        case "$i" in
            --cleanup=*)
                if [[ "${i#*=}" == "off" ]]; then
                    KUPL_CLEANUP="off"
                fi
                ;;
            --compiler=*)
                KUPL_COMPILER="${i#*=}"
                ;;
            --install_path=*)
                KUPL_INSTALL_PATH="${i#*=}"
                ;;
            --build_type=*)
                if [[ "${KUPL_BUILD_KIND}" == "src" ]]; then
                    set_build_type "${i#*=}"
                fi
                ;;
            --build_ninja=*)
                if [[ "${i#*=}" == "on" ]]; then
                    KUPL_GENERATOR="Ninja"
                fi
                ;;
            --enable_prof=*)
                if [[ "${i#*=}" == "on" ]]; then
                    ENABLE_KUPL_PROFILE="ON"
                    ENABLE_KUPL_TRACE="ON"
                fi
                ;;
            --enable_mma=*)
                if [[ "${i#*=}" == "off" ]]; then
                    ENABLE_KUPL_MMA="OFF"
                fi
                ;;
            --build_kind=*)
                KUPL_BUILD_KIND="${i#*=}"
                case "$KUPL_BUILD_KIND" in
                    src)
                        ;;
                    test)
                        KUPL_BUILD_TYPE="Debug"
                        ;;
                    cida)
                        KUPL_BUILD_TYPE="Release"
                        ;;
                    cida-only)
                        KUPL_BUILD_TYPE="Release"
                        ;;
                    fuzz)
                        KUPL_BUILD_TYPE="Debug"
                        ;;
                    *)
                        echo "invalid build kind $KUPL_BUILD_KIND"
                        exit 1;
                        ;;
                esac
                ;;
            --help|-h)
                print_help
                exit 0
                ;;
            *)
                echo "Unknown option: $i"
                echo ""
                exit 1
                ;;
        esac
    done
}

function main()
{
    echo "PROJ_PATH:    "   $KUPL_PROJ_PATH
    echo "INSTALL_PATH: "   $KUPL_INSTALL_PATH
    echo "BUILD_TYPE:   "   $KUPL_BUILD_TYPE
    echo "BUILD_KIND:   "   $KUPL_BUILD_KIND
    echo "GENERATOR:    "   $KUPL_GENERATOR
    echo "PROFILE:      "   $ENABLE_KUPL_PROFILE
    echo "TRACE:        "   $ENABLE_KUPL_TRACE
    echo "MMA:          "   $ENABLE_KUPL_MMA
    if [[ "${KUPL_CLEANUP,,}" == "on" ]]; then
        cleanup
    fi
    set_compiler $KUPL_COMPILER

    if [[ "${KUPL_BUILD_KIND,,}" != "cida-only" ]]; then
        build_src
    fi

    case "$KUPL_BUILD_KIND" in
        test)
            build_test
            ;;
        cida)
            build_cida
            ;;
        cida-only)
            build_cida
            ;;
        fuzz)
            build_fuzz
            ;;
    esac
}

parse_args $@
main
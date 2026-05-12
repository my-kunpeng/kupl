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
PROJ_PATH=$(cd "$(dirname ${0})"; pwd -P)
INSTALL_PATH=${3-"${PROJ_PATH}/install"}
echo "PROJ_PATH:    "   $PROJ_PATH
echo "INSTALL_PATH: "   $INSTALL_PATH

if [ -d "$PROJ_PATH/lcov" ];then
  rm -rf $PROJ_PATH/lcov
fi

hbw_detected=0
# A NUMA node with no cpu core is HBW node
if lscpu | grep -q 'NUMA node.* CPU(s):[^0-9]*$'; then
  if !(numactl --hardware | grep -q 'node [0-9]* size: 0 MB'); then
    hbw_detected=1
  fi
fi

zcopy_detected=0
if lsmod | grep zcopy; then
  zcopy_detected=1
fi

sdma_detected=0
if lsmod | grep sdma; then
  sdma_detected=1
fi

mkdir -p $PROJ_PATH/lcov
cd $PROJ_PATH/lcov
BUILD_KIND=${1-"test"}
BUILD_COMPLIER=${2-"gcc"}

# 1. perform test
if [[ "${BUILD_KIND}" == "test" ]]; then
  if ! [ -f "$INSTALL_PATH/bin/test_pthread_main" ]; then
    echo "$INSTALL_PATH/bin/test_pthread_main not exist."
    exit 0
  fi
  if [ $hbw_detected -eq 1 ]; then
    KUPL_EXECUTOR_COUNT=1024 KUPL_EXECUTOR_BACKEND=pthread KUPL_SCHED_POLICY=static_mq numactl -N 0 $INSTALL_PATH/bin/test_pthread_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_pthread_detail.xml
    KUPL_EXECUTOR_COUNT=1024 KUPL_EXECUTOR_BACKEND=pthread KUPL_SCHED_POLICY=static_mq numactl -N 0 --membind=16 $INSTALL_PATH/bin/test_pthread_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_pthread_detail.xml --gtest_filter=*kupl_malloc*
  else
    KUPL_EXECUTOR_COUNT=1024 KUPL_EXECUTOR_BACKEND=pthread KUPL_SCHED_POLICY=static_mq numactl -N 0 $INSTALL_PATH/bin/test_pthread_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_pthread_detail.xml --gtest_filter=-*hbw*:*kupl_mem_*
  fi

  KUPL_SCHED_MQ_PLACEQ_AFFINITY="0|0,1|0-1023|6-5|1025-1026|*" KUPL_EXECUTOR_BACKEND=pthread KUPL_SCHED_POLICY=mq numactl -N 0 $INSTALL_PATH/bin/test_pthread_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_pthread_detail.xml --gtest_filter=*queue_event*
  KUPL_SCHED_MQ_PLACEQ_AFFINITY="" KUPL_EXECUTOR_BACKEND=pthread KUPL_SCHED_POLICY=mq numactl -N 0 $INSTALL_PATH/bin/test_pthread_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_pthread_detail.xml --gtest_filter=*queue_event*
  KUPL_SCHED_MQ_PLACEQ_AFFINITY="0,1" KUPL_EXECUTOR_BACKEND=pthread KUPL_SCHED_POLICY=mq numactl -N 0 $INSTALL_PATH/bin/test_pthread_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_pthread_detail.xml --gtest_filter=*queue_event*
  KUPL_SCHED_MQ_PLACEQ_AFFINITY="0" KUPL_EXECUTOR_BACKEND=pthread KUPL_SCHED_POLICY=mq numactl -N 0 $INSTALL_PATH/bin/test_pthread_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_pthread_detail.xml --gtest_filter=*queue_event*
  KUPL_EXECUTOR_COUNT=1 KUPL_ENABLE_PRIORITY=1 KUPL_EXECUTOR_BACKEND=pthread KUPL_SCHED_POLICY=mq numactl -N 0 $INSTALL_PATH/bin/test_pthread_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_pthread_detail.xml --gtest_filter=test_queue_priority.*
  KUPL_EXECUTOR_COUNT=1 KUPL_ENABLE_PRIORITY=1 KUPL_EXECUTOR_BACKEND=pthread KUPL_SCHED_POLICY=static_mq numactl -N 0 $INSTALL_PATH/bin/test_pthread_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_pthread_detail.xml --gtest_filter=test_queue_priority.*

  KUPL_ENABLE_HUGEPAGES=1 KUPL_EXECUTOR_BACKEND=pthread KUPL_SCHED_POLICY=mq numactl -N 0 $INSTALL_PATH/bin/test_pthread_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_pthread_detail.xml --gtest_filter=*kupl_malloc*

  if [ $hbw_detected -eq 1 ]; then
    KUPL_EXECUTOR_BACKEND=omp KUPL_SCHED_POLICY=static_mq numactl -N 0 $INSTALL_PATH/bin/test_omp_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_detail.xml
    KUPL_EXECUTOR_BACKEND=omp KUPL_SCHED_POLICY=static_mq numactl -N 0 --membind=16 $INSTALL_PATH/bin/test_omp_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_detail.xml --gtest_filter=*kupl_malloc*
  else
    KUPL_EXECUTOR_BACKEND=omp KUPL_SCHED_POLICY=static_mq numactl -N 0 $INSTALL_PATH/bin/test_omp_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_detail.xml --gtest_filter=-*hbw*:*kupl_mem_*
  fi

  if ! [ -f "$INSTALL_PATH/bin/test_shm_main" ]; then
    echo "$INSTALL_PATH/bin/test_shm_main not exist."
    exit 0
  fi
  np_list=(1 3 4)
  bcast_algo_list=("0" "1" "2" "3" "4" "5" "6")
  allreduce_algo_list=("0" "1" "2")
  alltoall_algo_list=("0" "1" "2")
  fence_algo_list=("0" "1" "2")
  for np in "${np_list[@]}"; do
    for allreduce_algo in "${allreduce_algo_list[@]}"; do
        mpirun --allow-run-as-root -np ${np} -x KUPL_SHM_ALLREDUCE_ALGORITHM=${allreduce_algo} -x UCX_TLS=sm $INSTALL_PATH/bin/test_shm_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_shm_detail.xml
    done
    for bcast_algo in "${bcast_algo_list[@]}"; do
        mpirun --allow-run-as-root -np ${np} -x KUPL_SHM_BCAST_ALGORITHM=${bcast_algo} -x UCX_TLS=sm $INSTALL_PATH/bin/test_shm_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_shm_detail.xml
    done
    for alltoall_algo in "${alltoall_algo_list[@]}"; do
        mpirun --allow-run-as-root -np ${np} -x KUPL_SHM_ALLTOALL_ALGORITHM=${alltoall_algo} -x UCX_TLS=sm $INSTALL_PATH/bin/test_shm_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_shm_detail.xml
    done
    for fence_algo in "${fence_algo_list[@]}"; do
        mpirun --allow-run-as-root -np ${np} -x KUPL_SHM_FENCE_ALGORITHM=${fence_algo} -x UCX_TLS=sm $INSTALL_PATH/bin/test_shm_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_shm_detail.xml
    done
  done
  if [ $zcopy_detected -eq 1 ]; then
    mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x KUPL_SHM_ENABLE_HUGEPAGE=y -x UCX_TLS=sm $INSTALL_PATH/bin/test_shm_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_shm_detail.xml
    mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x KUPL_SHM_ENABLE_HUGEPAGE=y -x KUPL_ENABLE_MPOOL=y -x UCX_TLS=sm $INSTALL_PATH/bin/test_shm_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_shm_detail.xml
    mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x KUPL_SHM_ENABLE_HUGEPAGE=y -x KUPL_SHM_ON_PACKAGE=y -x UCX_TLS=sm $INSTALL_PATH/bin/test_shm_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_shm_detail.xml
    mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x KUPL_SHM_ENABLE_HUGEPAGE=y -x KUPL_ENABLE_MPOOL=y -x KUPL_SHM_ON_PACKAGE=y -x UCX_TLS=sm $INSTALL_PATH/bin/test_shm_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_shm_detail.xml
	  mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x UCX_TLS=sm $INSTALL_PATH/bin/test_shm_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_shm_detail.xml
    mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x KUPL_ENABLE_MPOOL=y -x UCX_TLS=sm $INSTALL_PATH/bin/test_shm_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_shm_detail.xml
    mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x KUPL_SHM_ON_PACKAGE=y -x UCX_TLS=sm $INSTALL_PATH/bin/test_shm_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_shm_detail.xml
    mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x KUPL_ENABLE_MPOOL=y -x KUPL_SHM_ON_PACKAGE=y -x UCX_TLS=sm $INSTALL_PATH/bin/test_shm_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_shm_detail.xml
	  mpirun --allow-run-as-root -np 4 -bind-to core -x KUPL_SHM_TYPE=sls -x KUPL_ENABLE_MPOOL=y -x UCX_TLS=sm $INSTALL_PATH/bin/test_shm_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_shm_detail.xml --gtest_filter=*kupl_comm_test_win_alloc_multiple_comm_overlap*
  fi

  if ! [ -f "$INSTALL_PATH/bin/test_mma_main" ]; then
    echo "$INSTALL_PATH/bin/test_mma_main not exist."
    exit 0
  fi
  $INSTALL_PATH/bin/test_mma_main --gtest_output=xml:$PROJ_PATH/lcov/report/test_mma_detail.xml
elif [[ "${BUILD_KIND}" == "fuzz" ]]; then
  if ! [ -f "$INSTALL_PATH/bin/fuzz_omp_main" ]; then
    echo "$INSTALL_PATH/bin/fuzz_omp_main not exist."
    exit 0
  fi
  OMP_PROC_BIND=close KUPL_LOG_LEVEL=4 KUPL_SCHED_POLICY=static_mq numactl -N 0 $INSTALL_PATH/bin/fuzz_omp_main all 30000
  KUPL_EXECUTOR_COUNT=1024 KUPL_EXECUTOR_BACKEND=pthread KUPL_SCHED_POLICY=static_mq numactl -N 0 $INSTALL_PATH/bin/fuzz_omp_main memcpy1d_async 30000
  if ! [ -f "$INSTALL_PATH/bin/fuzz_shm_main" ]; then
    echo "$INSTALL_PATH/bin/fuzz_shm_main not exist."
    exit 0
  fi
  np_list=(1 4 8 12 16)
  bcast_algo_list=("1" "2" "3" "4" "5" "6")
  allreduce_algo_list=("1" "2")
  alltoall_algo_list=("1" "2")
  fence_algo_list=("0" "1" "2")
  for np in "${np_list[@]}"; do
    mpirun --allow-run-as-root -np ${np} -x UCX_TLS=sm $INSTALL_PATH/bin/fuzz_shm_main all 30000
    for allreduce_algo in "${allreduce_algo_list[@]}"; do
        mpirun --allow-run-as-root -np ${np} -x KUPL_SHM_ALLREDUCE_ALGORITHM=${allreduce_algo} -x UCX_TLS=sm $INSTALL_PATH/bin/fuzz_shm_main allreduce 30000
    done
    for bcast_algo in "${bcast_algo_list[@]}"; do
        mpirun --allow-run-as-root -np ${np} -x KUPL_SHM_BCAST_ALGORITHM=${bcast_algo} -x UCX_TLS=sm $INSTALL_PATH/bin/fuzz_shm_main bcast 30000
    done
    for alltoall_algo in "${alltoall_algo_list[@]}"; do
        mpirun --allow-run-as-root -np ${np} -x KUPL_SHM_ALLTOALL_ALGORITHM=${alltoall_algo} -x UCX_TLS=sm $INSTALL_PATH/bin/fuzz_shm_main alltoall 30000
    done
    for fence_algo in "${fence_algo_list[@]}"; do
        mpirun --allow-run-as-root -np ${np} -x KUPL_SHM_FENCE_ALGORITHM=${fence_algo} -x UCX_TLS=sm $INSTALL_PATH/bin/fuzz_shm_main bcast 30000
    done
  done

  shm_method=("win_alloc" "win_query" "win_barrier" "allreduce" "alltoall")
  if [ $zcopy_detected -eq 1 ]; then
    for method in "${shm_method[@]}"; do
      mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x KUPL_SHM_ENABLE_HUGEPAGE=y -x UCX_TLS=sm $INSTALL_PATH/bin/fuzz_shm_main ${method} 300
      mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x KUPL_SHM_ENABLE_HUGEPAGE=y -x KUPL_ENABLE_MPOOL=y -x UCX_TLS=sm $INSTALL_PATH/bin/fuzz_shm_main ${method} 300
      mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x KUPL_SHM_ENABLE_HUGEPAGE=y -x KUPL_SHM_ON_PACKAGE=y -x UCX_TLS=sm $INSTALL_PATH/bin/fuzz_shm_main ${method} 300
      mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x KUPL_SHM_ENABLE_HUGEPAGE=y -x KUPL_ENABLE_MPOOL=y -x KUPL_SHM_ON_PACKAGE=y -x UCX_TLS=sm $INSTALL_PATH/bin/fuzz_shm_main ${method} 300
      mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x UCX_TLS=sm $INSTALL_PATH/bin/fuzz_shm_main ${method} 300
      mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x KUPL_ENABLE_MPOOL=y -x UCX_TLS=sm $INSTALL_PATH/bin/fuzz_shm_main ${method} 300
      mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x KUPL_SHM_ON_PACKAGE=y -x UCX_TLS=sm $INSTALL_PATH/bin/fuzz_shm_main ${method} 300
      mpirun --allow-run-as-root -np 1 -x KUPL_SHM_TYPE=sls -x KUPL_ENABLE_MPOOL=y -x KUPL_SHM_ON_PACKAGE=y -x UCX_TLS=sm $INSTALL_PATH/bin/fuzz_shm_main ${method} 300
      mpirun --allow-run-as-root -np 2 -x KUPL_SHM_TYPE=sls -x KUPL_ENABLE_MPOOL=y -x UCX_TLS=sm $INSTALL_PATH/bin/fuzz_shm_main win_query 300
    done
  fi

  if ! [ -f "$INSTALL_PATH/bin/fuzz_mma_main" ]; then
    echo "$INSTALL_PATH/bin/fuzz_mma_main not exist."
    exit 0
  fi
  $INSTALL_PATH/bin/fuzz_mma_main all 30000
fi

# 2. create test coverage data file
if [[ "${BUILD_COMPLIER}" == "gcc" ]]; then
  lcov --rc lcov_branch_coverage=1 -d $PROJ_PATH -c -o total.kupl.info -q
elif [[ "${BUILD_COMPLIER}" == "clang" ]]; then
  echo "Using llvm-gcov to analyze."
  lcov --gcov-tool $PROJ_PATH/llvm-gcov.sh --rc lcov_branch_coverage=1 -d $PROJ_PATH -c -o total.kupl.info -q
fi

# 3. remove files that don't need to be tracked
if [[ "${BUILD_KIND}" == "test" ]]; then
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*install*" -o total.kupl.info -q
  if [ 0 != $? ];then
    echo "Failed to remove install from coverage info"
    exit 1
  fi

  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*test*" -o total.kupl.info -q
  if [ 0 != $? ];then
    echo "Failed to remove test from coverage info"
    exit 1
  fi

  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*gtest*" -o total.kupl.info -q
  if [ 0 != $? ];then
    echo "Failed to remove gtest from coverage info"
    exit 1
  fi

  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*gcc*" -o total.kupl.info -q
  if [ 0 != $? ];then
    echo "Failed to remove gcc from coverage info"
    exit 1
  fi

  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*include/c++*" -o total.kupl.info -q
  if [ 0 != $? ];then
    echo "Failed to remove c++ from coverage info"
    exit 1
  fi

  if [ $hbw_detected -eq 0 ]; then
    lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/memory/hbw/*.cpp*" -o total.kupl.info -q
  fi

  if [ $zcopy_detected -eq 0 ]; then
    lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/memory/shm/sls/*.cpp*" -o total.kupl.info -q
    lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/memory/shm/sls/sls_kernel/*.cpp*" -o total.kupl.info -q
  fi

  if [ $sdma_detected -eq 0 ]; then
    lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/utils/thirdpart/*.cpp*" -o total.kupl.info -q
    lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/mt/kupl_event.cpp" -o total.kupl.info -q
    lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/mt/kupl_queue.cpp" -o total.kupl.info -q
  fi
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/core/*.cpp*" -o total.kupl.info -q
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/executor/kupl_executor.cpp" -o total.kupl.info -q
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/dm/memcpy/*.cpp*" -o total.kupl.info -q
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/memory/hbw/*.cpp*" -o total.kupl.info -q
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/memory/mpool/*.cpp*" -o total.kupl.info -q
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/memory/shm/*.cpp*" -o total.kupl.info -q
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/memory/shm/allreduce/*.cpp*" -o total.kupl.info -q
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/memory/shm/alltoall/*.cpp*" -o total.kupl.info -q
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/memory/shm/posix/*.cpp*" -o total.kupl.info -q
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/mt/barrier/*" -o total.kupl.info -q
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/mt/scheduler/plugin/*" -o total.kupl.info -q
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/mt/scheduler/plugin/static_mq/*.cpp*" -o total.kupl.info -q
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/mt/scheduler/plugin/mq/*.cpp*" -o total.kupl.info -q
  lcov --rc lcov_branch_coverage=1 -r total.kupl.info "*/utils/kupl_utils.cpp" -o total.kupl.info -q

elif [[ "${BUILD_KIND}" == "fuzz" ]]; then
  lcov --rc lcov_branch_coverage=1 --extract total.kupl.info '*/executor/backend/omp/*' \
                                                              '*/kupl_executor.cpp' \
                                                              '*/kupl_executor_group.cpp' \
                                                              '*/kupl_parallel_for.cpp' \
                                                              '*/kupl_graph.cpp' \
                                                              '*/kupl_static_graph.cpp' \
                                                              '*/kupl_task.cpp' \
                                                              '*/kupl_dag.*' \
                                                              '*/kupl_taskbase.h' \
                                                              '*/kupl_queue.*' \
                                                              '*/kupl_event.*' \
                                                              '*/dm/memcpy/kupl_memcpy.cpp' \
                                                              '*/memory/mpool/*.cpp' \
                                                              '*/memory/hbw/*.cpp' \
                                                              '*/mt/scheduler/kupl_sched.cpp' \
                                                              '*/mt/scheduler/plugin/static_mq/*.cpp' \
                                                              '*/mt/kupl_check.cpp' \
                                                              '*/utils/config/*' \
                                                              '*/utils/struct/*' \
                                                              '*/utils/lock/kupl_lock.*' \
                                                              '*/utils/lock/pthread_spinlock/*.cpp' \
                                                              '*/mt/barrier/kupl_barrier.cpp' \
                                                              '*/mt/barrier/kupl_barrier_dist.cpp' \
                                                              '*/memory/shm/fence/kupl_fence_linear.cpp' \
                                                              '*/memory/shm/fence/kupl_fence_peer.cpp' \
                                                              '*/shm/posix/*.cpp' \
                                                              '*/shm/allreduce/*.cpp' \
                                                              '*/shm/alltoall/*.cpp' \
                                                              '*/shm/bcast/*.cpp' \
                                                              '*/mma/kupl_mma.cpp' \
                                                              -o total.kupl.info -q
  if [ 0 != $? ];then
    echo "Failed to extract exactly from coverage info"
    exit 1
  fi
fi

# 4. generate html
genhtml --rc lcov_branch_coverage=1 -o ./report total.kupl.info --show-details --legend
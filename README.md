# KUPL

## 🔥Release Notes
- [2026/03] KUPL 项目首次上线，支持众核并行、数据管理和矩阵编程能力。

## 🚀概述

鲲鹏统一并行加速库（Kunpeng Unified Parallel Library，以下简称KUPL）提供了基于鲲鹏平台优化的并行加速基础库函数，所有接口用C/C++、汇编语言实现。本加速库提供包括底层线程管理、任务调度、线程同步、内存申请、共享内存申请、共享内存通信、矩阵编程计算等在内的基础功能，充分发挥鲲鹏处理器的硬件特性，提供高性能的基础接口。

## 📝版本配套

- 运行平台
    - 鲲鹏 920 系列
- 系统规格
    - openEuler 20.03（LTS-SP3）AArch64
    - openEuler 22.03（LTS-SP2）AArch64
    - openEuler 22.03（LTS-SP3）AArch64
    - openEuler 22.03（LTS-SP4）AArch64
    - openEuler 24.03（LTS-SP3）AArch64
    - Kylin Linux Advanced Server V10（Hydrogen）AArch64
    - Kylin Linux Advanced Server V10 （Sword）AArch64
    - Kylin Linux Advanced Server Industry V10 AArch64
    - Kylin Linux Advanced Server V10（Jasmine）
    - Kylin Linux Advanced Server V10（GFB）
    - Kylin Linux Advanced Server V11（Swan25）
    - KylinSec OS Linux 3（Qomolangma） AArch64（3.5.2）
    - KylinSec OS Linux 3（Qomolangma） AArch64（3.5.3）

## ⚡️编译安装

### 1. [获取 HPCKit 软件包](https://www.hikunpeng.com/document/detail/zh/kunpenghpcs/hpckit/instg/KunpengHPCKit_install_007.html)
#### 下载 HPCKit 软件包（HPCKit 版本号根据实际情况调整）
```
wget https://mirrors.huaweicloud.com/kunpeng/archive/HPC/HPCKit/HPCKit_26.0.RC1_Linux-aarch64.tar.gz
```

### 2. [安装 HPCKit](https://www.hikunpeng.com/document/detail/zh/kunpenghpcs/hpckit/instg/KunpengHPCKit_install_012.html)
#### 解压 HPCKit 软件包（HPCKit 版本号根据实际情况调整）
```
tar xvf HPCKit_26.0.RC1_Linux-aarch64.tar.gz
```
#### 安装 HPCKit
```
sh HPCKit_26.0.RC1_Linux-aarch64/install.sh -y --prefix=[HPCKit安装目录]
```
### 3. [设置环境变量](https://www.hikunpeng.com/document/detail/zh/kunpenghpcs/hpckit/instg/KunpengHPCKit_install_014.html)
#### 前置条件

已配置yum源。执行以下命令检查节点YUM源是否已配置，确保回显中有OS对应的YUM源。
```
yum list | grep kernel
```

已安装module工具。
执行以下命令检查是否已安装module工具。
```
yum list installed | grep environment-modules.aarch64
```
回显有如下信息说明已安装module工具。
```
environment-modules.aarch64                        5.0.1-3.oe2203sp1                @OS
```
如果未安装module工具，执行以下命令安装，并加载环境变量。
```
yum install -y environment-modules
source /etc/profile.d/modules.sh
```

#### 加载 module
```
module use [HPCKit安装目录]/HPCKit/latest/modulefiles
```

#### 加载编译器环境变量
确认您需要的编译器类型（GCC 或 Bisheng），在终端执行相应的加载命令：
- 若使用 GCC（编译器版本号根据实际情况调整）：
```
module load gcc/compiler12.3.1/gccmodule
```
- 若使用 BiSheng（编译器版本号根据实际情况调整）：
```
module load bisheng/compiler5.1.0.2/bishengmodule
```
### 4. 安装编译所需依赖

安装 cmake, numactl, numactl-devel
```
yum install cmake numactl numactl-devel
```
### 5. 编译 KUPL
克隆或者下载软件包，并进入到软件包的根目录

确认您需要的编译的类型（GCC 或 Bisheng），在终端执行相应的加载命令：
- 若使用 GCC
```
sh build.sh --install_path=[KUPL安装目录]
```
- 若使用 BiSheng
```
sh build.sh --compiler=clang --install_path=[KUPL安装目录]
```
### 6. 更多
#### 查看编译选项
```
sh build.sh --help
```
#### 编译运行测试程序(GCC)
```
module unload bisheng/hmpi26.0.RC1/release

module load gcc/hmpi26.0.RC1/release

sh build.sh --build_kind=test

sh run_lcov.sh
```
#### 编译运行测试程序(BiSheng)
需要编译并加载 Bisheng 版本的 googletest。

指定编译 googletest 的编译器为 Bisheng
```
export CC=clang
export CXX=clang++
```
参考 https://github.com/google/googletest/tree/main/googletest 编译 googletest

将 googletest 加入到环境变量中
```
export INCLUDE=[googletest安装目录]/include:$INCLUDE
export LIBRARY_PATH=[googletest安装目录]/lib64:$LIBRARY_PATH
export LD_LIBRARY_PATH=[googletest安装目录]/lib64:$LD_LIBRARY_PATH
```
编译运行测试程序

```
module unload gcc/hmpi26.0.RC1/release

module load bisheng/hmpi26.0.RC1/release

sh build.sh --build_kind=test --compiler=clang

sh run_lcov.sh test clang
```
## 📖学习教程

若您已学习编译安装，对本项目有一定认知，并希望深入了解和体验项目，请访问下述详细教程。

### 众核并行
[executor相关函数](https://www.hikunpeng.com/document/detail/zh/kunpenghpcs/hpckit/devg/KunpengHPCKit_developer_020.html): 介绍了 KUPL 中执行器相关接口，包括获取当前执行器编号、执行器总数以及细粒度控制多线程并发行为等能力。

[多线程编程函数](https://www.hikunpeng.com/document/detail/zh/kunpenghpcs/hpckit/devg/KunpengHPCKit_developer_031.html): 介绍了 KUPL 多线程编程使得一个进程中可以并发多个线程，每个线程并行执行不同的任务，进而提升性能的能力。

[计算图编程函数](https://www.hikunpeng.com/document/detail/zh/kunpenghpcs/hpckit/devg/KunpengHPCKit_developer_035.html): 介绍了 KUPL 中动态图和静态图相关编程模型。

[多队列多流编程函数](https://www.hikunpeng.com/document/detail/zh/kunpenghpcs/hpckit/devg/KunpengHPCKit_developer_048.html): 介绍了 KUPL 中多队列多流编程模型以及队列和事件相关的概念。

### 数据管理
[内存管理函数](https://www.hikunpeng.com/document/detail/zh/kunpenghpcs/hpckit/devg/KunpengHPCKit_developer_064.html): 介绍了 KUPL 提供的内存操作的相关接口，包括内存申请、拷贝及上锁等能力。

[共享内存通信函数](https://www.hikunpeng.com/document/detail/zh/kunpenghpcs/hpckit/devg/KunpengHPCKit_developer_085.html): 介绍了 KUPL 提供的共享内存底层通信接口，以及基于这些接口的集合通信函数实现。

### 矩阵编程
[矩阵编程接口函数](https://www.hikunpeng.com/document/detail/zh/kunpenghpcs/hpckit/devg/KunpengHPCKit_developer_104.html): 介绍了 KUPL 提供的矩阵编程相关接口，包括加速矩阵乘计算与拷贝等能力。

## 🔍目录结构

```
├── cmake                              # 项目工程编译目录
├── src                                # 项目源码目录
│   ├── core                           # 项目核心模块
│   ├── dm                             # 数据管理模块
│   ├── executor                       # 执行器模块
│   ├── memory                         # 内存模块
│   ├── mma                            # 矩阵编程模块
│   ├── mt                             # 众合编程模块
│   ├── tools                          # 工具模块
│   ├── utils                          # 公共基础类库
│   ├── CMakeLists.txt                 # 源码编译配置文件
│   ├── kupl_mma.h                     # 矩阵编程头文件
│   └── kupl.h                         # KUPL 头文件
├── test                               # 测试工程目录
├── build.sh                           # 项目工程编译脚本
├── CMakeLists.txt                     # 项目编译配置文件
├── LICENSE                            # LICENSE 文件
├── llvm-gcov.sh                       # llvm 覆盖率统计脚本
├── README.md                          # readme
└── run_lcov.sh                        # 安装依赖包脚本
```

## 🤝联系我们

本项目功能和文档正在持续更新和完善中，建议您关注最新版本。

- **问题反馈**：通过 [【Issues】](https://atomgit.com/kunpengcompute/kupl/issues)提交问题。
- **社区互动**：通过 [【鲲鹏论坛（HPC专区）】](https://www.hikunpeng.com/forum/forum-0187135482144798003-1.html)参与交流。
- **技术专栏**：通过 [【鲲鹏社区】](https://www.hikunpeng.com/developer/techArticles) 获取技术文章，如系列化教程、优秀实践等。

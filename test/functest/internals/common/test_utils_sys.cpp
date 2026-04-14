/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
 *
 * KUPL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *        http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "gtest/gtest.h"
#include "kupl.h"
#include "utils/type/kupl_status.h"
#include "utils/sys/kupl_dl_module.h"
#include "utils/sys/kupl_hardware.h"


TEST(test_utils_sys_inner, kupl_dl_get_default_path)
{
    const char *path1 = kupl_dl_get_default_path();
    ASSERT_TRUE(path1 != nullptr);
    const char *path2 = kupl_dl_get_default_path();
    ASSERT_TRUE(strcmp(path1, path2) == 0);
}


TEST(test_utils_sys_inner, kupl_dl_open_close)
{
    // dl open wrong
    auto dl_module = kupl_dl_open(nullptr, nullptr, 0);
    ASSERT_TRUE(dl_module == nullptr);

    const char *path = kupl_dl_get_default_path();
    std::string plugin_path = std::string(path) + "/libkupl.so";

    kupl_dl_module_sym_t symbol;
    symbol.sym_name = "kupl_parallel_for";
    dl_module = kupl_dl_open(plugin_path.c_str(), &symbol, 1);
    ASSERT_TRUE(dl_module != nullptr);

    // dl close
    kupl_dl_close(dl_module);

    // dl close wrong
    kupl_dl_close(nullptr);

    // wrong symbol
    symbol.sym_name = "hrt_parallel_for";
    dl_module = kupl_dl_open(plugin_path.c_str(), &symbol, 1);
    ASSERT_TRUE(dl_module == nullptr);

    // wrong path
    std::string wrong_plugin_path = std::string(path) + "/libkupl.xx";
    dl_module = kupl_dl_open(wrong_plugin_path.c_str(), &symbol, 1);
    ASSERT_TRUE(dl_module == nullptr);
}

TEST(test_utils_sys_inner, kupl_host_info_init)
{
    int ret = kupl_host_info_init();
    ASSERT_TRUE(ret == KUPL_OK);
}

TEST(test_utils_sys_inner, kupl_host_info_print)
{
    kupl_host_info_print();
}

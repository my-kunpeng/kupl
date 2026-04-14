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
#include <atomic>
#include "gtest/gtest.h"
#include "kupl.h"

static const int DIM_0 = 0;
static const int DIM_1 = 1;
static const int DIM_2 = 2;
static const int range_1d[] = {1, 2, 3, 4, 6, 8, 9, 17, 18, 20, 32, 67, 83, 128, 256, 512, 1024, 1033, 3072, 4096};
static const int range_2d[][2] = {{1, 1}, {1, 3}, {1, 4}, {1, 5}, {1, 1473}, {2, 2}, {2, 3}, {2, 4}, {2, 5},
                                  {2, 7343}, {3, 2}, {3, 3}, {3, 4}, {3, 5}, {4, 2}, {4, 3}, {4, 4}, {4, 5},
                                  {4, 6}, {5, 4}, {5, 5}, {5, 6}, {5, 7}, {6, 9}, {6, 16}, {7, 7}, {7, 8}, {7, 11},
                                  {13, 31}, {16, 16}, {16, 17}, {19, 11}, {21, 4}, {23, 21}, {25, 11}, {25, 25},
                                  {26, 27}, {28, 53}, {31, 17}, {32, 32}, {32, 37}, {35, 35}, {37, 4},
                                  {217, 217}, {54, 333}};
static const int range_3d[][3] = {{1, 3, 7}, {1, 5, 7}, {2, 2, 5}, {2, 3, 5}, {2, 3, 7}, {2, 4, 5}, {2, 4, 7},
                                  {2, 5, 7}, {3, 2, 5}, {3, 3, 3}, {3, 3, 5}, {3, 4, 5}, {3, 3, 7}, {3, 4, 7},
                                  {3, 5, 7}, {3, 7, 8}, {3, 9, 28}, {4, 2, 5}, {4, 3, 5}, {4, 4, 4}, {4, 4, 5},
                                  {8, 8, 8}, {8, 34, 5}, {9, 9, 9}, {3, 9, 28}, {11, 11, 11}, {11, 13, 17},
                                  {16, 16, 16}, {20, 15, 10}, {21, 18, 11}, {32, 64, 4}, {55, 4, 28}, {64, 64, 64}};
static const int blocksizes[] = {1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 16, 17, 23, 24};

class test_taskloop : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        graph_ = kupl_graph_create(nullptr);
        int num_executors = kupl_get_num_executors();

        int exe[num_executors];
        for (int i = 0; i < num_executors; i++) {
            exe[i] = i;
        }
        egroup_ = kupl_egroup_create(exe, num_executors);
    }
    static void TearDownTestCase()
    {
        kupl_graph_destroy(graph_);
        kupl_egroup_destroy(egroup_);
    }
    virtual void SetUp() {}
    virtual void TearDown() {}
    static kupl_graph_h graph_;
    static kupl_egroup_h egroup_;
};

kupl_graph_h test_taskloop::graph_;
kupl_egroup_h test_taskloop::egroup_;

static void loop_task(kupl_nd_range_t *nd_range, void *args)
{
    std::atomic<int> *count = (std::atomic<int> *)args;
    int total = 1;
    kupl_range_t *range = nd_range->nd_range;
    for (int d = 0; d < nd_range->dim; d++) {
        int size = range[d].upper - range[d].lower;
        if (size > 0) {
            total *= size;
        }
    }
    count->fetch_add(total);
}

TEST_F(test_taskloop, invalid_info)
{
    int ret = kupl_graph_submit(graph_, nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);

    kupl_task_info_t info = {
        .type = KUPL_TASK_TYPE_TASKLOOP,
        .desc = nullptr,
    };
    ret = kupl_graph_submit(graph_, &info);
    ASSERT_TRUE(ret == KUPL_ERROR);

    ret = kupl_graph_submit(nullptr, &info);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_taskloop, invalid_desc)
{
    std::atomic<int> count = {0};
    const int loop_tasks = 8;
    kupl_nd_range_t range;
    KUPL_1D_RANGE_INIT(range, 0, loop_tasks);
    // invalid egroup
    kupl_taskloop_desc_t desc = {
        .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
        .func = loop_task,
        .args = &count,
        .range = &range,
        .egroup = nullptr,
    };
    kupl_task_info_t info = {
        .type = KUPL_TASK_TYPE_TASKLOOP,
        .desc = &desc,
    };
    int ret = kupl_graph_submit(graph_, &info);
    ASSERT_TRUE(ret == KUPL_ERROR);

    // invalid func
    desc = {
        .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
        .func = nullptr,
        .args = &count,
        .range = &range,
        .egroup = egroup_,
    };
    ret = kupl_graph_submit(graph_, &info);
    ASSERT_TRUE(ret == KUPL_ERROR);

    // invalid range
    desc = {
        .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
        .func = loop_task,
        .args = &count,
        .range = nullptr,
        .egroup = egroup_,
    };
    ret = kupl_graph_submit(graph_, &info);
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_taskloop, invalid_desc_lambda)
{
    std::atomic<int> count = {0};
    const int loop_tasks = 8;
    kupl_nd_range_t range;
    KUPL_1D_RANGE_INIT(range, 0, loop_tasks);
    // invalid egroup
    kupl_taskloop_desc_t desc = {
        .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
        .range = &range,
        .egroup = nullptr,
    };
    int ret = kupl::graph_submit(graph_, &desc, [](const kupl_nd_range_t *nd_range) {});
    ASSERT_TRUE(ret == KUPL_ERROR);

    // invalid func
    desc = {
        .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
        .range = &range,
        .egroup = egroup_,
    };
    ret = kupl::graph_submit(graph_, &desc, nullptr);
    ASSERT_TRUE(ret == KUPL_ERROR);

    // invalid range
    desc = {
        .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
        .range = nullptr,
        .egroup = egroup_,
    };
    ret = kupl::graph_submit(graph_, &desc, [](const kupl_nd_range_t *nd_range) {});
    ASSERT_TRUE(ret == KUPL_ERROR);
}

TEST_F(test_taskloop, taskloop_1d)
{
    std::atomic<int> count = {0};
    kupl_nd_range_t range;
    kupl_taskloop_desc_t desc = {
        .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
        .func = loop_task,
        .args = &count,
        .range = &range,
        .egroup = egroup_
    };
    kupl_task_info_t info = {
        .type = KUPL_TASK_TYPE_TASKLOOP,
        .desc = &desc,
    };
    int range_1d_len = sizeof(range_1d) / sizeof(int);
    int blocksizes_len = sizeof(blocksizes) / sizeof(int);
    for (int i = 0; i < range_1d_len; i++) {
        for (int j = 0; j < blocksizes_len; j++) {
            count.store(0);
            KUPL_STRIDE_1D_RANGE_INIT(range, 0, range_1d[i], 1, blocksizes[j]);
            int ret = kupl_graph_submit(graph_, &info);
            ASSERT_TRUE(ret == KUPL_OK);
            kupl_graph_wait(graph_);
            ASSERT_TRUE(count.load() == range_1d[i]);
        }
    }
}

TEST_F(test_taskloop, taskloop_1d_lambda)
{
    std::atomic<int> count = {0};
    kupl_nd_range_t range;
    kupl_taskloop_desc_t desc = {
        .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
        .range = &range,
        .egroup = egroup_
    };
    int range_1d_len = sizeof(range_1d) / sizeof(int);
    int blocksizes_len = sizeof(blocksizes) / sizeof(int);
    for (int i = 0; i < range_1d_len; i++) {
        for (int j = 0; j < blocksizes_len; j++) {
            count.store(0);
            KUPL_STRIDE_1D_RANGE_INIT(range, 0, range_1d[i], 1, blocksizes[j]);
            int ret = kupl::graph_submit(graph_, &desc, [&](const kupl_nd_range_t *nd_range) {
                                        int total = 1;
                                        kupl_range_t *range = (kupl_range_t *)nd_range->nd_range;
                                        for (int d = 0; d < nd_range->dim; d++) {
                                            int size = range[d].upper - range[d].lower;
                                            if (size > 0) {
                                                total *= size;
                                            }
                                        }
                                        count.fetch_add(total);
                                    });
            ASSERT_TRUE(ret == KUPL_OK);
            kupl_graph_wait(graph_);
            ASSERT_TRUE(count.load() == range_1d[i]);
        }
    }
}

TEST_F(test_taskloop, taskloop_2d)
{
    std::atomic<int> count = {0};
    kupl_nd_range_t range;
    kupl_taskloop_desc_t desc = {
        .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
        .func = loop_task,
        .args = &count,
        .range = &range,
        .egroup = egroup_
    };
    kupl_task_info_t info = {
        .type = KUPL_TASK_TYPE_TASKLOOP,
        .desc = &desc,
    };
    int range_2d_len = sizeof(range_2d) / sizeof(int[2]);
    int blocksizes_len = sizeof(blocksizes) / sizeof(int);
    for (int i = 0; i < range_2d_len; i++) {
        for (int j = 0; j < blocksizes_len; j++) {
            count.store(0);
            KUPL_STRIDE_2D_RANGE_INIT(range, 0, range_2d[i][0], 1, blocksizes[j], 0, range_2d[i][1], 1, blocksizes[j]);
            int ret = kupl_graph_submit(graph_, &info);
            ASSERT_TRUE(ret == KUPL_OK);
            kupl_graph_wait(graph_);
            ASSERT_TRUE(count.load() == range_2d[i][0] * range_2d[i][1]);
        }
    }
}

TEST_F(test_taskloop, taskloop_2d_lambda)
{
    std::atomic<int> count = {0};
    kupl_nd_range_t range;
    kupl_taskloop_desc_t desc = {
        .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
        .range = &range,
        .egroup = egroup_
    };
    int range_2d_len = sizeof(range_2d) / sizeof(int[2]);
    int blocksizes_len = sizeof(blocksizes) / sizeof(int);
    for (int i = 0; i < range_2d_len; i++) {
        for (int j = 0; j < blocksizes_len; j++) {
            count.store(0);
            KUPL_STRIDE_2D_RANGE_INIT(range, 0, range_2d[i][0], 1, blocksizes[j], 0, range_2d[i][1], 1, blocksizes[j]);
            int ret = kupl::graph_submit(graph_, &desc, [&](const kupl_nd_range_t *nd_range) {
                                        int total = 1;
                                        kupl_range_t *range = (kupl_range_t *)nd_range->nd_range;
                                        for (int d = 0; d < nd_range->dim; d++) {
                                            int size = range[d].upper - range[d].lower;
                                            if (size > 0) {
                                                total *= size;
                                            }
                                        }
                                        count.fetch_add(total);
                                    });
            ASSERT_TRUE(ret == KUPL_OK);
            kupl_graph_wait(graph_);
            ASSERT_TRUE(count.load() == range_2d[i][0] * range_2d[i][1]);
        }
    }
}

TEST_F(test_taskloop, taskloop_3d)
{
    std::atomic<int> count = {0};
    kupl_nd_range_t range;
    kupl_taskloop_desc_t desc = {
        .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
        .func = loop_task,
        .args = &count,
        .range = &range,
        .egroup = egroup_
    };
    kupl_task_info_t info = {
        .type = KUPL_TASK_TYPE_TASKLOOP,
        .desc = &desc,
    };
    int range_3d_len = sizeof(range_3d) / sizeof(int[3]);
    int blocksizes_len = sizeof(blocksizes) / sizeof(int);
    for (int i = 0; i < range_3d_len; i++) {
        for (int j = 0; j < blocksizes_len; j++) {
            count.store(0);
            KUPL_STRIDE_3D_RANGE_INIT(range, 0, range_3d[i][DIM_0], 1, blocksizes[j],
                               0, range_3d[i][DIM_1], 1, blocksizes[j],
                               0, range_3d[i][DIM_2], 1, blocksizes[j]);
            int ret = kupl_graph_submit(graph_, &info);
            ASSERT_TRUE(ret == KUPL_OK);
            kupl_graph_wait(graph_);
            ASSERT_TRUE(count.load() == range_3d[i][0] * range_3d[i][DIM_1] * range_3d[i][DIM_2]);
        }
    }
}

TEST_F(test_taskloop, taskloop_3d_lambda)
{
    std::atomic<int> count = {0};
    kupl_nd_range_t range;
    kupl_taskloop_desc_t desc = {
        .field_mask = KUPL_TASKLOOP_DESC_FIELD_DEFAULT,
        .range = &range,
        .egroup = egroup_
    };
    int range_3d_len = sizeof(range_3d) / sizeof(int[3]);
    int blocksizes_len = sizeof(blocksizes) / sizeof(int);
    for (int i = 0; i < range_3d_len; i++) {
        for (int j = 0; j < blocksizes_len; j++) {
            count.store(0);
            KUPL_STRIDE_3D_RANGE_INIT(range, 0, range_3d[i][DIM_0], 1, blocksizes[j],
                               0, range_3d[i][DIM_1], 1, blocksizes[j],
                               0, range_3d[i][DIM_2], 1, blocksizes[j]);
            int ret = kupl::graph_submit(graph_, &desc, [&](const kupl_nd_range_t *nd_range) {
                                        int total = 1;
                                        kupl_range_t *range = (kupl_range_t *)nd_range->nd_range;
                                        for (int d = 0; d < nd_range->dim; d++) {
                                            int size = range[d].upper - range[d].lower;
                                            if (size > 0) {
                                                total *= size;
                                            }
                                        }
                                        count.fetch_add(total);
                                    });
            ASSERT_TRUE(ret == KUPL_OK);
            kupl_graph_wait(graph_);
            ASSERT_TRUE(count.load() == range_3d[i][0] * range_3d[i][DIM_1] * range_3d[i][DIM_2]);
        }
    }
}
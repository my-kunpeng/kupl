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
#ifndef KUPL_VLA_H
#define KUPL_VLA_H

template <typename T, const unsigned THRESHOLD = 128>
class kupl_vla {
public:
    explicit kupl_vla(size_t size)
    {
        if (size <= THRESHOLD) {
            data = stackData;
            isHeap = false;
        } else {
            data = new (std::nothrow)T[size];
            isHeap = true;
        }
    }

    ~kupl_vla()
    {
        if (isHeap) {
            delete[] data;
        }
    }

    kupl_vla(const kupl_vla&) = delete;
    kupl_vla& operator=(const kupl_vla&) = delete;

    T& operator[](size_t index)
    {
        return data[index];
    }

    T& operator[](int index)
    {
        return data[index];
    }

    T& operator[](uint32_t index)
    {
        return data[index];
    }

    const T& operator[](size_t index) const
    {
        return data[index];
    }

    T *get_data() const
    {
        return data;
    }
private:
    T* data = nullptr;
    T stackData[THRESHOLD] = {};
    bool isHeap = true;
};


#endif
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

#ifndef KUPL_MMA
#define KUPL_MMA

#ifdef __cplusplus

#include <cstddef>
#include <tuple>
#include <type_traits>

/** @brief exposed kupl symbol table */
#define kupl_export __attribute__((visibility("default")))

#if defined(__clang__)

#define MMA_INOUT
#define MMA_IN

#elif defined(__GNUC__)

#include <arm_sve.h>
#include <arm_sme.h>
#define MMA_INOUT __arm_streaming __arm_inout("za")
#define MMA_IN __arm_streaming __arm_in("za")

#endif

namespace kupl {

namespace tensor {

namespace detail {
template <int idx, typename... Args>
class GetArgFromArgs;

template <int idx, typename T, typename... Rest>
class GetArgFromArgs<idx, T, Rest...> {
public:
    using type = typename GetArgFromArgs<idx - 1, Rest...>::type;
};

template <typename T, typename... Rest>
class GetArgFromArgs<0, T, Rest...> {
public:
    using type = T;
};
} // namespace detail

template <typename T, T V>
class Val {
public:
    static constexpr T val = V;
};

template <int V>
class Int : public Val<int, V> {};

class Underscore : public Int<-1> {};

template <int V>
class Ops : public Val<int, V> {};

template <typename... Args>
class kupl_tuple {
public:
    using tuple_type = std::tuple<Args...>;

    kupl_tuple(Args&... args) : data_(args...) {}

    template<typename... AA>
    kupl_tuple(AA&&... args) : data_(std::forward<AA>(args)...) {}

    static constexpr size_t size_v()
    {
        return size_;
    }

    template<size_t Index>
    constexpr auto& get()
    {
        static_assert(Index < size_, "Index out of buonds");
        return std::template get<Index>(data_);
    }

    template<size_t Index>
    constexpr const auto& get() const
    {
        static_assert(Index < size_, "Index out of buonds");
        return std::template get<Index>(data_);
    }

private:
    std::tuple<Args...> data_;
    static constexpr size_t size_ = sizeof...(Args);
};

template <typename... Args>
class Coord : public kupl_tuple<Args...> {
public:
    Coord(Args... args) : kupl_tuple<Args...>(args...) {}
};

template <typename... Args>
class Shape : public kupl_tuple<Args...> {
public:
    Shape(Args... args) : kupl_tuple<Args...>(args...) {}
};

template <typename... Args>
class Stride : public kupl_tuple<Args...> {
public:
    Stride(Args... args) : kupl_tuple<Args...>(args...) {}
};

template <typename T, typename Tuple>
struct tuple_contains : std::false_type {};

template <typename T, typename... Args>
struct tuple_contains<T, kupl_tuple<Args...>>
    : std::disjunction<std::is_same<T, Args>...> {};

template <typename T, typename... Args>
struct tuple_contains<T, Coord<Args...>>
    : std::disjunction<std::is_same<T, Args>...> {};

template <typename T, typename Tuple>
constexpr bool has_elem = tuple_contains<T, Tuple>::value;

template <typename Tuple>
constexpr bool has_underscore = has_elem<Underscore, Tuple>;

template<typename Coord, typename Shape, typename Stride>
constexpr auto crd2idx(Coord coord, Shape shape, Stride stride);

template<typename Coord, typename Layout>
constexpr auto slice_and_offset(Coord coord, Layout layout);

template <typename Shape, typename Stride>
class Layout {
public:
    Layout(Shape shape, Stride stride) : shape_(shape), stride_(stride) {}

    Shape shape()
    {
        return shape_;
    }

    Stride stride()
    {
        return stride_;
    }

    template <class Coord>
    auto operator()(Coord const &coord) {
        static_assert(!has_underscore<Coord>, "layout() not support slice now");
        return crd2idx(coord, shape_, stride_);
    }

private:
    Shape shape_;
    Stride stride_;
};

template <typename dtype, typename Layout>
class Tensor {
public:
    Tensor(dtype *ptr, Layout layout) : ptr_(ptr), layout_(layout) {}
    ~Tensor()
    {
        ptr_ = nullptr;
    }

    inline dtype *get_ptr() const
    {
        return ptr_;
    }

    template <class Coord>
    decltype(auto) operator()(Coord const &coord) {
        if constexpr (has_underscore<Coord>) {
            auto [sliced_layout, offset] = slice_and_offset(coord, layout_);
            return Tensor<dtype, decltype(sliced_layout)>{ptr_ + offset, sliced_layout};
        } else {
            return ptr_[layout_(coord)];
        }
    }

private:
    dtype *ptr_;
    Layout layout_;
};

template <typename... Args>
Shape<Args...> make_shape(Args&&... args);

template<size_t I = 0, typename Coord, typename Shape, typename... Collected>
constexpr auto slice_shape_impl(const Coord &coord, const Shape &shape, Collected&&... collected)
{
    constexpr size_t size = Coord::size_v();
    if constexpr (I >= size) {
        return make_shape(std::forward<Collected>(collected)...);
    } else {
        if constexpr (std::is_same_v<std::decay_t<decltype(coord.template get<I>())>, Underscore>) {
            return slice_shape_impl<I + 1>(coord, shape, std::forward<Collected>(collected)..., shape.template get<I>());
        } else {
            return slice_shape_impl<I + 1>(coord, shape, std::forward<Collected>(collected)...);
        }
    }
}

template<typename Coord, typename Shape>
constexpr auto slice_shape(Coord coord, Shape shape)
{
    static_assert(Coord::size_v() == Shape::size_v(), "slice must have the same size");
    return slice_shape_impl(coord, shape);
}

template <typename... Args>
Stride<Args...> make_stride(Args&&... args);

template<size_t I = 0, typename Coord, typename Stride, typename... Collected>
constexpr auto slice_stride_impl(const Coord &coord, const Stride &stride, Collected&&... collected)
{
    constexpr size_t size = Coord::size_v();
    static_assert(size == 2, "slice stride size == 2");
    if constexpr (I >= size) {
        return make_stride(std::forward<Collected>(collected)...);
    } else {
        if constexpr (std::is_same_v<std::decay_t<decltype(coord.template get<I>())>, Underscore>) {
            return slice_stride_impl<I + 1>(coord, stride, std::forward<Collected>(collected)..., stride.template get<I>());
        } else {
            return slice_stride_impl<I + 1>(coord, stride, std::forward<Collected>(collected)...);
        }
    }
}

template<typename Coord, typename Stride>
constexpr auto slice_stride(Coord coord, Stride stride)
{
    static_assert(Coord::size_v() == Stride::size_v(), "slice must have the same size");
    return slice_stride_impl(coord, stride);
}

template<typename Coord, typename Shape, typename Stride, size_t... I>
constexpr auto crd2idx_impl(const Coord &coord, const Shape &shape, const Stride &stride, std::index_sequence<I...>)
{
    static_assert(((std::remove_reference_t<decltype(coord.template get<I>())>::val < std::remove_reference_t<decltype(shape.template get<I>())>::val) && ...), "Coord must less than Shape with the same index");
    return (((std::is_same_v<std::decay_t<decltype(coord.template get<I>())>, Underscore> ? 0 : std::remove_reference_t<decltype(coord.template get<I>())>::val) * std::remove_reference_t<decltype(stride.template get<I>())>::val) + ...);
}

template<typename Coord, typename Shape, typename Stride>
constexpr auto crd2idx(Coord coord, Shape shape, Stride stride)
{
    static_assert(coord.size_v() == shape.size_v(), "Coord & Shape must have the same size");
    static_assert(coord.size_v() == stride.size_v(), "Coord & Stride must have the same size");
    return crd2idx_impl(coord, shape, stride, std::make_index_sequence<coord.size_v()>{});
}

template<typename Coord, typename Layout>
constexpr auto slice_and_offset(Coord coord, Layout layout)
{
    auto sliced_shape = slice_shape(coord, layout.shape());
    auto sliced_stride = slice_stride(coord, layout.stride());
    auto sliced_idx = crd2idx(coord, layout.shape(), layout.stride());
    return std::make_pair(make_layout(sliced_shape, sliced_stride), sliced_idx);
}

typedef enum mma_atom {
    MMA_32x16x1_F64F64F64 = 0,
    MMA_32x16x512_F64F64F64,
    MMA_16x64x2_BF16BF16F32,
    MMA_16x64x1_BF16BF16F32,
    MMA_16x64x4_INT8INT8INT32,
    MMA_32x32x4_INT8INT8INT32
} mma_atom_t;

template <typename MmaAtom, typename Shape>
class TiledMma {
public:
    template <typename TiledMma, typename dtypeD, typename LayoutD, typename dtypeA, typename LayoutA, typename dtypeB,
              typename LayoutB, typename dtypeC, typename LayoutC>
    friend void tensor_tiled_mma(TiledMma mma, Tensor<dtypeD, LayoutD> D, Tensor<dtypeA, LayoutA> A,
                                 Tensor<dtypeB, LayoutB> B, Tensor<dtypeC, LayoutC> C);

private:
    template <typename dtypeD, typename LayoutD, typename dtypeA, typename LayoutA, typename dtypeB, typename LayoutB,
              typename dtypeC, typename LayoutC>
    void call(Tensor<dtypeD, LayoutD> &D, const Tensor<dtypeA, LayoutA> &A, const Tensor<dtypeB, LayoutB> &B,
              const Tensor<dtypeC, LayoutC> &C) MMA_INOUT;
};

typedef enum store_atom {
    STORE_32x16_F64 = 0,
    STORE_16x64_F32,
    STORE_16x64_INT32,
    STORE_32x32_INT32
} store_atom_t;

template <typename StoreAtom, typename Shape>
class TiledStore {
public:
    template <typename TiledStore, typename dtype, typename Layout>
    friend void tensor_tiled_store(TiledStore store, Tensor<dtype, Layout> tensor);

private:
    template <typename dtype, typename Layout>
    void call(Tensor<dtype, Layout> &tensor) MMA_IN;
};

typedef enum copy_atom {
    COPY_32x1_F64_RM2CM = 0,
    COPY_1x16_F64_CM2RM,
    COPY_16x2_BF16_RM2ZZ,
    COPY_2x64_BF16_CM2NN,
    COPY_16x1_BF16_RM2CM,
    COPY_1x64_BF16_CM2RM,
    COPY_16x4_INT8_RM2ZZ,
    COPY_4x64_INT8_CM2NN,
    COPY_32x4_INT8_RM2ZZ,
    COPY_4x32_INT8_CM2NN,
    KP36_PREFETCH_L1,
    KP36_PREFETCH_L2
} copy_atom_t;

template <typename CopyAtom, typename Shape>
class TiledCopy {
public:
    template <typename TiledCopy, typename dtypeD, typename LayoutD, typename dtypeS, typename LayoutS>
    friend void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeD, LayoutD> dst, Tensor<dtypeS, LayoutS> src);

    template <typename TiledCopy, typename dtypeS, typename LayoutS>
    friend void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeS, LayoutS> src);

private:
    template <typename dtypeD, typename LayoutD, typename dtypeS, typename LayoutS>
    void call(Tensor<dtypeD, LayoutD> &dst, const Tensor<dtypeS, LayoutS> &src) MMA_IN;

    template <typename dtypeS, typename LayoutS>
    void call(Tensor<dtypeS, LayoutS> &src);
};

template <typename... Args>
kupl_export Coord<Args...> make_coord(Args&&... args)
{
    return Coord<Args...>{std::forward<Args>(args)...};
}

template <typename... Args>
kupl_export Shape<Args...> make_shape(Args&&... args)
{
    return Shape<Args...>{std::forward<Args>(args)...};
}

template <typename... Args>
kupl_export Stride<Args...> make_stride(Args&&... args)
{
    return Stride<Args...>{std::forward<Args>(args)...};
}

template <typename Shape, typename Stride>
kupl_export Layout<Shape, Stride> make_layout(Shape shape, Stride stride)
{
    return Layout<Shape, Stride>{shape, stride};
}

template <typename dtype, typename Layout>
kupl_export Tensor<dtype, Layout> make_tensor(dtype *ptr, Layout layout)
{
    return Tensor<dtype, Layout>{ptr, layout};
}

template <typename MmaAtom, typename Shape>
kupl_export TiledMma<MmaAtom, Shape> make_tiled_mma([[maybe_unused]] MmaAtom mma_atom,
                                                    [[maybe_unused]] Shape atom_shape)
{
    return TiledMma<MmaAtom, Shape>{};
}

template <typename TiledMma, typename dtypeD, typename LayoutD, typename dtypeA, typename LayoutA, typename dtypeB,
          typename LayoutB, typename dtypeC, typename LayoutC>
kupl_export void tensor_tiled_mma(TiledMma mma, Tensor<dtypeD, LayoutD> D, Tensor<dtypeA, LayoutA> A,
                                  Tensor<dtypeB, LayoutB> B, Tensor<dtypeC, LayoutC> C) MMA_INOUT
{
    mma.call(D, A, B, C);
}

template <typename StoreAtom, typename Shape>
kupl_export TiledStore<StoreAtom, Shape> make_tiled_store([[maybe_unused]] StoreAtom store_atom,
                                                          [[maybe_unused]] Shape atom_shape)
{
    return TiledStore<StoreAtom, Shape>{};
}

template <typename TiledStore, typename dtype, typename Layout>
kupl_export void tensor_tiled_store(TiledStore store, Tensor<dtype, Layout> tensor) MMA_IN
{
    store.call(tensor);
}

template <typename CopyAtom, typename Shape>
kupl_export TiledCopy<CopyAtom, Shape> make_tiled_copy([[maybe_unused]] CopyAtom copy_atom,
                                                       [[maybe_unused]] Shape atom_shape)
{
    return TiledCopy<CopyAtom, Shape>{};
}

template <typename TiledCopy, typename dtypeD, typename LayoutD, typename dtypeS, typename LayoutS>
kupl_export void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeD, LayoutD> dst, Tensor<dtypeS, LayoutS> src) MMA_IN
{
    copy.call(dst, src);
}

template <typename TiledCopy, typename dtypeS, typename LayoutS>
kupl_export void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeS, LayoutS> src)
{
    copy.call(src);
}

class TiledCallFunc {
public:
    template <typename MmaAtom, typename Shape>
    friend class TiledMma;
    template <typename StoreAtom, typename Shape>
    friend class TiledStore;
    template <typename CopyAtom, typename Shape>
    friend class TiledCopy;

private:
    template <int size_m, int size_n, typename StrideA, typename StrideB, typename StrideC, typename dtypeA,
              typename dtypeB, typename dtypeC>
    static kupl_export void call_mma(dtypeA *A, dtypeB *B, dtypeC *C, int size_k) MMA_INOUT;

    template <int size_m, int size_n, typename StrideD, typename dtypeD>
    static kupl_export void call_store(dtypeD *data) MMA_IN;

    template <typename CopyAtom, typename dtypeD, typename dtypeS>
    static kupl_export void call_copy(dtypeD *dst, dtypeS *src, int size_m, int size_n) MMA_IN;

    template <typename CopyAtom, typename dtypeS>
    static kupl_export void call_copy(dtypeS *data, int size);
};

static constexpr int M_1 = 1;
static constexpr int M_2 = 2;
static constexpr int M_4 = 4;
static constexpr int M_16 = 16;
static constexpr int M_32 = 32;
static constexpr int N_1 = 1;
static constexpr int N_2 = 2;
static constexpr int N_4 = 4;
static constexpr int N_16 = 16;
static constexpr int N_32 = 32;
static constexpr int N_64 = 64;
static const int K_512 = 512;
static const int K_2 = 2;
static const int K_4 = 4;

template <int AtomShapeM, int AtomShapeN, int AtomShapeK>
class TiledMma<Ops<MMA_32x16x512_F64F64F64>, Shape<Int<AtomShapeM>, Int<AtomShapeN>, Int<AtomShapeK>>> {
public:
    template <typename TiledMma, typename dtypeD, typename LayoutD, typename dtypeA, typename LayoutA, typename dtypeB,
              typename LayoutB, typename dtypeC, typename LayoutC>
    friend void tensor_tiled_mma(TiledMma mma, Tensor<dtypeD, LayoutD> D, Tensor<dtypeA, LayoutA> A,
                                 Tensor<dtypeB, LayoutB> B, Tensor<dtypeC, LayoutC> C);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeA, typename ShapeA, typename StrideA,
              typename dtypeB, typename ShapeB, typename StrideB, typename dtypeC, typename ShapeC, typename StrideC>
    void call([[maybe_unused]] Tensor<dtypeD, Layout<ShapeD, StrideD>> &D,
              const Tensor<dtypeA, Layout<ShapeA, StrideA>> &A, const Tensor<dtypeB, Layout<ShapeB, StrideB>> &B,
              const Tensor<dtypeC, Layout<ShapeC, StrideC>> &C) MMA_INOUT
    {
        TiledCallFunc::call_mma<M_32 * AtomShapeM, N_16 * AtomShapeN, StrideA, StrideB, StrideC, dtypeA, dtypeB,
                                dtypeC>(A.get_ptr(), B.get_ptr(), C.get_ptr(), AtomShapeK * K_512);
    }
};

template <int AtomShapeM, int AtomShapeN, int AtomShapeK>
class TiledMma<Ops<MMA_32x16x1_F64F64F64>, Shape<Int<AtomShapeM>, Int<AtomShapeN>, Int<AtomShapeK>>> {
public:
    template <typename TiledMma, typename dtypeD, typename LayoutD, typename dtypeA, typename LayoutA, typename dtypeB,
              typename LayoutB, typename dtypeC, typename LayoutC>
    friend void tensor_tiled_mma(TiledMma mma, Tensor<dtypeD, LayoutD> D, Tensor<dtypeA, LayoutA> A,
                                 Tensor<dtypeB, LayoutB> B, Tensor<dtypeC, LayoutC> C);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeA, typename ShapeA, typename StrideA,
              typename dtypeB, typename ShapeB, typename StrideB, typename dtypeC, typename ShapeC, typename StrideC>
    void call([[maybe_unused]] Tensor<dtypeD, Layout<ShapeD, StrideD>> &D,
              const Tensor<dtypeA, Layout<ShapeA, StrideA>> &A, const Tensor<dtypeB, Layout<ShapeB, StrideB>> &B,
              const Tensor<dtypeC, Layout<ShapeC, StrideC>> &C) MMA_INOUT
    {
        TiledCallFunc::call_mma<M_32 * AtomShapeM, N_16 * AtomShapeN, StrideA, StrideB, StrideC, dtypeA, dtypeB,
                                dtypeC>(A.get_ptr(), B.get_ptr(), C.get_ptr(), AtomShapeK);
    }
};

template <int AtomShapeM, int AtomShapeN, int AtomShapeK>
class TiledMma<Ops<MMA_16x64x2_BF16BF16F32>, Shape<Int<AtomShapeM>, Int<AtomShapeN>, Int<AtomShapeK>>> {
public:
    template <typename TiledMma, typename dtypeD, typename LayoutD, typename dtypeA, typename LayoutA, typename dtypeB,
              typename LayoutB, typename dtypeC, typename LayoutC>
    friend void tensor_tiled_mma(TiledMma mma, Tensor<dtypeD, LayoutD> D, Tensor<dtypeA, LayoutA> A,
                                 Tensor<dtypeB, LayoutB> B, Tensor<dtypeC, LayoutC> C);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeA, typename ShapeA, typename StrideA,
              typename dtypeB, typename ShapeB, typename StrideB, typename dtypeC, typename ShapeC, typename StrideC>
    void call([[maybe_unused]] Tensor<dtypeD, Layout<ShapeD, StrideD>> &D,
              const Tensor<dtypeA, Layout<ShapeA, StrideA>> &A, const Tensor<dtypeB, Layout<ShapeB, StrideB>> &B,
              const Tensor<dtypeC, Layout<ShapeC, StrideC>> &C) MMA_INOUT
    {
        TiledCallFunc::call_mma<M_16 * AtomShapeM, N_64 * AtomShapeN, StrideA, StrideB, StrideC, dtypeA, dtypeB,
                                dtypeC>(A.get_ptr(), B.get_ptr(), C.get_ptr(), K_2 * AtomShapeK);
    }
};

template <int AtomShapeM, int AtomShapeN, int AtomShapeK>
class TiledMma<Ops<MMA_16x64x1_BF16BF16F32>, Shape<Int<AtomShapeM>, Int<AtomShapeN>, Int<AtomShapeK>>> {
public:
    template <typename TiledMma, typename dtypeD, typename LayoutD, typename dtypeA, typename LayoutA, typename dtypeB,
              typename LayoutB, typename dtypeC, typename LayoutC>
    friend void tensor_tiled_mma(TiledMma mma, Tensor<dtypeD, LayoutD> D, Tensor<dtypeA, LayoutA> A,
                                 Tensor<dtypeB, LayoutB> B, Tensor<dtypeC, LayoutC> C);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeA, typename ShapeA, typename StrideA,
              typename dtypeB, typename ShapeB, typename StrideB, typename dtypeC, typename ShapeC, typename StrideC>
    void call([[maybe_unused]] Tensor<dtypeD, Layout<ShapeD, StrideD>> &D,
              const Tensor<dtypeA, Layout<ShapeA, StrideA>> &A, const Tensor<dtypeB, Layout<ShapeB, StrideB>> &B,
              const Tensor<dtypeC, Layout<ShapeC, StrideC>> &C) MMA_INOUT
    {
        TiledCallFunc::call_mma<M_16 * AtomShapeM, N_64 * AtomShapeN, StrideA, StrideB, StrideC, dtypeA, dtypeB,
                                dtypeC>(A.get_ptr(), B.get_ptr(), C.get_ptr(), AtomShapeK);
    }
};

template <int AtomShapeM, int AtomShapeN, int AtomShapeK>
class TiledMma<Ops<MMA_16x64x4_INT8INT8INT32>, Shape<Int<AtomShapeM>, Int<AtomShapeN>, Int<AtomShapeK>>> {
public:
    template <typename TiledMma, typename dtypeD, typename LayoutD, typename dtypeA, typename LayoutA, typename dtypeB,
              typename LayoutB, typename dtypeC, typename LayoutC>
    friend void tensor_tiled_mma(TiledMma mma, Tensor<dtypeD, LayoutD> D, Tensor<dtypeA, LayoutA> A,
                                 Tensor<dtypeB, LayoutB> B, Tensor<dtypeC, LayoutC> C);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeA, typename ShapeA, typename StrideA,
              typename dtypeB, typename ShapeB, typename StrideB, typename dtypeC, typename ShapeC, typename StrideC>
    void call([[maybe_unused]] Tensor<dtypeD, Layout<ShapeD, StrideD>> &D,
              const Tensor<dtypeA, Layout<ShapeA, StrideA>> &A, const Tensor<dtypeB, Layout<ShapeB, StrideB>> &B,
              const Tensor<dtypeC, Layout<ShapeC, StrideC>> &C) MMA_INOUT
    {
        TiledCallFunc::call_mma<M_16 * AtomShapeM, N_64 * AtomShapeN, StrideA, StrideB, StrideC, dtypeA, dtypeB,
                                dtypeC>(A.get_ptr(), B.get_ptr(), C.get_ptr(), K_4 * AtomShapeK);
    }
};

template <int AtomShapeM, int AtomShapeN, int AtomShapeK>
class TiledMma<Ops<MMA_32x32x4_INT8INT8INT32>, Shape<Int<AtomShapeM>, Int<AtomShapeN>, Int<AtomShapeK>>> {
public:
    template <typename TiledMma, typename dtypeD, typename LayoutD, typename dtypeA, typename LayoutA, typename dtypeB,
              typename LayoutB, typename dtypeC, typename LayoutC>
    friend void tensor_tiled_mma(TiledMma mma, Tensor<dtypeD, LayoutD> D, Tensor<dtypeA, LayoutA> A,
                                 Tensor<dtypeB, LayoutB> B, Tensor<dtypeC, LayoutC> C);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeA, typename ShapeA, typename StrideA,
              typename dtypeB, typename ShapeB, typename StrideB, typename dtypeC, typename ShapeC, typename StrideC>
    void call([[maybe_unused]] Tensor<dtypeD, Layout<ShapeD, StrideD>> &D,
              const Tensor<dtypeA, Layout<ShapeA, StrideA>> &A, const Tensor<dtypeB, Layout<ShapeB, StrideB>> &B,
              const Tensor<dtypeC, Layout<ShapeC, StrideC>> &C) MMA_INOUT
    {
        TiledCallFunc::call_mma<M_32 * AtomShapeM, N_32 * AtomShapeN, StrideA, StrideB, StrideC, dtypeA, dtypeB,
                                dtypeC>(A.get_ptr(), B.get_ptr(), C.get_ptr(), K_4 * AtomShapeK);
    }
};

template <int AtomShapeM, int AtomShapeN>
class TiledStore<Ops<STORE_32x16_F64>, Shape<Int<AtomShapeM>, Int<AtomShapeN>>> {
public:
    template <typename TiledStore, typename dtype, typename Layout>
    friend void tensor_tiled_store(TiledStore store, Tensor<dtype, Layout> tensor);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD>
    void call(Tensor<dtypeD, Layout<ShapeD, StrideD>> &tensor) MMA_IN
    {
        TiledCallFunc::call_store<M_32 * AtomShapeM, N_16 * AtomShapeN, StrideD, dtypeD>(tensor.get_ptr());
    }
};

template <int AtomShapeM, int AtomShapeN>
class TiledStore<Ops<STORE_16x64_F32>, Shape<Int<AtomShapeM>, Int<AtomShapeN>>> {
public:
    template <typename TiledStore, typename dtype, typename Layout>
    friend void tensor_tiled_store(TiledStore store, Tensor<dtype, Layout> tensor);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD>
    void call(Tensor<dtypeD, Layout<ShapeD, StrideD>> &tensor) MMA_IN
    {
        TiledCallFunc::call_store<M_16 * AtomShapeM, N_64 * AtomShapeN, StrideD, dtypeD>(tensor.get_ptr());
    }
};

template <int AtomShapeM, int AtomShapeN>
class TiledStore<Ops<STORE_16x64_INT32>, Shape<Int<AtomShapeM>, Int<AtomShapeN>>> {
public:
    template <typename TiledStore, typename dtype, typename Layout>
    friend void tensor_tiled_store(TiledStore store, Tensor<dtype, Layout> tensor);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD>
    void call(Tensor<dtypeD, Layout<ShapeD, StrideD>> &tensor) MMA_IN
    {
        TiledCallFunc::call_store<M_16 * AtomShapeM, N_64 * AtomShapeN, StrideD, dtypeD>(tensor.get_ptr());
    }
};

template <int AtomShapeM, int AtomShapeN>
class TiledStore<Ops<STORE_32x32_INT32>, Shape<Int<AtomShapeM>, Int<AtomShapeN>>> {
public:
    template <typename TiledStore, typename dtype, typename Layout>
    friend void tensor_tiled_store(TiledStore store, Tensor<dtype, Layout> tensor);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD>
    void call(Tensor<dtypeD, Layout<ShapeD, StrideD>> &tensor) MMA_IN
    {
        TiledCallFunc::call_store<M_32 * AtomShapeM, N_32 * AtomShapeN, StrideD, dtypeD>(tensor.get_ptr());
    }
};

template <int AtomShapeM, int AtomShapeN>
class TiledCopy<Ops<COPY_32x1_F64_RM2CM>, Shape<Int<AtomShapeM>, Int<AtomShapeN>>> {
public:
    template <typename TiledCopy, typename dtypeD, typename LayoutD, typename dtypeS, typename LayoutS>
    friend void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeD, LayoutD> dst, Tensor<dtypeS, LayoutS> src);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeS, typename ShapeS, typename StrideS>
    void call(Tensor<dtypeD, Layout<ShapeD, StrideD>> &dst, const Tensor<dtypeS, Layout<ShapeS, StrideS>> &src) MMA_IN
    {
        TiledCallFunc::call_copy<Ops<COPY_32x1_F64_RM2CM>, dtypeD, dtypeS>(dst.get_ptr(), src.get_ptr(),
                                                                           M_32 * AtomShapeM, N_1 * AtomShapeN);
    }
};

template <int AtomShapeM, int AtomShapeN>
class TiledCopy<Ops<COPY_1x16_F64_CM2RM>, Shape<Int<AtomShapeM>, Int<AtomShapeN>>> {
public:
    template <typename TiledCopy, typename dtypeD, typename LayoutD, typename dtypeS, typename LayoutS>
    friend void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeD, LayoutD> dst, Tensor<dtypeS, LayoutS> src);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeS, typename ShapeS, typename StrideS>
    void call(Tensor<dtypeD, Layout<ShapeD, StrideD>> &dst, const Tensor<dtypeS, Layout<ShapeS, StrideS>> &src) MMA_IN
    {
        TiledCallFunc::call_copy<Ops<COPY_1x16_F64_CM2RM>, dtypeD, dtypeS>(dst.get_ptr(), src.get_ptr(), M_1 * AtomShapeM,
                                                                           N_16 * AtomShapeN);
    }
};

template <int AtomShapeM, int AtomShapeN>
class TiledCopy<Ops<COPY_16x2_BF16_RM2ZZ>, Shape<Int<AtomShapeM>, Int<AtomShapeN>>> {
public:
    template <typename TiledCopy, typename dtypeD, typename LayoutD, typename dtypeS, typename LayoutS>
    friend void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeD, LayoutD> dst, Tensor<dtypeS, LayoutS> src);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeS, typename ShapeS, typename StrideS>
    void call(Tensor<dtypeD, Layout<ShapeD, StrideD>> &dst, const Tensor<dtypeS, Layout<ShapeS, StrideS>> &src) MMA_IN
    {
        TiledCallFunc::call_copy<Ops<COPY_16x2_BF16_RM2ZZ>, dtypeD, dtypeS>(dst.get_ptr(), src.get_ptr(),
                                                                            M_16 * AtomShapeM, N_2 * AtomShapeN);
    }
};

template <int AtomShapeM, int AtomShapeN>
class TiledCopy<Ops<COPY_2x64_BF16_CM2NN>, Shape<Int<AtomShapeM>, Int<AtomShapeN>>> {
public:
    template <typename TiledCopy, typename dtypeD, typename LayoutD, typename dtypeS, typename LayoutS>
    friend void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeD, LayoutD> dst, Tensor<dtypeS, LayoutS> src);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeS, typename ShapeS, typename StrideS>
    void call(Tensor<dtypeD, Layout<ShapeD, StrideD>> &dst, const Tensor<dtypeS, Layout<ShapeS, StrideS>> &src) MMA_IN
    {
        TiledCallFunc::call_copy<Ops<COPY_2x64_BF16_CM2NN>, dtypeD, dtypeS>(dst.get_ptr(), src.get_ptr(),
                                                                            M_2 * AtomShapeM, N_64 * AtomShapeN);
    }
};

template <int AtomShapeM, int AtomShapeN>
class TiledCopy<Ops<COPY_16x1_BF16_RM2CM>, Shape<Int<AtomShapeM>, Int<AtomShapeN>>> {
public:
    template <typename TiledCopy, typename dtypeD, typename LayoutD, typename dtypeS, typename LayoutS>
    friend void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeD, LayoutD> dst, Tensor<dtypeS, LayoutS> src);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeS, typename ShapeS, typename StrideS>
    void call(Tensor<dtypeD, Layout<ShapeD, StrideD>> &dst, const Tensor<dtypeS, Layout<ShapeS, StrideS>> &src) MMA_IN
    {
        TiledCallFunc::call_copy<Ops<COPY_16x1_BF16_RM2CM>, dtypeD, dtypeS>(dst.get_ptr(), src.get_ptr(),
                                                                            M_16 * AtomShapeM, N_1 * AtomShapeN);
    }
};

template <int AtomShapeM, int AtomShapeN>
class TiledCopy<Ops<COPY_1x64_BF16_CM2RM>, Shape<Int<AtomShapeM>, Int<AtomShapeN>>> {
public:
    template <typename TiledCopy, typename dtypeD, typename LayoutD, typename dtypeS, typename LayoutS>
    friend void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeD, LayoutD> dst, Tensor<dtypeS, LayoutS> src);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeS, typename ShapeS, typename StrideS>
    void call(Tensor<dtypeD, Layout<ShapeD, StrideD>> &dst, const Tensor<dtypeS, Layout<ShapeS, StrideS>> &src) MMA_IN
    {
        TiledCallFunc::call_copy<Ops<COPY_1x64_BF16_CM2RM>, dtypeD, dtypeS>(dst.get_ptr(), src.get_ptr(),
                                                                            M_1 * AtomShapeM, N_64 * AtomShapeN);
    }
};

template <int AtomShapeM, int AtomShapeN>
class TiledCopy<Ops<COPY_16x4_INT8_RM2ZZ>, Shape<Int<AtomShapeM>, Int<AtomShapeN>>> {
public:
    template <typename TiledCopy, typename dtypeD, typename LayoutD, typename dtypeS, typename LayoutS>
    friend void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeD, LayoutD> dst, Tensor<dtypeS, LayoutS> src);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeS, typename ShapeS, typename StrideS>
    void call(Tensor<dtypeD, Layout<ShapeD, StrideD>> &dst, const Tensor<dtypeS, Layout<ShapeS, StrideS>> &src) MMA_IN
    {
        TiledCallFunc::call_copy<Ops<COPY_16x4_INT8_RM2ZZ>, dtypeD, dtypeS>(dst.get_ptr(), src.get_ptr(),
                                                                            M_16 * AtomShapeM, N_4 * AtomShapeN);
    }
};

template <int AtomShapeM, int AtomShapeN>
class TiledCopy<Ops<COPY_4x64_INT8_CM2NN>, Shape<Int<AtomShapeM>, Int<AtomShapeN>>> {
public:
    template <typename TiledCopy, typename dtypeD, typename LayoutD, typename dtypeS, typename LayoutS>
    friend void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeD, LayoutD> dst, Tensor<dtypeS, LayoutS> src);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeS, typename ShapeS, typename StrideS>
    void call(Tensor<dtypeD, Layout<ShapeD, StrideD>> &dst, const Tensor<dtypeS, Layout<ShapeS, StrideS>> &src) MMA_IN
    {
        TiledCallFunc::call_copy<Ops<COPY_4x64_INT8_CM2NN>, dtypeD, dtypeS>(dst.get_ptr(), src.get_ptr(),
                                                                            M_4 * AtomShapeM, N_64 * AtomShapeN);
    }
};

template <int AtomShapeM, int AtomShapeN>
class TiledCopy<Ops<COPY_32x4_INT8_RM2ZZ>, Shape<Int<AtomShapeM>, Int<AtomShapeN>>> {
public:
    template <typename TiledCopy, typename dtypeD, typename LayoutD, typename dtypeS, typename LayoutS>
    friend void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeD, LayoutD> dst, Tensor<dtypeS, LayoutS> src);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeS, typename ShapeS, typename StrideS>
    void call(Tensor<dtypeD, Layout<ShapeD, StrideD>> &dst, const Tensor<dtypeS, Layout<ShapeS, StrideS>> &src) MMA_IN
    {
        TiledCallFunc::call_copy<Ops<COPY_32x4_INT8_RM2ZZ>, dtypeD, dtypeS>(dst.get_ptr(), src.get_ptr(),
                                                                            M_32 * AtomShapeM, N_4 * AtomShapeN);
    }
};

template <int AtomShapeM, int AtomShapeN>
class TiledCopy<Ops<COPY_4x32_INT8_CM2NN>, Shape<Int<AtomShapeM>, Int<AtomShapeN>>> {
public:
    template <typename TiledCopy, typename dtypeD, typename LayoutD, typename dtypeS, typename LayoutS>
    friend void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeD, LayoutD> dst, Tensor<dtypeS, LayoutS> src);

private:
    template <typename dtypeD, typename ShapeD, typename StrideD, typename dtypeS, typename ShapeS, typename StrideS>
    void call(Tensor<dtypeD, Layout<ShapeD, StrideD>> &dst, const Tensor<dtypeS, Layout<ShapeS, StrideS>> &src) MMA_IN
    {
        TiledCallFunc::call_copy<Ops<COPY_4x32_INT8_CM2NN>, dtypeD, dtypeS>(dst.get_ptr(), src.get_ptr(),
                                                                            M_4 * AtomShapeM, N_32 * AtomShapeN);
    }
};

static inline __attribute__((always_inline)) void kupl_prefetch_L1(const void *data)
{
    __asm__ __volatile__("prfm PLDL1STRM, [%[data]]         \n\t" ::[data] "r"(data));
}

template <int AtomShapeM>
class TiledCopy<Ops<KP36_PREFETCH_L1>, Shape<Int<AtomShapeM>>> {
public:
    template <typename TiledCopy, typename dtypeS, typename LayoutS>
    friend void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeS, LayoutS> src);

private:
    template <typename dtypeS, typename ShapeS, typename StrideS>
    void call(const Tensor<dtypeS, Layout<ShapeS, StrideS>> &src)
    {
        kupl_prefetch_L1(src.get_ptr());
    }
};

static inline __attribute__((always_inline)) void kupl_prefetch_L2(const void *data)
{
    __asm__ __volatile__("prfm PLDL2KEEP, [%[data]]         \n\t" ::[data] "r"(data));
}

template <int AtomShapeM>
class TiledCopy<Ops<KP36_PREFETCH_L2>, Shape<Int<AtomShapeM>>> {
public:
    template <typename TiledCopy, typename dtypeS, typename LayoutS>
    friend void tensor_tiled_copy(TiledCopy copy, Tensor<dtypeS, LayoutS> src);

private:
    template <typename dtypeS, typename ShapeS, typename StrideS>
    void call(const Tensor<dtypeS, Layout<ShapeS, StrideS>> &src)
    {
        kupl_prefetch_L2(src.get_ptr());
    }
};
} // namespace tensor
} // namespace kupl

#endif

#endif
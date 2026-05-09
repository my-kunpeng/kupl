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

template <int V>
class Ops : public Val<int, V> {};

template <typename... Args>
class Shape {};

template <typename... Args>
class Stride {};

template <typename Shape, typename Stride>
class Layout {};

template <typename dtype, typename Layout>
class Tensor {
public:
    Tensor(dtype *ptr_, Layout layout_) : ptr(ptr_), layout(layout_) {}
    ~Tensor()
    {
        ptr = nullptr;
    }

    inline dtype *getPtr() const
    {
        return ptr;
    }

private:
    dtype *ptr;
    Layout layout;
};

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
kupl_export Shape<Args...> make_shape([[maybe_unused]] Args... args)
{
    return Shape<Args...>{};
}

template <typename... Args>
kupl_export Stride<Args...> make_stride([[maybe_unused]] Args... args)
{
    return Stride<Args...>{};
}

template <typename Shape, typename Stride>
kupl_export Layout<Shape, Stride> make_layout([[maybe_unused]] Shape shape, [[maybe_unused]] Stride stride)
{
    return Layout<Shape, Stride>{};
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
                                dtypeC>(A.getPtr(), B.getPtr(), C.getPtr(), AtomShapeK * K_512);
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
                                dtypeC>(A.getPtr(), B.getPtr(), C.getPtr(), AtomShapeK);
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
                                dtypeC>(A.getPtr(), B.getPtr(), C.getPtr(), K_2 * AtomShapeK);
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
                                dtypeC>(A.getPtr(), B.getPtr(), C.getPtr(), AtomShapeK);
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
                                dtypeC>(A.getPtr(), B.getPtr(), C.getPtr(), K_4 * AtomShapeK);
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
                                dtypeC>(A.getPtr(), B.getPtr(), C.getPtr(), K_4 * AtomShapeK);
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
        TiledCallFunc::call_store<M_32 * AtomShapeM, N_16 * AtomShapeN, StrideD, dtypeD>(tensor.getPtr());
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
        TiledCallFunc::call_store<M_16 * AtomShapeM, N_64 * AtomShapeN, StrideD, dtypeD>(tensor.getPtr());
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
        TiledCallFunc::call_store<M_16 * AtomShapeM, N_64 * AtomShapeN, StrideD, dtypeD>(tensor.getPtr());
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
        TiledCallFunc::call_store<M_32 * AtomShapeM, N_32 * AtomShapeN, StrideD, dtypeD>(tensor.getPtr());
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
        TiledCallFunc::call_copy<Ops<COPY_32x1_F64_RM2CM>, dtypeD, dtypeS>(dst.getPtr(), src.getPtr(),
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
        TiledCallFunc::call_copy<Ops<COPY_1x16_F64_CM2RM>, dtypeD, dtypeS>(dst.getPtr(), src.getPtr(), M_1 * AtomShapeM,
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
        TiledCallFunc::call_copy<Ops<COPY_16x2_BF16_RM2ZZ>, dtypeD, dtypeS>(dst.getPtr(), src.getPtr(),
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
        TiledCallFunc::call_copy<Ops<COPY_2x64_BF16_CM2NN>, dtypeD, dtypeS>(dst.getPtr(), src.getPtr(),
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
        TiledCallFunc::call_copy<Ops<COPY_16x1_BF16_RM2CM>, dtypeD, dtypeS>(dst.getPtr(), src.getPtr(),
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
        TiledCallFunc::call_copy<Ops<COPY_1x64_BF16_CM2RM>, dtypeD, dtypeS>(dst.getPtr(), src.getPtr(),
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
        TiledCallFunc::call_copy<Ops<COPY_16x4_INT8_RM2ZZ>, dtypeD, dtypeS>(dst.getPtr(), src.getPtr(),
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
        TiledCallFunc::call_copy<Ops<COPY_4x64_INT8_CM2NN>, dtypeD, dtypeS>(dst.getPtr(), src.getPtr(),
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
        TiledCallFunc::call_copy<Ops<COPY_32x4_INT8_RM2ZZ>, dtypeD, dtypeS>(dst.getPtr(), src.getPtr(),
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
        TiledCallFunc::call_copy<Ops<COPY_4x32_INT8_CM2NN>, dtypeD, dtypeS>(dst.getPtr(), src.getPtr(),
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
        kupl_prefetch_L1(src.getPtr());
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
        kupl_prefetch_L2(src.getPtr());
    }
};
} // namespace tensor
} // namespace kupl

#endif

#endif
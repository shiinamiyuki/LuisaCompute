//
// Created by Mike Smith on 2020/12/2.
//

#pragma once

#include <dsl/expr.h>
#include <dsl/arg.h>

namespace luisa::compute {

template<typename T>
struct Var : public detail::Expr<T> {

    static_assert(std::is_trivially_destructible_v<T>);

    // for local variables
    template<typename... Args>
    requires concepts::constructible<T, detail::expr_value_t<Args>...>
    Var(Args &&...args)
    noexcept
        : detail::Expr<T>{detail::FunctionBuilder::current()->local(
            Type::of<T>(),
            {detail::extract_expression(std::forward<Args>(args))...})} {}

    // for internal use only...
    explicit Var(detail::ArgumentCreation) noexcept
        : detail::Expr<T>{detail::FunctionBuilder::current()->argument(Type::of<T>())} {}

    Var(Var &&) noexcept = default;
    Var(const Var &another) noexcept : Var{detail::Expr{another}} {}
    void operator=(Var &&rhs) noexcept { detail::Expr<T>::operator=(rhs); }
    void operator=(const Var &rhs) noexcept { detail::Expr<T>::operator=(rhs); }
};

template<typename T>
struct Var<Buffer<T>> : public detail::Expr<Buffer<T>> {
    explicit Var(detail::ArgumentCreation) noexcept
        : detail::Expr<Buffer<T>>{
            detail::FunctionBuilder::current()->buffer(Type::of<Buffer<T>>())} {}
    Var(Var &&) noexcept = default;
    Var(const Var &) noexcept = delete;
    Var &operator=(Var &&) noexcept = delete;
    Var &operator=(const Var &) noexcept = delete;
};

template<typename T>
struct Var<BufferView<T>> : public detail::Expr<Buffer<T>> {
    explicit Var(detail::ArgumentCreation) noexcept
        : detail::Expr<Buffer<T>>{
            detail::FunctionBuilder::buffer(Type::of<Buffer<T>>())} {}
    Var(Var &&) noexcept = default;
    Var(const Var &) noexcept = delete;
    Var &operator=(Var &&) noexcept = delete;
    Var &operator=(const Var &) noexcept = delete;
};

template<typename T>
struct Var<Image<T>> : public detail::Expr<Image<T>> {
    explicit Var(detail::ArgumentCreation) noexcept
        : detail::Expr<Image<T>>{
            detail::FunctionBuilder::current()->texture(Type::of<Image<T>>()),
            detail::FunctionBuilder::current()->argument(Type::of<uint2>())} {
    }
    Var(Var &&) noexcept = default;
    Var(const Var &) noexcept = delete;
    Var &operator=(Var &&) noexcept = delete;
    Var &operator=(const Var &) noexcept = delete;
};

template<typename T>
struct Var<ImageView<T>> : public detail::Expr<Image<T>> {
    explicit Var(detail::ArgumentCreation) noexcept
        : detail::Expr<Image<T>>{
            detail::FunctionBuilder::texture(Type::of<Image<T>>()),
            detail::FunctionBuilder::current()->argument(Type::of<uint2>())} {}
    Var(Var &&) noexcept = default;
    Var(const Var &) noexcept = delete;
    Var &operator=(Var &&) noexcept = delete;
    Var &operator=(const Var &) noexcept = delete;
};

template<typename T>
struct Var<Volume<T>> : public detail::Expr<Volume<T>> {
    explicit Var(detail::ArgumentCreation) noexcept
        : detail::Expr<Volume<T>>{
            detail::FunctionBuilder::current()->texture(Type::of<Volume<T>>()),
            detail::FunctionBuilder::current()->argument(Type::of<uint3>())} {}
    Var(Var &&) noexcept = default;
    Var(const Var &) noexcept = delete;
    Var &operator=(Var &&) noexcept = delete;
    Var &operator=(const Var &) noexcept = delete;
};

template<typename T>
struct Var<VolumeView<T>> : public detail::Expr<Volume<T>> {
    explicit Var(detail::ArgumentCreation) noexcept
        : detail::Expr<Volume<T>>{
            detail::FunctionBuilder::texture(Type::of<Volume<T>>()),
            detail::FunctionBuilder::current()->argument(Type::of<uint3>())} {}
    Var(Var &&) noexcept = default;
    Var(const Var &) noexcept = delete;
    Var &operator=(Var &&) noexcept = delete;
    Var &operator=(const Var &) noexcept = delete;
};

template<>
struct Var<Heap> : public detail::Expr<Heap> {
    explicit Var(detail::ArgumentCreation) noexcept
        : detail::Expr<Heap>{
            detail::FunctionBuilder::current()->heap()} {}
    Var(Var &&) noexcept = default;
    Var(const Var &) noexcept = delete;
    Var &operator=(Var &&) noexcept = delete;
    Var &operator=(const Var &) noexcept = delete;
};

template<typename T>
Var(detail::Expr<T>) -> Var<T>;

template<typename T>
Var(T &&) -> Var<T>;

template<typename T, size_t N>
using ArrayVar = Var<std::array<T, N>>;

template<typename T>
using BufferVar = Var<Buffer<T>>;

template<typename T>
using ImageVar = Var<Image<T>>;

template<typename T>
using VolumeVar = Var<Volume<T>>;

using HeapVar = Var<Heap>;

using Int = Var<int>;
using Int2 = Var<int2>;
using Int3 = Var<int3>;
using Int4 = Var<int4>;
using UInt = Var<uint>;
using UInt2 = Var<uint2>;
using UInt3 = Var<uint3>;
using UInt4 = Var<uint4>;
using Float = Var<float>;
using Float2 = Var<float2>;
using Float3 = Var<float3>;
using Float4 = Var<float4>;
using Bool = Var<bool>;
using Bool2 = Var<bool2>;
using Bool3 = Var<bool3>;
using Bool4 = Var<bool4>;
using Float2x2 = Var<float2x2>;
using Float3x3 = Var<float3x3>;
using Float4x4 = Var<float4x4>;

template<size_t N>
using ArrayInt = ArrayVar<int, N>;
template<size_t N>
using ArrayInt2 = ArrayVar<int2, N>;
template<size_t N>
using ArrayInt3 = ArrayVar<int3, N>;
template<size_t N>
using ArrayInt4 = ArrayVar<int4, N>;
template<size_t N>
using ArrayUInt = ArrayVar<uint, N>;
template<size_t N>
using ArrayUInt2 = ArrayVar<uint2, N>;
template<size_t N>
using ArrayUInt3 = ArrayVar<uint3, N>;
template<size_t N>
using ArrayUInt4 = ArrayVar<uint4, N>;
template<size_t N>
using ArrayFloat = ArrayVar<float, N>;
template<size_t N>
using ArrayFloat2 = ArrayVar<float2, N>;
template<size_t N>
using ArrayFloat3 = ArrayVar<float3, N>;
template<size_t N>
using ArrayFloat4 = ArrayVar<float4, N>;
template<size_t N>
using ArrayBool = ArrayVar<bool, N>;
template<size_t N>
using ArrayBool2 = ArrayVar<bool2, N>;
template<size_t N>
using ArrayBool3 = ArrayVar<bool3, N>;
template<size_t N>
using ArrayBool4 = ArrayVar<bool4, N>;

using BufferInt = BufferVar<int>;
using BufferInt2 = BufferVar<int2>;
using BufferInt3 = BufferVar<int3>;
using BufferInt4 = BufferVar<int4>;
using BufferUInt = BufferVar<uint>;
using BufferUInt2 = BufferVar<uint2>;
using BufferUInt3 = BufferVar<uint3>;
using BufferUInt4 = BufferVar<uint4>;
using BufferFloat = BufferVar<float>;
using BufferFloat2 = BufferVar<float2>;
using BufferFloat3 = BufferVar<float3>;
using BufferFloat4 = BufferVar<float4>;
using BufferBool = BufferVar<bool>;
using BufferBool2 = BufferVar<bool2>;
using BufferBool3 = BufferVar<bool3>;
using BufferBool4 = BufferVar<bool4>;

using ImageInt = ImageVar<int>;
using ImageUInt = ImageVar<uint>;
using ImageFloat = ImageVar<float>;

using VolumeInt = VolumeVar<int>;
using VolumeUInt = VolumeVar<uint>;
using VolumeFloat = VolumeVar<float>;

}// namespace luisa::compute

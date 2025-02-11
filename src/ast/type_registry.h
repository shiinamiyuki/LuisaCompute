//
// Created by Mike Smith on 2021/2/22.
//

#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <tuple>
#include <sstream>

#include <util/arena.h>
#include <core/macro.h>
#include <util/spin_mutex.h>
#include <ast/type.h>

namespace luisa::compute {

template<typename T>
class Buffer;

template<typename T>
class BufferView;

template<typename T>
class Image;

template<typename T>
class ImageView;

template<typename T>
class Volume;

template<typename T>
class VolumeView;

class Heap;
class Accel;

class TypeRegistry {

private:
    std::vector<std::unique_ptr<Type>> _types;
    spin_mutex _types_mutex;

public:
    template<typename F>
    decltype(auto) with_types(F &&f) noexcept {
        std::scoped_lock lock{_types_mutex};
        return f(_types);
    }
};

namespace detail {

template<typename T>
struct TypeDesc {
    static_assert(always_false_v<T>, "Invalid type.");
};

// scalar
#define LUISA_MAKE_SCALAR_AND_VECTOR_TYPE_DESC_SPECIALIZATION(S, tag) \
    template<>                                                        \
    struct TypeDesc<S> {                                              \
        static constexpr std::string_view description() noexcept {    \
            using namespace std::string_view_literals;                \
            return #S##sv;                                            \
        }                                                             \
    };                                                                \
    template<>                                                        \
    struct TypeDesc<Vector<S, 2>> {                                   \
        static constexpr std::string_view description() noexcept {    \
            using namespace std::string_view_literals;                \
            return "vector<" #S ",2>"sv;                              \
        }                                                             \
    };                                                                \
    template<>                                                        \
    struct TypeDesc<Vector<S, 3>> {                                   \
        static constexpr std::string_view description() noexcept {    \
            using namespace std::string_view_literals;                \
            return "vector<" #S ",3>"sv;                              \
        }                                                             \
    };                                                                \
    template<>                                                        \
    struct TypeDesc<Vector<S, 4>> {                                   \
        static constexpr std::string_view description() noexcept {    \
            using namespace std::string_view_literals;                \
            return "vector<" #S ",4>"sv;                              \
        }                                                             \
    };

LUISA_MAKE_SCALAR_AND_VECTOR_TYPE_DESC_SPECIALIZATION(bool, BOOL)
LUISA_MAKE_SCALAR_AND_VECTOR_TYPE_DESC_SPECIALIZATION(float, FLOAT)
LUISA_MAKE_SCALAR_AND_VECTOR_TYPE_DESC_SPECIALIZATION(int, INT32)
LUISA_MAKE_SCALAR_AND_VECTOR_TYPE_DESC_SPECIALIZATION(uint, UINT32)

#undef LUISA_MAKE_SCALAR_AND_VECTOR_TYPE_DESC_SPECIALIZATION

// array
template<typename T, size_t N>
struct TypeDesc<std::array<T, N>> {
    static std::string_view description() noexcept {
        static thread_local auto s = fmt::format(FMT_STRING("array<{},{}>"), TypeDesc<T>::description(), N);
        return s;
    }
};

template<typename T, size_t N>
struct TypeDesc<T[N]> {
    static std::string_view description() noexcept {
        static thread_local auto s = fmt::format(FMT_STRING("array<{},{}>"), TypeDesc<T>::description(), N);
        return s;
    }
};

template<typename T>
struct TypeDesc<Buffer<T>> {
    static std::string_view description() noexcept {
        static thread_local auto s = fmt::format(
            FMT_STRING("buffer<{}>"),
            TypeDesc<T>::description());
        return s;
    }
};

template<typename T>
struct TypeDesc<BufferView<T>> : TypeDesc<Buffer<T>> {};

template<typename T>
struct TypeDesc<Image<T>> {
    static std::string_view description() noexcept {
        static thread_local auto s = fmt::format(
            FMT_STRING("texture<2,{}>"),
            TypeDesc<T>::description());
        return s;
    }
};

template<typename T>
struct TypeDesc<ImageView<T>> : TypeDesc<Image<T>> {};

template<typename T>
struct TypeDesc<Volume<T>> {
    static std::string_view description() noexcept {
        static thread_local auto s = fmt::format(
            FMT_STRING("texture<3,{}>"),
            TypeDesc<T>::description());
        return s;
    }
};

template<>
struct TypeDesc<Heap> {
    static constexpr std::string_view description() noexcept {
        return "heap";
    }
};

template<>
struct TypeDesc<Accel> {
    static constexpr std::string_view description() noexcept {
        return "accel";
    }
};

template<typename T>
struct TypeDesc<VolumeView<T>> : TypeDesc<Volume<T>> {};

// matrices
template<>
struct TypeDesc<float2x2> {
    static constexpr std::string_view description() noexcept {
        using namespace std::string_view_literals;
        return "matrix<2>"sv;
    }
};

template<>
struct TypeDesc<float3x3> {
    static constexpr std::string_view description() noexcept {
        using namespace std::string_view_literals;
        return "matrix<3>"sv;
    }
};

template<>
struct TypeDesc<float4x4> {
    static constexpr std::string_view description() noexcept {
        using namespace std::string_view_literals;
        return "matrix<4>"sv;
    }
};

template<typename... T>
struct TypeDesc<std::tuple<T...>> {
    static std::string_view description() noexcept {
        static thread_local auto s = [] {
            std::ostringstream os;
            os << "struct<" << alignof(std::tuple<T...>);
            auto appender = [](std::string_view ts) { return fmt::format(",{}", ts); };
            (os << ... << appender(TypeDesc<T>::description()));
            os << ">";
            return os.str();
        }();
        return s;
    }
};

}// namespace detail

template<typename T>
const Type *Type::of() noexcept {
    static thread_local auto info = Type::from(detail::TypeDesc<std::remove_cvref_t<T>>::description());
    return info;
}

}// namespace luisa::compute

// struct
#define LUISA_STRUCTURE_MAP_MEMBER_TO_DESC(m) TypeDesc<std::remove_cvref_t<decltype(std::declval<This>().m)>>::description()
#define LUISA_STRUCTURE_MAP_MEMBER_TO_FMT(m) ",{}"

#define LUISA_MAKE_STRUCTURE_TYPE_DESC_SPECIALIZATION(S, ...)                                            \
    namespace luisa::compute::detail {                                                                   \
    static_assert(std::is_standard_layout_v<S>);                                                         \
    template<>                                                                                           \
    struct TypeDesc<S> {                                                                                 \
        using This = S;                                                                                  \
        static std::string_view description() noexcept {                                                 \
            static auto s = fmt::format(                                                                 \
                FMT_STRING("struct<{}" LUISA_MAP(LUISA_STRUCTURE_MAP_MEMBER_TO_FMT, ##__VA_ARGS__) ">"), \
                alignof(S),                                                                              \
                LUISA_MAP_LIST(LUISA_STRUCTURE_MAP_MEMBER_TO_DESC, ##__VA_ARGS__));                      \
            return s;                                                                                    \
        }                                                                                                \
    };                                                                                                   \
    }
#define LUISA_STRUCT_REFLECT(S, ...) \
    LUISA_MAKE_STRUCTURE_TYPE_DESC_SPECIALIZATION(S, __VA_ARGS__)

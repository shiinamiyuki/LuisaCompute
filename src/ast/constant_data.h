//
// Created by Mike Smith on 2021/3/6.
//

#pragma once

#include <variant>
#include <memory>
#include <span>
#include <mutex>
#include <vector>
#include <array>

#include <core/data_types.h>
#include <core/concepts.h>

namespace luisa::compute {

namespace detail {

template<typename T>
struct constant_data_view {
    static_assert(always_false_v<T>);
};

template<typename... T>
struct constant_data_view<std::tuple<T...>> {
    using type = std::variant<std::span<const T>...>;
};

}

class ConstantData {

public:
    using View = typename detail::constant_data_view<basic_types>::type;

private:
    std::unique_ptr<std::byte[]> _storage;
    View _view;
    uint64_t _hash;
    
    ConstantData(std::unique_ptr<std::byte[]> s, View v, uint64_t hash) noexcept
        : _storage{std::move(s)}, _view{v}, _hash{hash}{}

public:
    [[nodiscard]] static uint64_t create(View data) noexcept;
    [[nodiscard]] static View view(uint64_t hash) noexcept;
};

}// namespace luisa::compute

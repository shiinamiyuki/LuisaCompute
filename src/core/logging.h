//
// Created by Mike Smith on 2021/2/2.
//

#pragma once

#include <string_view>

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <core/platform.h>

namespace luisa {

template<typename... Args>
inline void log_verbose(Args &&...args) noexcept { spdlog::debug(std::forward<Args>(args)...); }

template<typename... Args>
inline void log_info(Args &&...args) noexcept { spdlog::info(std::forward<Args>(args)...); }

template<typename... Args>
inline void log_warning(Args &&...args) noexcept { spdlog::warn(std::forward<Args>(args)...); }

template<typename... Args>
[[noreturn]] LUISA_FORCE_INLINE void log_error(Args &&...args) noexcept {
    std::string error_message = fmt::format(std::forward<Args>(args)...);
    auto trace = luisa::backtrace();
    for (auto i = 0u; i < trace.size(); i++) {
        auto &&t = trace[i];
        using namespace std::string_view_literals;
        error_message.append(fmt::format(
            FMT_STRING("\n    {:>2} [0x{:012x}]: {} :: {} + {}"sv),
            i, t.address, t.module, t.symbol, t.offset));
    }
    spdlog::error("{}", error_message);
    std::abort();
}

void log_level_verbose() noexcept;
void log_level_info() noexcept;
void log_level_warning() noexcept;
void log_level_error() noexcept;

}// namespace luisa

#ifndef NDEBUG
#define LUISA_VERBOSE(fmt, ...) \
    ::luisa::log_verbose(FMT_STRING(std::string_view{fmt}), ##__VA_ARGS__)
#else
#define LUISA_VERBOSE(...)
#endif

#define LUISA_INFO(fmt, ...) \
    ::luisa::log_info(FMT_STRING(std::string_view{fmt}) __VA_OPT__(, ) __VA_ARGS__)
#define LUISA_WARNING(fmt, ...) \
    ::luisa::log_warning(FMT_STRING(std::string_view{fmt}), ##__VA_ARGS__)
#define LUISA_ERROR(fmt, ...) \
    ::luisa::log_error(FMT_STRING(std::string_view{fmt}), ##__VA_ARGS__)

#define LUISA_VERBOSE_WITH_LOCATION(fmt, ...) \
    LUISA_VERBOSE(fmt " [{}:{}]", ##__VA_ARGS__, __FILE__, __LINE__)
#define LUISA_INFO_WITH_LOCATION(fmt, ...) \
    LUISA_INFO(fmt " [{}:{}]", ##__VA_ARGS__, __FILE__, __LINE__)
#define LUISA_WARNING_WITH_LOCATION(fmt, ...) \
    LUISA_WARNING(fmt " [{}:{}]", ##__VA_ARGS__, __FILE__, __LINE__)
#define LUISA_ERROR_WITH_LOCATION(fmt, ...) \
    LUISA_ERROR(fmt " [{}:{}]", ##__VA_ARGS__, __FILE__, __LINE__)

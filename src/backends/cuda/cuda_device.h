//
// Created by Mike on 7/28/2021.
//

#pragma once

#include <cuda.h>

#include <runtime/device.h>
#include <backends/cuda/cuda_error.h>

namespace luisa::compute::cuda {

class CUDADevice : public Device::Interface {

    class ContextGuard {

    private:
        CUcontext _ctx;

    public:
        explicit ContextGuard(CUcontext ctx) noexcept : _ctx{ctx} {
            LUISA_CHECK_CUDA(cuCtxPushCurrent(_ctx));
        }
        ~ContextGuard() noexcept {
            CUcontext ctx = nullptr;
            LUISA_CHECK_CUDA(cuCtxPopCurrent(&ctx));
            if (ctx != _ctx) [[unlikely]] {
              LUISA_ERROR_WITH_LOCATION(
                  "Invalid CUDA context {} (expected {}).",
                  fmt::ptr(ctx), fmt::ptr(_ctx));
            }
        }
    };

public:
    class Handle {

    private:
        CUcontext _context{nullptr};
        CUdevice _device{0};

    public:
        explicit Handle(uint index) noexcept;
        ~Handle() noexcept;
        Handle(Handle &&) noexcept = delete;
        Handle(const Handle &) noexcept = delete;
        Handle &operator=(Handle &&) noexcept = delete;
        Handle &operator=(const Handle &) noexcept = delete;
        [[nodiscard]] std::string_view name() const noexcept;
        [[nodiscard]] auto device() const noexcept { return _device; }
        [[nodiscard]] auto context() const noexcept { return _context; }
    };

private:
    Handle _handle;
    std::recursive_mutex _mutex;

public:
    CUDADevice(const Context &ctx, uint device_id) noexcept;
    ~CUDADevice() noexcept override = default;
    [[nodiscard]] auto &handle() const noexcept { return _handle; }
    uint64_t create_buffer(size_t size_bytes, uint64_t heap_handle, uint32_t index_in_heap) noexcept override;
    void destroy_buffer(uint64_t handle) noexcept override;
    uint64_t create_texture(PixelFormat format, uint dimension, uint width, uint height, uint depth, uint mipmap_levels, TextureSampler sampler, uint64_t heap_handle, uint32_t index_in_heap) override;
    void destroy_texture(uint64_t handle) noexcept override;
    uint64_t create_heap(size_t size) noexcept override;
    size_t query_heap_memory_usage(uint64_t handle) noexcept override;
    void destroy_heap(uint64_t handle) noexcept override;
    uint64_t create_stream() noexcept override;
    void destroy_stream(uint64_t handle) noexcept override;
    void synchronize_stream(uint64_t stream_handle) noexcept override;
    void dispatch(uint64_t stream_handle, CommandList list) noexcept override;
    uint64_t create_shader(Function kernel) noexcept override;
    void destroy_shader(uint64_t handle) noexcept override;
    uint64_t create_event() noexcept override;
    void destroy_event(uint64_t handle) noexcept override;
    void signal_event(uint64_t handle, uint64_t stream_handle) noexcept override;
    void wait_event(uint64_t handle, uint64_t stream_handle) noexcept override;
    void synchronize_event(uint64_t handle) noexcept override;
    uint64_t create_mesh() noexcept override;
    void destroy_mesh(uint64_t handle) noexcept override;
    uint64_t create_accel() noexcept override;
    void destroy_accel(uint64_t handle) noexcept override;

    template<typename F>
    decltype(auto) with_locked(F &&f) noexcept {
        std::scoped_lock lock{_mutex};
        return f();
    }

    template<typename F>
    decltype(auto) with_handle(F &&f) const noexcept {
        ContextGuard guard{_handle.context()};
        return f();
    }
};

}// namespace luisa::compute::cuda

//
// Created by Mike Smith on 2021/3/19.
//

#import <core/platform.h>
#import <core/clock.h>
#import <ast/function.h>
#import <backends/metal/metal_device.h>
#import <backends/metal/metal_stream.h>
#import <backends/metal/metal_command_encoder.h>

namespace luisa::compute::metal {

MetalCommandEncoder::MetalCommandEncoder(
    MetalDevice *device,
    MetalStream *stream) noexcept
    : _device{device},
      _stream{stream},
      _command_buffer{stream->command_buffer()} {}

void MetalCommandEncoder::visit(const BufferCopyCommand *command) noexcept {
    auto blit_encoder = [_command_buffer blitCommandEncoder];
    [blit_encoder copyFromBuffer:_device->buffer(command->src_handle())
                    sourceOffset:command->src_offset()
                        toBuffer:_device->buffer(command->dst_handle())
               destinationOffset:command->dst_offset()
                            size:command->size()];
    [blit_encoder endEncoding];
}

void MetalCommandEncoder::visit(const BufferUploadCommand *command) noexcept {
    auto buffer = _device->buffer(command->handle());
    auto temp_buffer = _upload(command->data(), command->size());
    auto blit_encoder = [_command_buffer blitCommandEncoder];
    [blit_encoder copyFromBuffer:temp_buffer.handle()
                    sourceOffset:temp_buffer.offset()
                        toBuffer:buffer
               destinationOffset:command->offset()
                            size:command->size()];
    [blit_encoder endEncoding];
}

void MetalCommandEncoder::visit(const BufferDownloadCommand *command) noexcept {
    auto buffer = _device->buffer(command->handle());
    auto size = command->size();
    auto temp_buffer = _download(command->data(), size);
    auto blit_encoder = [_command_buffer blitCommandEncoder];
    [blit_encoder copyFromBuffer:buffer
                    sourceOffset:command->offset()
                        toBuffer:temp_buffer.handle()
               destinationOffset:temp_buffer.offset()
                            size:size];
    [blit_encoder endEncoding];
}

void MetalCommandEncoder::visit(const BufferToTextureCopyCommand *command) noexcept {
    auto buffer = _device->buffer(command->buffer());
    auto texture = _device->texture(command->texture());
    auto size = command->size();
    auto offset = command->offset();
    auto pixel_bytes = pixel_storage_size(command->storage());
    auto pitch_bytes = pixel_bytes * size.x;
    auto image_bytes = pitch_bytes * size.y * size.z;
    auto blit_encoder = [_command_buffer blitCommandEncoder];
    [blit_encoder copyFromBuffer:buffer
                    sourceOffset:command->buffer_offset()
               sourceBytesPerRow:pitch_bytes
             sourceBytesPerImage:image_bytes
                      sourceSize:MTLSizeMake(size.x, size.y, size.z)
                       toTexture:texture
                destinationSlice:0u
                destinationLevel:command->level()
               destinationOrigin:MTLOriginMake(offset.x, offset.y, offset.z)];
    [blit_encoder endEncoding];
}

void MetalCommandEncoder::visit(const TextureCopyCommand *command) noexcept {
    auto src = _device->texture(command->src_handle());
    auto dst = _device->texture(command->dst_handle());
    auto src_offset = command->src_offset();
    auto dst_offset = command->dst_offset();
    auto size = command->size();
    auto blit_encoder = [_command_buffer blitCommandEncoder];
    [blit_encoder copyFromTexture:src
                      sourceSlice:0u
                      sourceLevel:command->src_level()
                     sourceOrigin:MTLOriginMake(src_offset.x, src_offset.y, src_offset.z)
                       sourceSize:MTLSizeMake(size.x, size.y, size.z)
                        toTexture:dst
                 destinationSlice:0u
                 destinationLevel:command->dst_level()
                destinationOrigin:MTLOriginMake(dst_offset.x, dst_offset.y, dst_offset.z)];
    [blit_encoder endEncoding];
}

void MetalCommandEncoder::visit(const TextureToBufferCopyCommand *command) noexcept {
    auto buffer = _device->buffer(command->buffer());
    auto texture = _device->texture(command->texture());
    auto size = command->size();
    auto offset = command->offset();
    auto pixel_bytes = pixel_storage_size(command->storage());
    auto pitch_bytes = pixel_bytes * size.x;
    auto image_bytes = pitch_bytes * size.y * size.z;
    auto blit_encoder = [_command_buffer blitCommandEncoder];
    [blit_encoder copyFromTexture:texture
                      sourceSlice:0u
                      sourceLevel:command->level()
                     sourceOrigin:MTLOriginMake(offset.x, offset.y, offset.z)
                       sourceSize:MTLSizeMake(size.x, size.y, size.z)
                         toBuffer:buffer
                destinationOffset:command->buffer_offset()
           destinationBytesPerRow:pitch_bytes
         destinationBytesPerImage:image_bytes];
    [blit_encoder endEncoding];
}

void MetalCommandEncoder::visit(const TextureUploadCommand *command) noexcept {
    auto offset = command->offset();
    auto size = command->size();
    auto pixel_bytes = pixel_storage_size(command->storage());
    auto pitch_bytes = pixel_bytes * size.x;
    auto texture = _device->texture(command->handle());
    auto image_bytes = pitch_bytes * size.y * size.z;
    auto buffer = _upload(command->data(), image_bytes);
    auto blit_encoder = [_command_buffer blitCommandEncoder];
    [blit_encoder copyFromBuffer:buffer.handle()
                    sourceOffset:buffer.offset()
               sourceBytesPerRow:pitch_bytes
             sourceBytesPerImage:image_bytes
                      sourceSize:MTLSizeMake(size.x, size.y, size.z)
                       toTexture:texture
                destinationSlice:0u
                destinationLevel:command->level()
               destinationOrigin:MTLOriginMake(offset.x, offset.y, offset.z)];
    [blit_encoder endEncoding];
}

void MetalCommandEncoder::visit(const TextureDownloadCommand *command) noexcept {
    auto offset = command->offset();
    auto size = command->size();
    auto pixel_bytes = pixel_storage_size(command->storage());
    auto pitch_bytes = pixel_bytes * size.x;
    auto image_bytes = pitch_bytes * size.y * size.z;
    auto texture = _device->texture(command->handle());
    auto buffer = _download(command->data(), image_bytes);
    auto blit_encoder = [_command_buffer blitCommandEncoder];
    [blit_encoder copyFromTexture:texture
                      sourceSlice:0u
                      sourceLevel:command->level()
                     sourceOrigin:MTLOriginMake(offset.x, offset.y, offset.z)
                       sourceSize:MTLSizeMake(size.x, size.y, size.z)
                         toBuffer:buffer.handle()
                destinationOffset:buffer.offset()
           destinationBytesPerRow:pitch_bytes
         destinationBytesPerImage:image_bytes];
    [blit_encoder endEncoding];
}

void MetalCommandEncoder::visit(const ShaderDispatchCommand *command) noexcept {

    auto compiled_kernel = _device->compiled_kernel(command->handle());
    auto buffer_index = 0u;
    auto texture_index = 0u;

    auto launch_size = command->dispatch_size();
    auto block_size = command->kernel().block_size();
    auto blocks = (launch_size + block_size - 1u) / block_size;
    LUISA_VERBOSE_WITH_LOCATION(
        "Dispatching shader #{} in ({}, {}, {}) blocks "
        "with block_size ({}, {}, {}).",
        command->handle(),
        blocks.x, blocks.y, blocks.z,
        block_size.x, block_size.y, block_size.z);

    // update texture desc heap if any
    command->decode([&](auto, auto argument) noexcept -> void {
        if constexpr (std::is_same_v<decltype(argument), ShaderDispatchCommand::TextureHeapArgument>) {
            _command_buffer = _device->heap(argument.handle)->encode_update(_stream, _command_buffer);
        }
    });

    // encode compute shader
    auto compute_encoder = [_command_buffer computeCommandEncoderWithDispatchType:MTLDispatchTypeConcurrent];
    [compute_encoder setComputePipelineState:compiled_kernel.handle()];
    command->decode([&](auto, auto argument) noexcept -> void {
        using T = decltype(argument);
        if constexpr (std::is_same_v<T, ShaderDispatchCommand::BufferArgument>) {
            LUISA_VERBOSE_WITH_LOCATION(
                "Encoding buffer #{} at index {} with offset {}.",
                argument.handle, buffer_index, argument.offset);
            auto buffer = _device->buffer(argument.handle);
            [compute_encoder setBuffer:buffer
                                offset:argument.offset
                               atIndex:buffer_index++];
        } else if constexpr (std::is_same_v<T, ShaderDispatchCommand::TextureArgument>) {
            LUISA_VERBOSE_WITH_LOCATION(
                "Encoding texture #{} at index {}.",
                argument.handle, texture_index);
            auto texture = _device->texture(argument.handle);
            [compute_encoder setTexture:texture
                                atIndex:texture_index++];
        } else if constexpr (std::is_same_v<T, ShaderDispatchCommand::TextureHeapArgument>) {
            LUISA_VERBOSE_WITH_LOCATION(
                "Encoding texture heap #{} at index {}.",
                argument.handle, buffer_index);
            auto heap = _device->heap(argument.handle);
            [compute_encoder useHeap:heap->handle()];
            [compute_encoder setBuffer:heap->desc_buffer()
                                offset:0u
                               atIndex:buffer_index++];
        } else if constexpr (std::is_same_v<T, ShaderDispatchCommand::AccelArgument>) {
#ifdef LUISA_METAL_RAYTRACING_ENABLED
            LUISA_VERBOSE_WITH_LOCATION(
                "Encoding geometry #{} at index {}.",
                argument.handle, buffer_index);
            auto accel = _device->accel(argument.handle);
            if (auto resources = accel->resources(); !resources.empty()) {
                [compute_encoder useResources:resources.data()
                                        count:resources.size()
                                        usage:MTLResourceUsageRead];
            }
            if (auto heaps = accel->heaps(); !heaps.empty()) {
                [compute_encoder useHeaps:heaps.data() count:heaps.size()];
            }
            [compute_encoder setAccelerationStructure:accel->handle()
                                        atBufferIndex:buffer_index++];
#else
            LUISA_ERROR_WITH_LOCATION("Raytracing is not enabled for Metal backend.");
#endif
        } else {// uniform
            LUISA_VERBOSE_WITH_LOCATION(
                "Encoding uniform at index {}.",
                buffer_index);
            [compute_encoder setBytes:argument.data()
                               length:argument.size_bytes()
                              atIndex:buffer_index++];
        }
    });

    LUISA_VERBOSE_WITH_LOCATION(
        "Encoding dispatch size at index {}.",
        buffer_index);
    [compute_encoder setBytes:&launch_size
                       length:sizeof(launch_size)
                      atIndex:buffer_index];
    [compute_encoder dispatchThreadgroups:MTLSizeMake(blocks.x, blocks.y, blocks.z)
                    threadsPerThreadgroup:MTLSizeMake(block_size.x, block_size.y, block_size.z)];
    [compute_encoder endEncoding];
}

MetalBufferView MetalCommandEncoder::_upload(const void *host_ptr, size_t size) noexcept {
    auto rb = &_stream->upload_ring_buffer();
    auto buffer = rb->allocate(size);
    if (buffer.handle() == nullptr) {
        auto options = MTLResourceStorageModeShared
                       | MTLResourceCPUCacheModeWriteCombined
                       | MTLResourceHazardTrackingModeUntracked;
        auto handle = [_device->handle() newBufferWithBytes:host_ptr length:size options:options];
        return {handle, 0u, size};
    }
    std::memcpy(static_cast<std::byte *>(buffer.handle().contents) + buffer.offset(), host_ptr, size);
    [_command_buffer addCompletedHandler:^(id<MTLCommandBuffer>) { rb->recycle(buffer); }];
    return buffer;
}

MetalBufferView MetalCommandEncoder::_download(void *host_ptr, size_t size) noexcept {
    auto rb = &_stream->download_ring_buffer();
    auto buffer = rb->allocate(size);
    if (buffer.handle() == nullptr) {
        auto options = MTLResourceStorageModeShared | MTLResourceHazardTrackingModeUntracked;
        auto handle = [_device->handle() newBufferWithLength:size options:options];
        [_command_buffer addCompletedHandler:^(id<MTLCommandBuffer>) { std::memcpy(host_ptr, handle.contents, size); }];
        return {handle, 0u, size};
    }
    [_command_buffer addCompletedHandler:^(id<MTLCommandBuffer>) {
      std::memcpy(host_ptr, static_cast<const std::byte *>(buffer.handle().contents) + buffer.offset(), size);
      rb->recycle(buffer);
    }];
    return buffer;
}

#ifdef LUISA_METAL_RAYTRACING_ENABLED

void MetalCommandEncoder::visit(const AccelUpdateCommand *command) noexcept {
    auto accel = _device->accel(command->handle());
    _command_buffer = accel->update(
        _stream,
        _command_buffer,
        command->updated_transforms(),
        command->first_instance_to_update());
}

void MetalCommandEncoder::visit(const AccelBuildCommand *command) noexcept {
    auto accel = _device->accel(command->handle());
    _command_buffer = accel->build(
        _stream,
        _command_buffer, command->hint(),
        command->instance_mesh_handles(),
        command->instance_transforms(),
        _device->compacted_size_buffer_pool());
}

void MetalCommandEncoder::visit(const MeshUpdateCommand *command) noexcept {
    auto mesh = _device->mesh(command->handle());
    _command_buffer = mesh->update(_stream, _command_buffer);
}

void MetalCommandEncoder::visit(const MeshBuildCommand *command) noexcept {
    auto mesh = _device->mesh(command->handle());
    auto v_buffer = _device->buffer(command->vertex_buffer_handle());
    auto t_buffer = _device->buffer(command->triangle_buffer_handle());
    _command_buffer = mesh->build(
        _stream,
        _command_buffer, command->hint(),
        v_buffer, command->vertex_buffer_offset(), command->vertex_stride(),
        t_buffer, command->triangle_buffer_offset(), command->triangle_count(),
        _device->compacted_size_buffer_pool());
}

#else

void MetalCommandEncoder::visit(const AccelUpdateCommand *command) noexcept {
    LUISA_ERROR_WITH_LOCATION("Raytracing is not enabled for Metal backend.");
}

void MetalCommandEncoder::visit(const AccelBuildCommand *command) noexcept {
    LUISA_ERROR_WITH_LOCATION("Raytracing is not enabled for Metal backend.");
}

void MetalCommandEncoder::visit(const MeshUpdateCommand *command) noexcept {
    LUISA_ERROR_WITH_LOCATION("Raytracing is not enabled for Metal backend.");
}

void MetalCommandEncoder::visit(const MeshBuildCommand *command) noexcept {
    LUISA_ERROR_WITH_LOCATION("Raytracing is not enabled for Metal backend.");
}

#endif

}

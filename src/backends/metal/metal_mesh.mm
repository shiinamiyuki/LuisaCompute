//
// Created by Mike Smith on 2021/7/22.
//

#import <backends/metal/metal_stream.h>
#import <backends/metal/metal_mesh.h>
#import <backends/metal/metal_shared_buffer_pool.h>

namespace luisa::compute::metal {

id<MTLCommandBuffer> MetalMesh::build(
    MetalStream *stream,
    id<MTLCommandBuffer> command_buffer,
    AccelBuildHint hint,
    id<MTLBuffer> v_buffer, size_t v_offset, size_t v_stride,
    id<MTLBuffer> t_buffer, size_t t_offset, size_t t_count,
    MetalSharedBufferPool *pool) noexcept {

    auto mesh_desc = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
    mesh_desc.vertexBuffer = v_buffer;
    mesh_desc.vertexBufferOffset = v_offset;
    mesh_desc.vertexStride = v_stride;
    mesh_desc.indexBuffer = t_buffer;
    mesh_desc.indexBufferOffset = t_offset;
    mesh_desc.indexType = MTLIndexTypeUInt32;
    mesh_desc.triangleCount = t_count;
    mesh_desc.opaque = YES;
    auto descriptor = [MTLPrimitiveAccelerationStructureDescriptor descriptor];
    descriptor.geometryDescriptors = @[mesh_desc];
    _descriptor = descriptor;
    switch (hint) {
        case AccelBuildHint::FAST_TRACE: _descriptor.usage = MTLAccelerationStructureUsageNone; break;
        case AccelBuildHint::FAST_UPDATE: _descriptor.usage = MTLAccelerationStructureUsageRefit; break;
        case AccelBuildHint::FAST_REBUILD: _descriptor.usage = MTLAccelerationStructureUsagePreferFastBuild; break;
    }
    auto device = command_buffer.device;
    auto sizes = [device accelerationStructureSizesWithDescriptor:_descriptor];
    _update_scratch_size = sizes.refitScratchBufferSize;
    _handle = [device newAccelerationStructureWithSize:sizes.accelerationStructureSize];
    auto scratch_buffer = [device newBufferWithLength:sizes.buildScratchBufferSize
                                               options:MTLResourceStorageModePrivate
                                                       | MTLResourceHazardTrackingModeUntracked];
    auto command_encoder = [command_buffer accelerationStructureCommandEncoder];
    [command_encoder buildAccelerationStructure:_handle
                                     descriptor:_descriptor
                                  scratchBuffer:scratch_buffer
                            scratchBufferOffset:0u];
    if (hint != AccelBuildHint::FAST_REBUILD) {
        auto compacted_size_buffer = pool->allocate();
        [command_encoder writeCompactedAccelerationStructureSize:_handle
                                                        toBuffer:compacted_size_buffer.handle()
                                                          offset:compacted_size_buffer.offset()];
        [command_encoder endEncoding];
        auto compacted_size = 0u;
        auto p_compacted_size = &compacted_size;
        [command_buffer addCompletedHandler:^(id<MTLCommandBuffer>) {
          *p_compacted_size = *reinterpret_cast<const uint *>(
              static_cast<const std::byte *>(compacted_size_buffer.handle().contents)
              + compacted_size_buffer.offset());
          pool->recycle(compacted_size_buffer);
        }];
        stream->dispatch(command_buffer);
        [command_buffer waitUntilCompleted];
        auto accel_before_compaction = _handle;
        _handle = [device newAccelerationStructureWithSize:compacted_size];
        command_buffer = stream->command_buffer();
        command_encoder = [command_buffer accelerationStructureCommandEncoder];
        [command_encoder copyAndCompactAccelerationStructure:accel_before_compaction
                                     toAccelerationStructure:_handle];
    }
    [command_encoder endEncoding];
    return command_buffer;
}

id<MTLCommandBuffer> MetalMesh::update(
    MetalStream *,
    id<MTLCommandBuffer> command_buffer) {

    auto device = command_buffer.device;
    if (_update_buffer == nullptr || _update_buffer.length < _update_scratch_size) {
        _update_buffer = [device newBufferWithLength:_update_scratch_size
                                              options:MTLResourceStorageModePrivate];
    }
    auto command_encoder = [command_buffer accelerationStructureCommandEncoder];
    [command_encoder refitAccelerationStructure:_handle
                                     descriptor:_descriptor
                                    destination:_handle
                                  scratchBuffer:_update_buffer
                            scratchBufferOffset:0u];
    [command_encoder endEncoding];
    return command_buffer;
}

id<MTLBuffer> MetalMesh::vertex_buffer() const noexcept {
    auto geom_desc = static_cast<const MTLAccelerationStructureTriangleGeometryDescriptor *>(
        _descriptor.geometryDescriptors[0]);
    return geom_desc.vertexBuffer;
}

id<MTLBuffer> MetalMesh::triangle_buffer() const noexcept {
    auto geom_desc = static_cast<const MTLAccelerationStructureTriangleGeometryDescriptor *>(
        _descriptor.geometryDescriptors[0]);
    return geom_desc.indexBuffer;
}

}

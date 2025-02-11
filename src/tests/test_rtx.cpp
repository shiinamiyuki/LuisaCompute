//
// Created by Mike Smith on 2021/6/23.
//

#include <iostream>

#include <runtime/context.h>
#include <runtime/device.h>
#include <runtime/stream.h>
#include <runtime/event.h>
#include <dsl/syntax.h>
#include <rtx/accel.h>
#include <tests/fake_device.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tests/stb_image_write.h>

using namespace luisa;
using namespace luisa::compute;

int main(int argc, char *argv[]) {

    log_level_verbose();

    Context context{argv[0]};

#if defined(LUISA_BACKEND_METAL_ENABLED)
    auto device = context.create_device("metal", 1u);
#elif defined(LUISA_BACKEND_DX_ENABLED)
    auto device = context.create_device("dx");
#else
    auto device = FakeDevice::create(context);
#endif

    std::array vertices{
        float3(-0.5f, -0.5f, 0.0f),
        float3(0.5f, -0.5f, 0.0f),
        float3(0.0f, 0.5f, 0.0f)};
    std::array indices{0u, 1u, 2u};

    auto stream = device.create_stream();
    auto vertex_buffer = device.create_buffer<float3>(3u);
    auto triangle_buffer = device.create_buffer<Triangle>(1u);
    auto mesh = device.create_mesh();
    auto accel = device.create_accel();
    std::vector instances{mesh.handle(), mesh.handle()};
    std::vector transforms{scaling(1.5f),
                           translation(float3(-0.25f, 0.0f, 0.1f)) * rotation(float3(0.0f, 0.0f, 1.0f), 0.5f)};
    stream << vertex_buffer.copy_from(vertices.data())
           << triangle_buffer.copy_from(indices.data())
           << mesh.build(AccelBuildHint::FAST_TRACE, vertex_buffer, triangle_buffer)
           << accel.build(AccelBuildHint::FAST_TRACE, instances, transforms)
           << synchronize();

    Callable linear_to_srgb = [](Var<float3> x) noexcept {
        return select(1.055f * pow(x, 1.0f / 2.4f) - 0.055f,
                      12.92f * x,
                      x <= 0.00031308f);
    };

    Callable halton = [](UInt i, UInt b) noexcept {
        Var f = 1.0f;
        Var invB = 1.0f / b;
        Var r = 0.0f;
        while_(i > 0u, [&] {
            f = f * invB;
            r = r + f * (i % b);
            i = i / b;
        });
        return r;
    };

    Callable tea = [](UInt v0, UInt v1) noexcept {
        Var s0 = 0u;
        for (auto n = 0u; n < 4u; n++) {
            s0 += 0x9e3779b9u;
            v0 += ((v1 << 4) + 0xa341316cu) ^ (v1 + s0) ^ ((v1 >> 5u) + 0xc8013ea4u);
            v1 += ((v0 << 4) + 0xad90777du) ^ (v0 + s0) ^ ((v0 >> 5u) + 0x7e95761eu);
        }
        return v0;
    };

    Callable rand = [&](UInt f, UInt2 p) noexcept {
        Var i = tea(p.x, p.y) + f;
        Var rx = halton(i, 2u);
        Var ry = halton(i, 3u);
        return make_float2(rx, ry);
    };

    Kernel2D raytracing_kernel = [&](ImageFloat image, AccelVar accel, UInt frame_index) noexcept {
        Var coord = dispatch_id().xy();
        Var p = (make_float2(coord) + rand(frame_index, coord)) / make_float2(dispatch_size().xy()) * 2.0f - 1.0f;
        Var ray = make_ray(make_float3(p * make_float2(1.0f, -1.0f), 1.0f), make_float3(0.0f, 0.0f, -1.0f));
        Var hit = accel.trace_closest(ray);
        Var color = make_float3(0.3f, 0.5f, 0.7f);
        if_(!miss(hit), [&] {
            constexpr auto red = float3(1.0f, 0.0f, 0.0f);
            constexpr auto green = float3(0.0f, 1.0f, 0.0f);
            constexpr auto blue = float3(0.0f, 0.0f, 1.0f);
            color = interpolate(hit, red, green, blue);
        });
        Var old = image.read(coord).xyz();
        Var t = 1.0f / (frame_index + 1.0f);
        image.write(coord, make_float4(lerp(old, color, t), 1.0f));
    };

    Kernel2D colorspace_kernel = [&](ImageFloat hdr_image, ImageFloat ldr_image) noexcept {
        Var coord = dispatch_id().xy();
        Var hdr = hdr_image.read(coord).xyz();
        Var ldr = linear_to_srgb(hdr);
        ldr_image.write(coord, make_float4(ldr, 1.0f));
    };

    auto raytracing_shader = device.compile(raytracing_kernel);
    auto colorspace_shader = device.compile(colorspace_kernel);

    static constexpr auto width = 512u;
    static constexpr auto height = 512u;
    auto hdr_image = device.create_image<float>(PixelStorage::FLOAT4, width, height);
    auto ldr_image = device.create_image<float>(PixelStorage::BYTE4, width, height);
    std::vector<uint8_t> pixels(width * height * 4u);

    Clock clock;
    clock.tic();
    static constexpr auto spp = 1024u;
    for (auto i = 0u; i < spp; i++) {
        auto t = static_cast<float>(i) * (1.0f / spp);
        vertices[2].y = 0.5f - 0.2f * t;
        transforms[1] = translation(float3(-0.25f + t * 0.15f, 0.0f, 0.1f))
                        * rotation(float3(0.0f, 0.0f, 1.0f), 0.5f + t * 0.5f);
        stream << vertex_buffer.copy_from(vertices.data())
               << mesh.update()
               << accel.refit(1u, 1u, &transforms[1])
               << raytracing_shader(hdr_image, accel, i).dispatch(width, height);
    }
    stream << colorspace_shader(hdr_image, ldr_image).dispatch(width, height)
           << ldr_image.copy_to(pixels.data())
           << synchronize();
    auto time = clock.toc();
    LUISA_INFO("Time: {} ms", time);
    stbi_write_png("test_rtx.png", width, height, 4, pixels.data(), 0);
}

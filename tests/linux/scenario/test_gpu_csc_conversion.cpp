#include "bsv/buffer_allocator.h"
#include "bsv/gpu_csc.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace {

void FillNv12Pattern(bsv::IBuffer* buffer) {
    bsv::BufferMapping mapping;
    assert(buffer->Map(bsv::BufferAccessMode::kWrite, &mapping) == bsv::BsvError::kOk);
    auto* bytes = static_cast<uint8_t*>(mapping.data);
    const bsv::BufferDesc& desc = buffer->GetDesc();
    const size_t y_plane = static_cast<size_t>(desc.stride) * desc.height;
    for (uint32_t y = 0; y < desc.height; ++y) {
        for (uint32_t x = 0; x < desc.width; ++x) {
            bytes[y * desc.stride + x] = static_cast<uint8_t>((x + y) & 0xFF);
        }
    }
    uint8_t* uv = bytes + y_plane;
    for (uint32_t y = 0; y < desc.height / 2; ++y) {
        for (uint32_t x = 0; x < desc.width / 2; ++x) {
            const size_t offset = static_cast<size_t>(y) * desc.stride + x * 2;
            uv[offset] = 128;
            uv[offset + 1] = 128;
        }
    }
    buffer->Unmap(&mapping);
}

void FillNv21Pattern(bsv::IBuffer* buffer) {
    bsv::BufferMapping mapping;
    assert(buffer->Map(bsv::BufferAccessMode::kWrite, &mapping) == bsv::BsvError::kOk);
    auto* bytes = static_cast<uint8_t*>(mapping.data);
    const bsv::BufferDesc& desc = buffer->GetDesc();
    const size_t y_plane = static_cast<size_t>(desc.stride) * desc.height;
    for (uint32_t y = 0; y < desc.height; ++y) {
        for (uint32_t x = 0; x < desc.width; ++x) {
            bytes[y * desc.stride + x] = static_cast<uint8_t>((x * 3 + y * 5) & 0xFF);
        }
    }
    uint8_t* vu = bytes + y_plane;
    for (uint32_t y = 0; y < desc.height / 2; ++y) {
        for (uint32_t x = 0; x < desc.width / 2; ++x) {
            const size_t offset = static_cast<size_t>(y) * desc.stride + x * 2;
            vu[offset] = 128;
            vu[offset + 1] = 128;
        }
    }
    buffer->Unmap(&mapping);
}

}  // namespace

int main() {
    bsv::LinuxBufferAllocator allocator;
    assert(allocator.Initialize() == bsv::BsvError::kOk);

    const uint32_t width = 128;
    const uint32_t height = 72;
    const uint32_t stride = 128;

    bsv::IBuffer* src_nv12 = nullptr;
    bsv::BufferDesc src_desc_nv12{width, height, stride, bsv::PixelFormat::kNV12, 0};
    assert(allocator.Allocate(src_desc_nv12, &src_nv12) == bsv::BsvError::kOk);
    FillNv12Pattern(src_nv12);

    bsv::IBuffer* dst_nv12 = nullptr;
    bsv::BufferDesc dst_desc{width, height, width, bsv::PixelFormat::kRGBA8888, 0};
    assert(allocator.Allocate(dst_desc, &dst_nv12) == bsv::BsvError::kOk);

    bsv::LinuxGpuCscConverter converter_nv12;
    bsv::CscConfig config_nv12{bsv::PixelFormat::kNV12, bsv::PixelFormat::kRGBA8888};
    assert(converter_nv12.Initialize(config_nv12) == bsv::BsvError::kOk);
    assert(converter_nv12.Start() == bsv::BsvError::kOk);
    assert(converter_nv12.ConvertFrame(*src_nv12, *dst_nv12) == bsv::BsvError::kOk);
    converter_nv12.Shutdown();

    bsv::BufferMapping dst_nv12_map;
    assert(dst_nv12->Map(bsv::BufferAccessMode::kRead, &dst_nv12_map) == bsv::BsvError::kOk);
    auto* dst_bytes = static_cast<uint8_t*>(dst_nv12_map.data);
    bool any_nonzero = false;
    for (size_t i = 0; i < dst_nv12_map.size; ++i) {
        if (dst_bytes[i] != 0) {
            any_nonzero = true;
            break;
        }
    }
    dst_nv12->Unmap(&dst_nv12_map);
    assert(any_nonzero);

    allocator.Release(src_nv12);
    allocator.Release(dst_nv12);

    bsv::IBuffer* src_nv21 = nullptr;
    bsv::BufferDesc src_desc_nv21{width, height, stride, bsv::PixelFormat::kNV21, 0};
    assert(allocator.Allocate(src_desc_nv21, &src_nv21) == bsv::BsvError::kOk);
    FillNv21Pattern(src_nv21);

    bsv::IBuffer* dst_nv21 = nullptr;
    assert(allocator.Allocate(dst_desc, &dst_nv21) == bsv::BsvError::kOk);

    bsv::LinuxGpuCscConverter converter_nv21;
    bsv::CscConfig config_nv21{bsv::PixelFormat::kNV21, bsv::PixelFormat::kRGBA8888};
    assert(converter_nv21.Initialize(config_nv21) == bsv::BsvError::kOk);
    assert(converter_nv21.Start() == bsv::BsvError::kOk);
    assert(converter_nv21.ConvertFrame(*src_nv21, *dst_nv21) == bsv::BsvError::kOk);
    converter_nv21.Shutdown();

    bsv::BufferMapping dst_nv21_map;
    assert(dst_nv21->Map(bsv::BufferAccessMode::kRead, &dst_nv21_map) == bsv::BsvError::kOk);
    dst_bytes = static_cast<uint8_t*>(dst_nv21_map.data);
    any_nonzero = false;
    for (size_t i = 0; i < dst_nv21_map.size; ++i) {
        if (dst_bytes[i] != 0) {
            any_nonzero = true;
            break;
        }
    }
    dst_nv21->Unmap(&dst_nv21_map);
    assert(any_nonzero);

    allocator.Release(src_nv21);
    allocator.Release(dst_nv21);
    allocator.Shutdown();
    return 0;
}

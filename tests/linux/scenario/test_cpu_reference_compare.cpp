#include "bsv/gpu_csc.h"
#include "bsv/linux_buffer.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

void FillNv12Gradient(bsv::IBuffer* buffer) {
    bsv::BufferMapping mapping;
    assert(buffer->Map(bsv::BufferAccessMode::kWrite, &mapping) == bsv::BsvError::kOk);
    auto* bytes = static_cast<uint8_t*>(mapping.data);
    const bsv::BufferDesc& desc = buffer->GetDesc();
    const size_t y_plane = static_cast<size_t>(desc.stride) * desc.height;
    for (uint32_t y = 0; y < desc.height; ++y) {
        for (uint32_t x = 0; x < desc.width; ++x) {
            bytes[y * desc.stride + x] = static_cast<uint8_t>((x * 7 + y * 11) & 0xFF);
        }
    }
    uint8_t* uv = bytes + y_plane;
    for (uint32_t y = 0; y < desc.height / 2; ++y) {
        for (uint32_t x = 0; x < desc.width / 2; ++x) {
            const size_t offset = static_cast<size_t>(y) * desc.stride + x * 2;
            uv[offset] = 90;
            uv[offset + 1] = 160;
        }
    }
    buffer->Unmap(&mapping);
}

void FillNv21Gradient(bsv::IBuffer* buffer) {
    bsv::BufferMapping mapping;
    assert(buffer->Map(bsv::BufferAccessMode::kWrite, &mapping) == bsv::BsvError::kOk);
    auto* bytes = static_cast<uint8_t*>(mapping.data);
    const bsv::BufferDesc& desc = buffer->GetDesc();
    const size_t y_plane = static_cast<size_t>(desc.stride) * desc.height;
    for (uint32_t y = 0; y < desc.height; ++y) {
        for (uint32_t x = 0; x < desc.width; ++x) {
            bytes[y * desc.stride + x] = static_cast<uint8_t>((x * 9 + y * 13) & 0xFF);
        }
    }
    uint8_t* vu = bytes + y_plane;
    for (uint32_t y = 0; y < desc.height / 2; ++y) {
        for (uint32_t x = 0; x < desc.width / 2; ++x) {
            const size_t offset = static_cast<size_t>(y) * desc.stride + x * 2;
            vu[offset] = 110;
            vu[offset + 1] = 140;
        }
    }
    buffer->Unmap(&mapping);
}

void CompareBuffers(const bsv::IBuffer& gpu, const std::vector<uint8_t>& cpu, uint32_t width, uint32_t height, uint32_t stride, uint8_t tolerance) {
    bsv::BufferMapping mapping;
    bsv::IBuffer& mutable_gpu = const_cast<bsv::IBuffer&>(gpu);
    assert(mutable_gpu.Map(bsv::BufferAccessMode::kRead, &mapping) == bsv::BsvError::kOk);
    const auto* gpu_bytes = static_cast<const uint8_t*>(mapping.data);
    for (uint32_t y = 0; y < height; ++y) {
        const size_t row_offset = static_cast<size_t>(y) * stride * 4;
        for (uint32_t x = 0; x < width; ++x) {
            const size_t index = row_offset + x * 4;
            for (size_t c = 0; c < 4; ++c) {
                const uint8_t g = gpu_bytes[index + c];
                const uint8_t r = cpu[index + c];
                const uint8_t diff = static_cast<uint8_t>(g > r ? g - r : r - g);
                assert(diff <= tolerance);
            }
        }
    }
    mutable_gpu.Unmap(&mapping);
}

}  // namespace

int main() {
    bsv::LinuxBufferAllocator allocator;
    assert(allocator.Initialize() == bsv::BsvError::kOk);

    const uint32_t width = 64;
    const uint32_t height = 64;
    const uint32_t stride = 64;
    const uint8_t tolerance = 2;

    bsv::BufferDesc nv12_desc{width, height, stride, bsv::PixelFormat::kNV12, 0};
    bsv::IBuffer* nv12_src = nullptr;
    assert(allocator.Allocate(nv12_desc, &nv12_src) == bsv::BsvError::kOk);
    FillNv12Gradient(nv12_src);

    bsv::BufferDesc rgba_desc{width, height, width, bsv::PixelFormat::kRGBA8888, 0};
    bsv::IBuffer* nv12_dst = nullptr;
    assert(allocator.Allocate(rgba_desc, &nv12_dst) == bsv::BsvError::kOk);

    bsv::LinuxGpuCscConverter converter_nv12;
    bsv::CscConfig config_nv12{bsv::PixelFormat::kNV12, bsv::PixelFormat::kRGBA8888};
    assert(converter_nv12.Initialize(config_nv12) == bsv::BsvError::kOk);
    assert(converter_nv12.Start() == bsv::BsvError::kOk);
    assert(converter_nv12.ConvertFrame(*nv12_src, *nv12_dst) == bsv::BsvError::kOk);
    std::vector<uint8_t> nv12_cpu = bsv::ConvertNv12ToRgba(*nv12_src);
    CompareBuffers(*nv12_dst, nv12_cpu, width, height, width, tolerance);
    converter_nv12.Shutdown();

    allocator.Release(nv12_src);
    allocator.Release(nv12_dst);

    bsv::BufferDesc nv21_desc{width, height, stride, bsv::PixelFormat::kNV21, 0};
    bsv::IBuffer* nv21_src = nullptr;
    assert(allocator.Allocate(nv21_desc, &nv21_src) == bsv::BsvError::kOk);
    FillNv21Gradient(nv21_src);

    bsv::IBuffer* nv21_dst = nullptr;
    assert(allocator.Allocate(rgba_desc, &nv21_dst) == bsv::BsvError::kOk);

    bsv::LinuxGpuCscConverter converter_nv21;
    bsv::CscConfig config_nv21{bsv::PixelFormat::kNV21, bsv::PixelFormat::kRGBA8888};
    assert(converter_nv21.Initialize(config_nv21) == bsv::BsvError::kOk);
    assert(converter_nv21.Start() == bsv::BsvError::kOk);
    assert(converter_nv21.ConvertFrame(*nv21_src, *nv21_dst) == bsv::BsvError::kOk);
    std::vector<uint8_t> nv21_cpu = bsv::ConvertNv21ToRgba(*nv21_src);
    CompareBuffers(*nv21_dst, nv21_cpu, width, height, width, tolerance);
    converter_nv21.Shutdown();

    allocator.Release(nv21_src);
    allocator.Release(nv21_dst);
    allocator.Shutdown();
    return 0;
}

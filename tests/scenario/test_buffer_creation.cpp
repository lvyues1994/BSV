#include "bsv/buffer.h"

#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    bsv::LinuxBufferAllocator allocator;
    assert(allocator.Initialize() == bsv::BsvError::kOk);

    bsv::BufferDesc desc;
    desc.width = 640;
    desc.height = 480;
    desc.stride = 640;
    desc.format = bsv::PixelFormat::kNV12;

    bsv::IBuffer* internal_buffer = nullptr;
    assert(allocator.Allocate(desc, &internal_buffer) == bsv::BsvError::kOk);
    assert(internal_buffer != nullptr);
    assert(internal_buffer->GetDesc().width == desc.width);
    assert(internal_buffer->GetDesc().height == desc.height);
    assert(internal_buffer->GetDesc().stride == desc.stride);
    assert(internal_buffer->GetDesc().format == desc.format);
    allocator.Release(internal_buffer);

    std::vector<uint8_t> external_storage(desc.height * desc.stride * 2, 0);
    bsv::PlatformHandle handle;
    handle.type = bsv::PlatformHandleType::kHostMemory;
    handle.handle = external_storage.data();
    handle.size = external_storage.size();
    handle.desc = desc;

    bsv::IBuffer* external_buffer = nullptr;
    assert(allocator.ImportFromHandle(handle, &external_buffer) == bsv::BsvError::kOk);
    assert(external_buffer != nullptr);
    assert(external_buffer->GetDesc().width == desc.width);
    assert(external_buffer->GetDesc().height == desc.height);
    assert(external_buffer->GetDesc().stride == desc.stride);
    assert(external_buffer->GetDesc().format == desc.format);
    allocator.Release(external_buffer);

    allocator.Shutdown();
    return 0;
}

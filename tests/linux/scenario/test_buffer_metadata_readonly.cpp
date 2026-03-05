#include "bsv/buffer_allocator.h"

#include <cassert>

int main() {
    bsv::LinuxBufferAllocator allocator;
    assert(allocator.Initialize() == bsv::BsvError::kOk);

    bsv::BufferDesc desc;
    desc.width = 320;
    desc.height = 240;
    desc.stride = 320;
    desc.format = bsv::PixelFormat::kRGBA8888;

    bsv::IBuffer* buffer = nullptr;
    assert(allocator.Allocate(desc, &buffer) == bsv::BsvError::kOk);
    assert(buffer != nullptr);

    const bsv::BufferDesc& out_desc = buffer->GetDesc();
    assert(out_desc.width == desc.width);
    assert(out_desc.height == desc.height);
    assert(out_desc.stride == desc.stride);
    assert(out_desc.format == desc.format);

    bsv::BufferMapping mapping;
    assert(buffer->Map(bsv::BufferAccessMode::kRead, &mapping) == bsv::BsvError::kOk);
    assert(mapping.data != nullptr);
    buffer->Unmap(&mapping);

    const bsv::BufferDesc& out_desc_after = buffer->GetDesc();
    assert(out_desc_after.width == desc.width);
    assert(out_desc_after.height == desc.height);
    assert(out_desc_after.stride == desc.stride);
    assert(out_desc_after.format == desc.format);

    allocator.Release(buffer);
    allocator.Shutdown();
    return 0;
}

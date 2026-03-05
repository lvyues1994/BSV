#include "bsv/buffer_allocator.h"

#include <cassert>
#include <cstdint>

int main() {
    bsv::LinuxBufferAllocator allocator;
    assert(allocator.Initialize() == bsv::BsvError::kOk);

    bsv::BufferDesc desc;
    desc.width = 128;
    desc.height = 72;
    desc.stride = 128;
    desc.format = bsv::PixelFormat::kNV21;

    bsv::IBuffer* buffer = nullptr;
    assert(allocator.Allocate(desc, &buffer) == bsv::BsvError::kOk);
    assert(buffer != nullptr);

    bsv::BufferMapping write_mapping;
    assert(buffer->Map(bsv::BufferAccessMode::kWrite, &write_mapping) == bsv::BsvError::kOk);
    assert(write_mapping.data != nullptr);
    auto* bytes = static_cast<uint8_t*>(write_mapping.data);
    bytes[0] = 0xAB;
    buffer->Unmap(&write_mapping);

    bsv::BufferMapping read_mapping;
    assert(buffer->Map(bsv::BufferAccessMode::kRead, &read_mapping) == bsv::BsvError::kOk);
    assert(read_mapping.data != nullptr);
    auto* read_bytes = static_cast<const uint8_t*>(read_mapping.data);
    assert(read_bytes[0] == 0xAB);
    buffer->Unmap(&read_mapping);

    bsv::BufferMapping read_mapping_a;
    bsv::BufferMapping read_mapping_b;
    assert(buffer->Map(bsv::BufferAccessMode::kRead, &read_mapping_a) == bsv::BsvError::kOk);
    assert(buffer->Map(bsv::BufferAccessMode::kRead, &read_mapping_b) == bsv::BsvError::kOk);
    buffer->Unmap(&read_mapping_b);
    buffer->Unmap(&read_mapping_a);

    allocator.Release(buffer);
    allocator.Shutdown();
    return 0;
}

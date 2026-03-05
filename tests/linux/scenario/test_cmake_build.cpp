#include "bsv/buffer_allocator.h"

#include <cassert>
#include <cstddef>

int main() {
    bsv::LinuxBufferAllocator allocator;
    assert(allocator.Initialize() == bsv::BsvError::kOk);

    bsv::BufferDesc desc{16, 16, 16, bsv::PixelFormat::kRGBA8888, 0};
    bsv::IBuffer* buffer = nullptr;
    assert(allocator.Allocate(desc, &buffer) == bsv::BsvError::kOk);
    assert(buffer != nullptr);
    assert(buffer->Size() == static_cast<size_t>(16 * 16 * 4));

    allocator.Release(buffer);
    allocator.Shutdown();
    return 0;
}

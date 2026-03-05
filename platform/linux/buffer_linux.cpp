#include "bsv/buffer_allocator.h"
#include "bsv/linux_buffer.h"

#include <cstring>
#include <new>

namespace bsv {

namespace {

BufferDesc ResolveDesc(BufferDesc desc) {
    if (desc.size_in_bytes == 0) {
        desc.size_in_bytes = static_cast<size_t>(desc.stride) * desc.height;
        if (desc.format == PixelFormat::kRGBA8888) {
            desc.size_in_bytes *= 4;
        } else if (desc.format == PixelFormat::kNV12 || desc.format == PixelFormat::kNV21) {
            desc.size_in_bytes = static_cast<size_t>(desc.stride) * desc.height * 3 / 2;
        }
    }
    return desc;
}

}  // namespace

LinuxBuffer::LinuxBuffer(const BufferDesc& desc, void* data, bool owns_memory)
    : desc_(ResolveDesc(desc)), data_(data), owns_memory_(owns_memory) {}

LinuxBuffer::~LinuxBuffer() {
    if (owns_memory_ && data_ != nullptr) {
        delete[] static_cast<uint8_t*>(data_);
    }
}

const BufferDesc& LinuxBuffer::GetDesc() const {
    return desc_;
}

const PlatformHandle* LinuxBuffer::GetPlatformHandle() const {
    return nullptr;
}

void* LinuxBuffer::Data() {
    return data_;
}

const void* LinuxBuffer::Data() const {
    return data_;
}

size_t LinuxBuffer::Size() const {
    return desc_.size_in_bytes;
}

BsvError LinuxBuffer::Map(BufferAccessMode mode, BufferMapping* mapping) {
    if (mapping == nullptr || data_ == nullptr) {
        return BsvError::kInvalidArgument;
    }
    if (mode == BufferAccessMode::kWrite || mode == BufferAccessMode::kReadWrite) {
        if (active_writers_ > 0 || active_readers_ > 0) {
            return BsvError::kInvalidState;
        }
        ++active_writers_;
    } else {
        if (active_writers_ > 0) {
            return BsvError::kInvalidState;
        }
        ++active_readers_;
    }
    mapping->data = data_;
    mapping->size = desc_.size_in_bytes;
    mapping->mode = mode;
    return BsvError::kOk;
}

void LinuxBuffer::Unmap(BufferMapping* mapping) {
    if (mapping == nullptr) {
        return;
    }
    if (mapping->mode == BufferAccessMode::kWrite || mapping->mode == BufferAccessMode::kReadWrite) {
        if (active_writers_ > 0) {
            --active_writers_;
        }
    } else {
        if (active_readers_ > 0) {
            --active_readers_;
        }
    }
    mapping->data = nullptr;
    mapping->size = 0;
}

BsvError LinuxBufferAllocator::Initialize() {
    return BsvError::kOk;
}

void LinuxBufferAllocator::Shutdown() {}

BsvError LinuxBufferAllocator::Allocate(const BufferDesc& desc, IBuffer** out_buffer) {
    if (out_buffer == nullptr || desc.width == 0 || desc.height == 0 || desc.stride == 0) {
        return BsvError::kInvalidArgument;
    }
    BufferDesc resolved = ResolveDesc(desc);
    uint8_t* data = new (std::nothrow) uint8_t[resolved.size_in_bytes];
    if (data == nullptr) {
        return BsvError::kOutOfMemory;
    }
    std::memset(data, 0, resolved.size_in_bytes);
    *out_buffer = new (std::nothrow) LinuxBuffer(resolved, data, true);
    if (*out_buffer == nullptr) {
        delete[] data;
        return BsvError::kOutOfMemory;
    }
    return BsvError::kOk;
}

BsvError LinuxBufferAllocator::ImportFromHandle(const PlatformHandle& handle, IBuffer** out_buffer) {
    if (out_buffer == nullptr || handle.handle == nullptr) {
        return BsvError::kInvalidArgument;
    }
    if (handle.type == PlatformHandleType::kUnknown) {
        return BsvError::kInvalidArgument;
    }
    BufferDesc resolved = ResolveDesc(handle.desc);
    if (resolved.size_in_bytes == 0) {
        resolved.size_in_bytes = handle.size;
    }
    *out_buffer = new (std::nothrow) LinuxBuffer(resolved, handle.handle, false);
    if (*out_buffer == nullptr) {
        return BsvError::kOutOfMemory;
    }
    return BsvError::kOk;
}

void LinuxBufferAllocator::Release(IBuffer* buffer) {
    delete buffer;
}

}  // namespace bsv

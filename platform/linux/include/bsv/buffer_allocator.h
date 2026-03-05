#ifndef BSV_PLATFORM_LINUX_BUFFER_ALLOCATOR_H
#define BSV_PLATFORM_LINUX_BUFFER_ALLOCATOR_H

#include "bsv/buffer.h"
#include "bsv/linux_buffer.h"

namespace bsv {

class LinuxBufferAllocator final : public IBufferAllocator {
public:
    BsvError Initialize() override;
    void Shutdown() override;
    BsvError Allocate(const BufferDesc& desc, IBuffer** out_buffer) override;
    BsvError ImportFromHandle(const PlatformHandle& handle, IBuffer** out_buffer) override;
    void Release(IBuffer* buffer) override;
};

}  // namespace bsv

#endif  // BSV_PLATFORM_LINUX_BUFFER_ALLOCATOR_H

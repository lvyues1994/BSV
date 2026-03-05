#ifndef BSV_BUFFER_H
#define BSV_BUFFER_H

#include "bsv/core_types.h"

namespace bsv {

class IBufferAllocator {
public:
    virtual ~IBufferAllocator() = default;
    virtual BsvError Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual BsvError Allocate(const BufferDesc& desc, IBuffer** out_buffer) = 0;
    virtual BsvError ImportFromHandle(const PlatformHandle& handle, IBuffer** out_buffer) = 0;
    virtual void Release(IBuffer* buffer) = 0;
};

}  // namespace bsv

#endif  // BSV_BUFFER_H

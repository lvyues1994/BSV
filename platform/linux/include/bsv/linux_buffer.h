#ifndef BSV_PLATFORM_LINUX_BUFFER_H
#define BSV_PLATFORM_LINUX_BUFFER_H

#include "bsv/buffer.h"

namespace bsv {

class LinuxBuffer final : public IBuffer {
public:
    LinuxBuffer(const BufferDesc& desc, void* data, bool owns_memory);
    ~LinuxBuffer() override;

    const BufferDesc& GetDesc() const override;
    const PlatformHandle* GetPlatformHandle() const override;
    void* Data() override;
    const void* Data() const override;
    size_t Size() const override;
    BsvError Map(BufferAccessMode mode, BufferMapping* mapping) override;
    void Unmap(BufferMapping* mapping) override;

private:
    BufferDesc desc_{};
    void* data_ = nullptr;
    bool owns_memory_ = false;
    size_t active_readers_ = 0;
    size_t active_writers_ = 0;
};

}  // namespace bsv

#endif  // BSV_PLATFORM_LINUX_BUFFER_H

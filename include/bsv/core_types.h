#ifndef BSV_CORE_TYPES_H
#define BSV_CORE_TYPES_H

#include <cstddef>
#include <cstdint>

namespace bsv {

enum class PixelFormat : uint32_t {
    kUnknown = 0,
    kNV21,
    kNV12,
    kRGBA8888,
};

enum class PlatformHandleType : uint32_t {
    kUnknown = 0,
    kDmaBuf,
    kAndroidHardwareBuffer,
    kHostMemory,
};

struct BufferDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    PixelFormat format = PixelFormat::kUnknown;
    size_t size_in_bytes = 0;
};

struct PlatformHandle {
    PlatformHandleType type = PlatformHandleType::kUnknown;
    void* handle = nullptr;
    size_t size = 0;
    BufferDesc desc;
};

enum class BufferAccessMode : uint32_t {
    kRead = 0,
    kWrite,
    kReadWrite,
};

struct BufferMapping {
    void* data = nullptr;
    size_t size = 0;
    BufferAccessMode mode = BufferAccessMode::kRead;
};

enum class BsvState : uint32_t {
    kClosed = 0,
    kOpening,
    kRunning,
    kClosing,
};

enum class BsvError : int32_t {
    kOk = 0,
    kInvalidArgument = -1,
    kInvalidState = -2,
    kNotInitialized = -3,
    kAlreadyStarted = -4,
    kNotSupported = -5,
    kOutOfMemory = -6,
    kInternal = -7,
};

class IBuffer {
public:
    virtual ~IBuffer() = default;
    virtual const BufferDesc& GetDesc() const = 0;
    virtual void* Data() = 0;
    virtual const void* Data() const = 0;
    virtual size_t Size() const = 0;
    virtual BsvError Map(BufferAccessMode mode, BufferMapping* mapping) = 0;
    virtual void Unmap(BufferMapping* mapping) = 0;
};

class IBufferAllocator {
public:
    virtual ~IBufferAllocator() = default;
    virtual BsvError Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual BsvError Allocate(const BufferDesc& desc, IBuffer** out_buffer) = 0;
    virtual BsvError ImportFromHandle(const PlatformHandle& handle, IBuffer** out_buffer) = 0;
    virtual void Release(IBuffer* buffer) = 0;
};

struct CameraConfig {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t fps = 0;
    PixelFormat output_format = PixelFormat::kUnknown;
};

struct CscConfig {
    PixelFormat input_format = PixelFormat::kUnknown;
    PixelFormat output_format = PixelFormat::kUnknown;
};

class ICameraProvider {
public:
    using FrameCallback = void (*)(const IBuffer& buffer, void* user_data);

    virtual ~ICameraProvider() = default;
    virtual BsvError Initialize(const CameraConfig& config) = 0;
    virtual BsvError Start() = 0;
    virtual BsvError Stop() = 0;
    virtual void Shutdown() = 0;
    virtual BsvError SetFrameCallback(FrameCallback callback, void* user_data) = 0;
};

class ICscConverter {
public:
    virtual ~ICscConverter() = default;
    virtual BsvError Initialize(const CscConfig& config) = 0;
    virtual BsvError Start() = 0;
    virtual BsvError Stop() = 0;
    virtual void Shutdown() = 0;
    virtual BsvError ConvertFrame(const IBuffer& src, IBuffer& dst) = 0;
};

}  // namespace bsv

#endif  // BSV_CORE_TYPES_H

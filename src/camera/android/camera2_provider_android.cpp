#include "bsv/camera2_provider.h"

#include <android/hardware_buffer.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCaptureSession.h>
#include <media/NdkImageReader.h>

#include <cstring>
#include <vector>

namespace bsv {

namespace {

BsvError MapCameraStatus(camera_status_t status) {
    switch (status) {
        case ACAMERA_OK:
            return BsvError::kOk;
        case ACAMERA_ERROR_INVALID_PARAMETER:
            return BsvError::kInvalidArgument;
        case ACAMERA_ERROR_CAMERA_DISCONNECTED:
            return BsvError::kInternal;
        case ACAMERA_ERROR_CAMERA_IN_USE:
            return BsvError::kInvalidState;
        case ACAMERA_ERROR_MAX_CAMERAS_IN_USE:
            return BsvError::kInvalidState;
        case ACAMERA_ERROR_CAMERA_DISABLED:
            return BsvError::kNotSupported;
        case ACAMERA_ERROR_PERMISSION_DENIED:
            return BsvError::kInvalidState;
        default:
            return BsvError::kInternal;
    }
}

uint32_t ComputeBufferSize(const BufferDesc& desc) {
    if (desc.size_in_bytes != 0) {
        return static_cast<uint32_t>(desc.size_in_bytes);
    }
    if (desc.format == PixelFormat::kRGBA8888) {
        return desc.stride * desc.height * 4;
    }
    if (desc.format == PixelFormat::kNV12 || desc.format == PixelFormat::kNV21) {
        return desc.stride * desc.height * 3 / 2;
    }
    return desc.stride * desc.height;
}

PixelFormat ResolvePixelFormat(PixelFormat requested) {
    if (requested == PixelFormat::kNV12 || requested == PixelFormat::kNV21 ||
        requested == PixelFormat::kRGBA8888) {
        return requested;
    }
    return PixelFormat::kUnknown;
}

int ResolveImageFormat(PixelFormat requested) {
    if (requested == PixelFormat::kRGBA8888) {
        return AIMAGE_FORMAT_RGBA_8888;
    }
    return AIMAGE_FORMAT_YUV_420_888;
}

class AndroidCamera2Buffer final : public IBuffer {
public:
    AndroidCamera2Buffer(AImage* image, PixelFormat format) : image_(image) {
        std::memset(&handle_desc_, 0, sizeof(handle_desc_));
        if (image_ != nullptr) {
            AImage_getWidth(image_, reinterpret_cast<int32_t*>(&handle_desc_.width));
            AImage_getHeight(image_, reinterpret_cast<int32_t*>(&handle_desc_.height));
        }
        if (image_ != nullptr) {
            AImage_getHardwareBuffer(image_, &hardware_buffer_);
        }
        if (hardware_buffer_ != nullptr) {
            AHardwareBuffer_acquire(hardware_buffer_);
            AHardwareBuffer_Desc ah_desc{};
            AHardwareBuffer_describe(hardware_buffer_, &ah_desc);
            handle_desc_.width = ah_desc.width;
            handle_desc_.height = ah_desc.height;
            handle_desc_.stride = ah_desc.stride;
        } else {
            handle_desc_.stride = handle_desc_.width;
        }
        handle_desc_.format = format;
        handle_desc_.size_in_bytes = ComputeBufferSize(handle_desc_);
        handle_.type = PlatformHandleType::kAndroidHardwareBuffer;
        handle_.handle = hardware_buffer_;
        handle_.size = handle_desc_.size_in_bytes;
        handle_.desc = handle_desc_;
    }

    ~AndroidCamera2Buffer() override {
        if (hardware_buffer_ != nullptr) {
            AHardwareBuffer_release(hardware_buffer_);
        }
        if (image_ != nullptr) {
            AImage_delete(image_);
        }
    }

    const BufferDesc& GetDesc() const override {
        return handle_desc_;
    }

    const PlatformHandle* GetPlatformHandle() const override {
        return &handle_;
    }

    void* Data() override {
        return nullptr;
    }

    const void* Data() const override {
        return nullptr;
    }

    size_t Size() const override {
        return handle_desc_.size_in_bytes;
    }

    BsvError Map(BufferAccessMode mode, BufferMapping* mapping) override {
        if (mapping == nullptr || hardware_buffer_ == nullptr) {
            return BsvError::kInvalidArgument;
        }
        if (mode != BufferAccessMode::kRead) {
            return BsvError::kNotSupported;
        }
        void* data = nullptr;
        const int32_t result = AHardwareBuffer_lock(
            hardware_buffer_, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, -1, nullptr, &data);
        if (result != 0 || data == nullptr) {
            return BsvError::kInternal;
        }
        mapping->data = data;
        mapping->size = handle_desc_.size_in_bytes;
        mapping->mode = mode;
        return BsvError::kOk;
    }

    void Unmap(BufferMapping* mapping) override {
        if (mapping == nullptr || hardware_buffer_ == nullptr) {
            return;
        }
        AHardwareBuffer_unlock(hardware_buffer_, nullptr);
        mapping->data = nullptr;
        mapping->size = 0;
    }

private:
    AImage* image_ = nullptr;
    AHardwareBuffer* hardware_buffer_ = nullptr;
    BufferDesc handle_desc_{};
    PlatformHandle handle_{};
};

void OnImageAvailable(void* context, AImageReader* reader) {
    auto* provider = static_cast<AndroidCamera2Provider*>(context);
    if (provider == nullptr || reader == nullptr) {
        return;
    }
    provider->OnImageAvailable(reader);
}

void OnCameraDisconnected(void* context, ACameraDevice* device) {
    (void)device;
    auto* provider = static_cast<AndroidCamera2Provider*>(context);
    if (provider != nullptr) {
        provider->OnCameraDisconnected();
    }
}

void OnCameraError(void* context, ACameraDevice* device, int error) {
    (void)device;
    auto* provider = static_cast<AndroidCamera2Provider*>(context);
    if (provider != nullptr) {
        provider->OnCameraError(error);
    }
}

}  // namespace

AndroidCamera2Provider::AndroidCamera2Provider() = default;

AndroidCamera2Provider::~AndroidCamera2Provider() {
    Shutdown();
}

BsvError AndroidCamera2Provider::ValidateConfig(const CameraConfig& config) {
    if (config.width == 0 || config.height == 0 || config.fps == 0) {
        return BsvError::kInvalidArgument;
    }
    if (ResolvePixelFormat(config.output_format) == PixelFormat::kUnknown) {
        return BsvError::kNotSupported;
    }
    return BsvError::kOk;
}

BsvError AndroidCamera2Provider::Initialize(const CameraConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        return BsvError::kInvalidState;
    }
    BsvError status = ValidateConfig(config);
    if (status != BsvError::kOk) {
        return status;
    }
    config_ = config;
    if (config_.camera_id != nullptr) {
        active_camera_id_ = config_.camera_id;
    }
    camera_manager_ = ACameraManager_create();
    if (camera_manager_ == nullptr) {
        return BsvError::kInternal;
    }
    status = CreateImageReaderLocked();
    if (status != BsvError::kOk) {
        ACameraManager_delete(reinterpret_cast<ACameraManager*>(camera_manager_));
        camera_manager_ = nullptr;
        return status;
    }
    initialized_ = true;
    state_ = BsvState::kClosed;
    return BsvError::kOk;
}

BsvError AndroidCamera2Provider::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        return BsvError::kNotInitialized;
    }
    if (state_ == BsvState::kRunning) {
        return BsvError::kAlreadyStarted;
    }
    state_ = BsvState::kOpening;
    BsvError status = OpenCameraLocked();
    if (status != BsvError::kOk) {
        state_ = BsvState::kClosed;
        return status;
    }
    status = CreateCaptureSessionLocked();
    if (status != BsvError::kOk) {
        CloseSessionLocked();
        CloseCameraLocked();
        state_ = BsvState::kClosed;
        return status;
    }
    state_ = BsvState::kRunning;
    return BsvError::kOk;
}

BsvError AndroidCamera2Provider::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != BsvState::kRunning) {
        return BsvError::kInvalidState;
    }
    state_ = BsvState::kClosing;
    CloseSessionLocked();
    CloseCameraLocked();
    state_ = BsvState::kClosed;
    return BsvError::kOk;
}

void AndroidCamera2Provider::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        return;
    }
    CloseSessionLocked();
    CloseCameraLocked();
    ReleaseImageReaderLocked();
    if (camera_manager_ != nullptr) {
        ACameraManager_delete(reinterpret_cast<ACameraManager*>(camera_manager_));
        camera_manager_ = nullptr;
    }
    initialized_ = false;
    state_ = BsvState::kClosed;
}

BsvError AndroidCamera2Provider::SetFrameCallback(FrameCallback callback, void* user_data) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
    callback_user_data_ = user_data;
    return BsvError::kOk;
}

BsvError AndroidCamera2Provider::RequestCameraSwitch(const char* camera_id) {
    if (camera_id == nullptr) {
        return BsvError::kInvalidArgument;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != BsvState::kRunning) {
        return BsvError::kInvalidState;
    }
    state_ = BsvState::kClosing;
    CloseSessionLocked();
    CloseCameraLocked();
    active_camera_id_ = camera_id;
    state_ = BsvState::kOpening;
    BsvError status = OpenCameraLocked();
    if (status != BsvError::kOk) {
        state_ = BsvState::kClosed;
        return status;
    }
    status = CreateCaptureSessionLocked();
    if (status != BsvError::kOk) {
        CloseSessionLocked();
        CloseCameraLocked();
        state_ = BsvState::kClosed;
        return status;
    }
    state_ = BsvState::kRunning;
    return BsvError::kOk;
}

BsvError AndroidCamera2Provider::OpenCameraLocked() {
    if (camera_manager_ == nullptr) {
        return BsvError::kNotInitialized;
    }
    if (active_camera_id_.empty()) {
        ACameraIdList* camera_list = nullptr;
        camera_status_t status = ACameraManager_getCameraIdList(
            reinterpret_cast<ACameraManager*>(camera_manager_), &camera_list);
        if (status != ACAMERA_OK || camera_list == nullptr || camera_list->numCameras < 1) {
            if (camera_list != nullptr) {
                ACameraManager_deleteCameraIdList(camera_list);
            }
            return BsvError::kInternal;
        }
        active_camera_id_ = camera_list->cameraIds[0];
        ACameraManager_deleteCameraIdList(camera_list);
    }
    ACameraDevice_StateCallbacks callbacks{};
    callbacks.context = this;
    callbacks.onDisconnected = OnCameraDisconnected;
    callbacks.onError = OnCameraError;
    camera_status_t status = ACameraManager_openCamera(
        reinterpret_cast<ACameraManager*>(camera_manager_),
        active_camera_id_.c_str(),
        &callbacks,
        reinterpret_cast<ACameraDevice**>(&camera_device_));
    return MapCameraStatus(status);
}

BsvError AndroidCamera2Provider::CreateImageReaderLocked() {
    if (image_reader_ != nullptr) {
        return BsvError::kOk;
    }
    const int format = ResolveImageFormat(config_.output_format);
    const uint64_t usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    AImageReader* reader = nullptr;
    media_status_t status = AImageReader_newWithUsage(
        static_cast<int32_t>(config_.width),
        static_cast<int32_t>(config_.height),
        format,
        usage,
        4,
        &reader);
    if (status != AMEDIA_OK || reader == nullptr) {
        return BsvError::kInternal;
    }
    AImageReader_ImageListener listener{};
    listener.context = this;
    listener.onImageAvailable = OnImageAvailable;
    AImageReader_setImageListener(reader, &listener);
    image_reader_ = reader;
    return BsvError::kOk;
}

BsvError AndroidCamera2Provider::CreateCaptureSessionLocked() {
    if (camera_device_ == nullptr || image_reader_ == nullptr) {
        return BsvError::kInvalidState;
    }
    ANativeWindow* window = nullptr;
    AImageReader_getWindow(reinterpret_cast<AImageReader*>(image_reader_), &window);
    if (window == nullptr) {
        return BsvError::kInternal;
    }
    ACaptureSessionOutputContainer_create(reinterpret_cast<ACaptureSessionOutputContainer**>(&output_container_));
    ACaptureSessionOutput_create(window, reinterpret_cast<ACaptureSessionOutput**>(&session_output_));
    ACaptureSessionOutputContainer_add(reinterpret_cast<ACaptureSessionOutputContainer*>(output_container_),
                                       reinterpret_cast<ACaptureSessionOutput*>(session_output_));
    ACameraOutputTarget_create(window, reinterpret_cast<ACameraOutputTarget**>(&output_target_));
    camera_status_t status = ACameraDevice_createCaptureRequest(
        reinterpret_cast<ACameraDevice*>(camera_device_),
        TEMPLATE_PREVIEW,
        reinterpret_cast<ACaptureRequest**>(&capture_request_));
    if (status != ACAMERA_OK) {
        return MapCameraStatus(status);
    }
    ACaptureRequest_addTarget(reinterpret_cast<ACaptureRequest*>(capture_request_),
                              reinterpret_cast<ACameraOutputTarget*>(output_target_));
    ACameraCaptureSession_stateCallbacks callbacks{};
    callbacks.context = this;
    status = ACameraDevice_createCaptureSession(
        reinterpret_cast<ACameraDevice*>(camera_device_),
        reinterpret_cast<ACaptureSessionOutputContainer*>(output_container_),
        &callbacks,
        reinterpret_cast<ACameraCaptureSession**>(&capture_session_));
    if (status != ACAMERA_OK) {
        return MapCameraStatus(status);
    }
    status = ACameraCaptureSession_setRepeatingRequest(
        reinterpret_cast<ACameraCaptureSession*>(capture_session_),
        nullptr,
        1,
        reinterpret_cast<ACaptureRequest**>(&capture_request_),
        nullptr);
    return MapCameraStatus(status);
}

void AndroidCamera2Provider::CloseSessionLocked() {
    if (capture_session_ != nullptr) {
        ACameraCaptureSession_stopRepeating(reinterpret_cast<ACameraCaptureSession*>(capture_session_));
        ACameraCaptureSession_close(reinterpret_cast<ACameraCaptureSession*>(capture_session_));
        capture_session_ = nullptr;
    }
    if (capture_request_ != nullptr) {
        ACaptureRequest_free(reinterpret_cast<ACaptureRequest*>(capture_request_));
        capture_request_ = nullptr;
    }
    if (output_target_ != nullptr) {
        ACameraOutputTarget_free(reinterpret_cast<ACameraOutputTarget*>(output_target_));
        output_target_ = nullptr;
    }
    if (session_output_ != nullptr) {
        ACaptureSessionOutput_free(reinterpret_cast<ACaptureSessionOutput*>(session_output_));
        session_output_ = nullptr;
    }
    if (output_container_ != nullptr) {
        ACaptureSessionOutputContainer_free(reinterpret_cast<ACaptureSessionOutputContainer*>(output_container_));
        output_container_ = nullptr;
    }
}

void AndroidCamera2Provider::ReleaseImageReaderLocked() {
    if (image_reader_ != nullptr) {
        AImageReader_delete(reinterpret_cast<AImageReader*>(image_reader_));
        image_reader_ = nullptr;
    }
}

void AndroidCamera2Provider::CloseCameraLocked() {
    if (camera_device_ != nullptr) {
        ACameraDevice_close(reinterpret_cast<ACameraDevice*>(camera_device_));
        camera_device_ = nullptr;
    }
}

void AndroidCamera2Provider::OnImageAvailable(void* reader) {
    FrameCallback callback = nullptr;
    void* user_data = nullptr;
    PixelFormat format = config_.output_format;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != BsvState::kRunning || callback_ == nullptr) {
            AImage* image = nullptr;
            AImageReader_acquireLatestImage(static_cast<AImageReader*>(reader), &image);
            if (image != nullptr) {
                AImage_delete(image);
            }
            return;
        }
        callback = callback_;
        user_data = callback_user_data_;
    }
    AImage* image = nullptr;
    if (AImageReader_acquireLatestImage(static_cast<AImageReader*>(reader), &image) != AMEDIA_OK || image == nullptr) {
        return;
    }
    AndroidCamera2Buffer buffer(image, format);
    callback(buffer, user_data);
}

void AndroidCamera2Provider::OnCameraDisconnected() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = BsvState::kClosed;
}

void AndroidCamera2Provider::OnCameraError(int error) {
    (void)error;
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = BsvState::kClosed;
}

}  // namespace bsv

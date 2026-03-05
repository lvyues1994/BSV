#ifndef BSV_CAMERA2_PROVIDER_H
#define BSV_CAMERA2_PROVIDER_H

#include <mutex>
#include <string>

#include "../../../controller/include/bsv/core_types.h"

namespace bsv {

// Error handling rules:
// - Android NDK camera status codes are mapped to BsvError via MapCameraStatus.
// - Initialization errors return kInvalidArgument or kNotSupported.
// - Runtime errors return kInternal and transition state to kClosed.

class AndroidCamera2Provider final : public ICameraProvider {
public:
    AndroidCamera2Provider();
    ~AndroidCamera2Provider() override;

    BsvError Initialize(const CameraConfig& config) override;
    BsvError Start() override;
    BsvError Stop() override;
    void Shutdown() override;
    BsvError SetFrameCallback(FrameCallback callback, void* user_data) override;

    // Request a camera switch while running.
    // Returns kInvalidState when not in kRunning.
    BsvError RequestCameraSwitch(const char* camera_id) override;

private:
    BsvError OpenCameraLocked();
    BsvError CreateImageReaderLocked();
    BsvError CreateCaptureSessionLocked();
    void CloseSessionLocked();
    void ReleaseImageReaderLocked();
    void CloseCameraLocked();

    BsvError ValidateConfig(const CameraConfig& config);

    void OnImageAvailable(void* reader);
    void OnCameraDisconnected();
    void OnCameraError(int error);

    std::mutex mutex_{};
    CameraConfig config_{};
    std::string active_camera_id_{};
    FrameCallback callback_ = nullptr;
    void* callback_user_data_ = nullptr;
    BsvState state_ = BsvState::kClosed;
    bool initialized_ = false;

    void* camera_manager_ = nullptr;
    void* camera_device_ = nullptr;
    void* capture_session_ = nullptr;
    void* image_reader_ = nullptr;
    void* capture_request_ = nullptr;
    void* output_container_ = nullptr;
    void* session_output_ = nullptr;
    void* output_target_ = nullptr;
};

}  // namespace bsv

#endif  // BSV_CAMERA2_PROVIDER_H

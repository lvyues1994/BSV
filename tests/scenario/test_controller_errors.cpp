#include "bsv/controller.h"

#include "bsv/buffer.h"

#include <cassert>
#include <cstring>
#include <thread>
#include <vector>

namespace {

class FakeCameraProvider final : public bsv::ICameraProvider {
public:
    bsv::BsvError Initialize(const bsv::CameraConfig& config) override {
        config_ = config;
        initialized_ = true;
        return bsv::BsvError::kOk;
    }

    bsv::BsvError Start() override {
        started_ = true;
        return bsv::BsvError::kOk;
    }

    bsv::BsvError Stop() override {
        started_ = false;
        return bsv::BsvError::kOk;
    }

    void Shutdown() override {
        initialized_ = false;
    }

    bsv::BsvError SetFrameCallback(FrameCallback callback, void* user_data) override {
        callback_ = callback;
        user_data_ = user_data;
        return bsv::BsvError::kOk;
    }

    bsv::BsvError RequestCameraSwitch(const char* camera_id) override {
        (void)camera_id;
        return bsv::BsvError::kOk;
    }

    void TriggerFrame(const bsv::IBuffer& buffer) {
        if (started_ && callback_ != nullptr) {
            callback_(buffer, user_data_);
        }
    }

private:
    bsv::CameraConfig config_{};
    FrameCallback callback_ = nullptr;
    void* user_data_ = nullptr;
    bool initialized_ = false;
    bool started_ = false;
};

class FailingCscConverter final : public bsv::ICscConverter {
public:
    bsv::BsvError Initialize(const bsv::CscConfig& config) override {
        config_ = config;
        return bsv::BsvError::kOk;
    }

    bsv::BsvError Start() override {
        return bsv::BsvError::kOk;
    }

    bsv::BsvError Stop() override {
        return bsv::BsvError::kOk;
    }

    void Shutdown() override {}

    bsv::BsvError ConvertFrame(const bsv::IBuffer& src, bsv::IBuffer& dst) override {
        (void)src;
        (void)dst;
        ++convert_attempts_;
        return bsv::BsvError::kInternal;
    }

    int convert_attempts() const {
        return convert_attempts_;
    }

private:
    bsv::CscConfig config_{};
    int convert_attempts_ = 0;
};

class FailingCameraProvider final : public bsv::ICameraProvider {
public:
    bsv::BsvError Initialize(const bsv::CameraConfig& config) override {
        config_ = config;
        return bsv::BsvError::kInternal;
    }

    bsv::BsvError Start() override {
        return bsv::BsvError::kInternal;
    }

    bsv::BsvError Stop() override {
        return bsv::BsvError::kInternal;
    }

    void Shutdown() override {}

    bsv::BsvError SetFrameCallback(FrameCallback callback, void* user_data) override {
        (void)callback;
        (void)user_data;
        return bsv::BsvError::kOk;
    }

private:
    bsv::CameraConfig config_{};
};

}  // namespace

int main() {
    bsv::LinuxBufferAllocator allocator;
    assert(allocator.Initialize() == bsv::BsvError::kOk);

    bsv::BufferDesc input_desc{4, 4, 4, bsv::PixelFormat::kNV12, 0};
    bsv::IBuffer* input = nullptr;
    assert(allocator.Allocate(input_desc, &input) == bsv::BsvError::kOk);

    bsv::BufferDesc output_desc{4, 4, 4, bsv::PixelFormat::kRGBA8888, 0};
    std::vector<uint8_t> output_data(output_desc.width * output_desc.height * 4, 0);
    bsv::PlatformHandle output_handle{};
    output_handle.type = bsv::PlatformHandleType::kDmaBuf;
    output_handle.handle = output_data.data();
    output_handle.size = output_data.size();
    output_handle.desc = output_desc;

    FailingCameraProvider failing_camera;
    FailingCscConverter failing_csc;

    bsv::ControllerConfig config{};
    config.camera = &failing_camera;
    config.csc = &failing_csc;
    config.allocator = &allocator;
    config.camera_config = {4, 4, 30, bsv::PixelFormat::kNV12, "0"};
    config.csc_config = {bsv::PixelFormat::kNV12, bsv::PixelFormat::kRGBA8888};
    config.output_handle = output_handle;

    bsv::BsvController controller;
    assert(controller.Open(config) == bsv::BsvError::kInternal);
    assert(controller.Close() == bsv::BsvError::kOk);

    FakeCameraProvider camera;
    config.camera = &camera;
    config.csc = &failing_csc;
    assert(controller.Open(config) == bsv::BsvError::kOk);
    camera.TriggerFrame(*input);
    for (int i = 0; i < 100 && failing_csc.convert_attempts() < 1; ++i) {
        std::this_thread::yield();
    }
    assert(failing_csc.convert_attempts() >= 1);
    assert(controller.Close() == bsv::BsvError::kOk);

    allocator.Release(input);
    allocator.Shutdown();
    return 0;
}

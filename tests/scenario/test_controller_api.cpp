#include "bsv/controller.h"

#include "bsv/buffer.h"

#include <cassert>
#include <cstring>
#include <string>
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
        if (!started_) {
            return bsv::BsvError::kInvalidState;
        }
        last_camera_id_ = camera_id != nullptr ? camera_id : "";
        return bsv::BsvError::kOk;
    }

private:
    bsv::CameraConfig config_{};
    FrameCallback callback_ = nullptr;
    void* user_data_ = nullptr;
    bool initialized_ = false;
    bool started_ = false;
    std::string last_camera_id_{};
};

class FakeCscConverter final : public bsv::ICscConverter {
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
        bsv::BufferMapping mapping{};
        if (dst.Map(bsv::BufferAccessMode::kWrite, &mapping) != bsv::BsvError::kOk) {
            return bsv::BsvError::kInternal;
        }
        std::memset(mapping.data, 0xCD, mapping.size);
        dst.Unmap(&mapping);
        return bsv::BsvError::kOk;
    }

private:
    bsv::CscConfig config_{};
};

}  // namespace

int main() {
    FakeCameraProvider camera;
    FakeCscConverter csc;
    bsv::LinuxBufferAllocator allocator;
    assert(allocator.Initialize() == bsv::BsvError::kOk);

    bsv::BufferDesc output_desc{16, 16, 16, bsv::PixelFormat::kRGBA8888, 0};
    std::vector<uint8_t> output_data(output_desc.width * output_desc.height * 4);
    bsv::PlatformHandle output_handle{};
    output_handle.type = bsv::PlatformHandleType::kDmaBuf;
    output_handle.handle = output_data.data();
    output_handle.size = output_data.size();
    output_handle.desc = output_desc;

    bsv::ControllerConfig config{};
    config.camera = &camera;
    config.csc = &csc;
    config.allocator = &allocator;
    config.camera_config = {16, 16, 30, bsv::PixelFormat::kNV12, "0"};
    config.csc_config = {bsv::PixelFormat::kNV12, bsv::PixelFormat::kRGBA8888};
    config.output_handle = output_handle;

    bsv::BsvController controller;
    assert(controller.Open(config) == bsv::BsvError::kOk);
    assert(controller.Open(config) == bsv::BsvError::kInvalidState);
    assert(controller.SelectCamera("1") == bsv::BsvError::kOk);
    assert(controller.Close() == bsv::BsvError::kOk);
    assert(controller.SelectCamera("1") == bsv::BsvError::kInvalidState);

    allocator.Shutdown();
    return 0;
}

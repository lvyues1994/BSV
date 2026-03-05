#include "bsv/controller.h"

#include "bsv/buffer.h"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
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
        std::memset(mapping.data, 0xAB, mapping.size);
        dst.Unmap(&mapping);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++convert_count_;
        }
        cv_.notify_all();
        return bsv::BsvError::kOk;
    }

    bool WaitForConvert(int expected, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return convert_count_ >= expected; });
    }

private:
    bsv::CscConfig config_{};
    std::mutex mutex_{};
    std::condition_variable cv_{};
    int convert_count_ = 0;
};

void FillNv12Pattern(bsv::IBuffer& buffer) {
    bsv::BufferMapping mapping{};
    assert(buffer.Map(bsv::BufferAccessMode::kWrite, &mapping) == bsv::BsvError::kOk);
    auto* data = static_cast<uint8_t*>(mapping.data);
    std::memset(data, 0x10, mapping.size);
    buffer.Unmap(&mapping);
}

}  // namespace

int main() {
    FakeCameraProvider camera;
    FakeCscConverter csc;
    bsv::LinuxBufferAllocator allocator;
    assert(allocator.Initialize() == bsv::BsvError::kOk);

    bsv::BufferDesc input_desc{8, 8, 8, bsv::PixelFormat::kNV12, 0};
    bsv::IBuffer* input = nullptr;
    assert(allocator.Allocate(input_desc, &input) == bsv::BsvError::kOk);
    FillNv12Pattern(*input);

    bsv::BufferDesc output_desc{8, 8, 8, bsv::PixelFormat::kRGBA8888, 0};
    std::vector<uint8_t> output_data(output_desc.width * output_desc.height * 4, 0);
    bsv::PlatformHandle output_handle{};
    output_handle.type = bsv::PlatformHandleType::kDmaBuf;
    output_handle.handle = output_data.data();
    output_handle.size = output_data.size();
    output_handle.desc = output_desc;

    bsv::ControllerConfig config{};
    config.camera = &camera;
    config.csc = &csc;
    config.allocator = &allocator;
    config.camera_config = {8, 8, 30, bsv::PixelFormat::kNV12, "0"};
    config.csc_config = {bsv::PixelFormat::kNV12, bsv::PixelFormat::kRGBA8888};
    config.output_handle = output_handle;

    bsv::BsvController controller;
    assert(controller.Open(config) == bsv::BsvError::kOk);
    camera.TriggerFrame(*input);
    assert(csc.WaitForConvert(1, std::chrono::milliseconds(500)));
    assert(output_data[0] == 0xAB);
    assert(controller.Close() == bsv::BsvError::kOk);

    allocator.Release(input);
    allocator.Shutdown();

    bsv::BufferDesc bad_output_desc{8, 8, 8, bsv::PixelFormat::kNV12, 0};
    std::vector<uint8_t> bad_output_data(bad_output_desc.width * bad_output_desc.height * 2, 0);
    bsv::PlatformHandle bad_handle{};
    bad_handle.type = bsv::PlatformHandleType::kDmaBuf;
    bad_handle.handle = bad_output_data.data();
    bad_handle.size = bad_output_data.size();
    bad_handle.desc = bad_output_desc;

    config.output_handle = bad_handle;
    assert(controller.Open(config) == bsv::BsvError::kInvalidArgument);
    return 0;
}

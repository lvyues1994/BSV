#include "bsv/controller.h"

#include <thread>

namespace bsv {

BsvController::BsvController() = default;

BsvController::~BsvController() {
    Close();
}

BsvError BsvController::Open(const ControllerConfig& config) {
    if (config.camera == nullptr || config.csc == nullptr || config.allocator == nullptr) {
        return BsvError::kInvalidArgument;
    }
    if (config.output_handle.handle == nullptr ||
        config.output_handle.desc.format != PixelFormat::kRGBA8888) {
        return BsvError::kInvalidArgument;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != BsvState::kClosed) {
            return BsvError::kInvalidState;
        }
        state_ = BsvState::kOpening;
        config_ = config;
    }

    BsvError status = PrepareOutputBufferLocked(config.output_handle);
    if (status != BsvError::kOk) {
        ShutdownModulesLocked();
        ReleaseOutputBufferLocked();
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = BsvState::kClosed;
        return status;
    }

    status = InitializeModulesLocked(config);
    if (status != BsvError::kOk) {
        ShutdownModulesLocked();
        ReleaseOutputBufferLocked();
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = BsvState::kClosed;
        return status;
    }

    worker_exit_ = false;
    worker_thread_ = new std::thread([this] { WorkerLoop(); });
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = BsvState::kRunning;
    }
    return BsvError::kOk;
}

BsvError BsvController::Close() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (state_ == BsvState::kClosed) {
        return BsvError::kOk;
    }
    state_ = BsvState::kClosing;
    worker_exit_ = true;
    cv_.notify_all();
    lock.unlock();

    if (worker_thread_ != nullptr) {
        auto* thread = static_cast<std::thread*>(worker_thread_);
        if (thread->joinable()) {
            thread->join();
        }
        delete thread;
        worker_thread_ = nullptr;
    }

    ShutdownModulesLocked();

    lock.lock();
    DrainQueueLocked();
    ReleaseOutputBufferLocked();
    state_ = BsvState::kClosed;
    return BsvError::kOk;
}

BsvError BsvController::SelectCamera(const char* camera_id) {
    if (camera_id == nullptr) {
        return BsvError::kInvalidArgument;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != BsvState::kRunning) {
        return BsvError::kInvalidState;
    }
    return config_.camera->RequestCameraSwitch(camera_id);
}

void BsvController::OnFrame(const IBuffer& buffer, void* user_data) {
    auto* controller = static_cast<BsvController*>(user_data);
    if (controller != nullptr) {
        controller->HandleFrame(buffer);
    }
}

void BsvController::HandleFrame(const IBuffer& buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != BsvState::kRunning) {
        return;
    }
    frame_queue_.push_back(FrameTask{const_cast<IBuffer*>(&buffer)});
    cv_.notify_one();
}

void BsvController::WorkerLoop() {
    while (true) {
        FrameTask task{};
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return worker_exit_ || !frame_queue_.empty(); });
            if (worker_exit_) {
                break;
            }
            if (frame_queue_.empty()) {
                continue;
            }
            task = frame_queue_.front();
            frame_queue_.pop_front();
        }
        if (task.buffer != nullptr && output_buffer_ != nullptr && config_.csc != nullptr) {
            config_.csc->ConvertFrame(*task.buffer, *output_buffer_);
        }
    }
}

void BsvController::DrainQueueLocked() {
    frame_queue_.clear();
}

void BsvController::ReleaseOutputBufferLocked() {
    if (output_buffer_ != nullptr && config_.allocator != nullptr) {
        config_.allocator->Release(output_buffer_);
    }
    output_buffer_ = nullptr;
}

BsvError BsvController::PrepareOutputBufferLocked(const PlatformHandle& handle) {
    if (handle.handle == nullptr) {
        return BsvError::kInvalidArgument;
    }
    ReleaseOutputBufferLocked();
    return config_.allocator->ImportFromHandle(handle, &output_buffer_);
}

BsvError BsvController::InitializeModulesLocked(const ControllerConfig& config) {
    BsvError status = config.camera->Initialize(config.camera_config);
    if (status != BsvError::kOk) {
        return status;
    }
    status = config.csc->Initialize(config.csc_config);
    if (status != BsvError::kOk) {
        config.camera->Shutdown();
        return status;
    }
    status = config.camera->SetFrameCallback(OnFrame, this);
    if (status != BsvError::kOk) {
        config.csc->Shutdown();
        config.camera->Shutdown();
        return status;
    }
    status = config.camera->Start();
    if (status != BsvError::kOk) {
        config.csc->Shutdown();
        config.camera->Shutdown();
        return status;
    }
    status = config.csc->Start();
    if (status != BsvError::kOk) {
        config.camera->Stop();
        config.csc->Shutdown();
        config.camera->Shutdown();
        return status;
    }
    return BsvError::kOk;
}

void BsvController::ShutdownModulesLocked() {
    if (config_.camera != nullptr) {
        config_.camera->Stop();
        config_.camera->Shutdown();
    }
    if (config_.csc != nullptr) {
        config_.csc->Stop();
        config_.csc->Shutdown();
    }
}

}  // namespace bsv

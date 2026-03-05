#ifndef BSV_CONTROLLER_H
#define BSV_CONTROLLER_H

#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <mutex>

#include "buffer.h"
#include "core_types.h"

namespace bsv {

struct ControllerConfig {
    ICameraProvider* camera = nullptr;
    ICscConverter* csc = nullptr;
    IBufferAllocator* allocator = nullptr;
    CameraConfig camera_config{};
    CscConfig csc_config{};
    PlatformHandle output_handle{};
};

class BsvController final {
public:
    BsvController();
    ~BsvController();

    BsvError Open(const ControllerConfig& config);
    BsvError Close();
    BsvError SelectCamera(const char* camera_id);

private:
    struct FrameTask {
        IBuffer* buffer = nullptr;
    };

    static void OnFrame(const IBuffer& buffer, void* user_data);
    void HandleFrame(const IBuffer& buffer);
    void WorkerLoop();
    void DrainQueueLocked();
    void ReleaseOutputBufferLocked();

    BsvError PrepareOutputBufferLocked(const PlatformHandle& handle);
    BsvError InitializeModulesLocked(const ControllerConfig& config);
    void ShutdownModulesLocked();

    void* worker_thread_ = nullptr;
    bool worker_exit_ = false;

    std::mutex mutex_{};
    std::condition_variable cv_{};
    std::deque<FrameTask> frame_queue_{};

    BsvState state_ = BsvState::kClosed;
    ControllerConfig config_{};
    IBuffer* output_buffer_ = nullptr;
};

}  // namespace bsv

#endif  // BSV_CONTROLLER_H

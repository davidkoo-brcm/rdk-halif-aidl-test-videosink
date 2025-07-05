#ifndef VTS_L3_COMMON_H
#define VTS_L3_COMMON_H
#include <ut.h>
#include <ut_kvp_profile.h>
#include <ut_log.h>
#include <ut_control_plane.h>
#include <deque>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <binder/Status.h>
#include <utils/String16.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <optional>
#include <algorithm>
#include <unordered_set>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <openssl/sha.h>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "com/rdk/hal/videodecoder/FrameMetadata.h"

using namespace com::rdk::hal::videodecoder;

#define UT_LOG_DEBUG UT_LOG_INFO

struct FrameData {
    int64_t pts;
    int64_t bufferHandle;
    FrameMetadata metadata;

    // Constructor for push
    FrameData(int64_t pts_, int64_t handle_, FrameMetadata&& meta)
        : pts(pts_), bufferHandle(handle_), metadata(std::move(meta)) {}

    // Delete copy constructor and copy assignment
    FrameData(const FrameData&) = delete;
    FrameData& operator=(const FrameData&) = delete;

    // Delete move assignment explicitly (required due to FrameMetadata)
    FrameData(FrameData&&) = default;
    FrameData& operator=(FrameData&&) = delete;
};


class FrameQueue {
public:
    void push(FrameData&& frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(frame));
        condVar_.notify_one();
    }

    std::optional<FrameData> waitAndPop(int timeoutMs = 1000) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!condVar_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]{ return !queue_.empty(); })) {
            return std::nullopt;
        }

        FrameData frame(
            queue_.front().pts,
            queue_.front().bufferHandle,
            std::move(queue_.front().metadata)
        );
        queue_.pop();
        return frame;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
    }

private:
    std::queue<FrameData> queue_;
    std::mutex mutex_;
    std::condition_variable condVar_;
};

// Declare this in a shared source file (e.g., FrameQueue.cpp)
extern FrameQueue gFrameQueue;

#endif // VTS_L3_COMMON
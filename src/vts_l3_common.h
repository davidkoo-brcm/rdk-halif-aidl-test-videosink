/**
 *  If not stated otherwise in this file or this component's LICENSE
 *  file the following copyright and licenses apply:
 *
 *  Copyright 2025 RDK Management
 *
 *  Licensed under the Apache License, Version 2.0 (the License);
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an AS IS BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * @addtogroup HPK Hardware Porting Kit
 * @{
 * @par The Hardware Porting Kit
 * HPK is the next evolution of the well-defined Hardware Abstraction Layer
 * (HAL), but augmented with more comprehensive documentation and test suites
 * that OEM or SOC vendors can use to self-certify their ports before taking
 * them to RDKM for validation or to an operator for final integration and
 * deployment. The Hardware Porting Kit effectively enables an OEM and/or SOC
 * vendor to self-certify their own Video Accelerator devices, with minimal RDKM
 * assistance.
 *
 */

/**
 * @addtogroup VIDEO_SINK Video sink Module
 * @{
 */

/**
 * @addtogroup VIDEO_SINK_HALTEST Video sink HAL Tests
 * @{
 */

/**
 * @defgroup VTS_L3_COMMON Video sink HAL Tests parse Hardware feature profile Header File
 * @{
 * @parblock
 *
 * ### Provides common calls for the Videosink :
 *
 * Provide common function calls to call from the videosink.
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_l3_common.h
 *
 */
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

// Undefine UT_LOG_INFO from ut_log.h to avoid redefinition conflicts
#undef UT_LOG_INFO

#ifdef ENABLE_UT_INFO_LOGGING
// Enable INFO logging using ut_log.h's definition
#define UT_LOG_INFO(format, ...) UT_logPrefix(__FILE__, __LINE__, UT_LOG_ASCII_CYAN "INFO  " UT_LOG_ASCII_NC, format, ## __VA_ARGS__)
#else
#define UT_LOG_INFO(format, ...) // Disable INFO logging by default (no-op)
#endif

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

/** @} */ // End of VTS_L3_COMMON_HEADER
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK
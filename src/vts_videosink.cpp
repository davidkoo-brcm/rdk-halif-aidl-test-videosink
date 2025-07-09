/**
 *  If not stated otherwise in this file or this component's LICENSE
 *  file the following copyright and licenses apply:
 *
 *  Copyright 2024 RDK Management
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
 * @addtogroup VIDEO_SINK Video Sink Module
 * @{
 */

/**
 * @addtogroup VIDEO_SINK_HALTEST Video Sink HAL Tests
 * @{
 */

/**
 * @defgroup VIDEO_SINK_HALTEST_CLASS Video Sink HAL Tests helper functions Header File
 * @{
 * @parblock
 *
 * ### Common functions/macros required for the video sink module :
 *
 * Common functions/macros required for the video sink module .
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_videosink.cpp
 *
 */

#include <fstream>
#include <ut.h>
#include <ut_log.h>
#include <ut_kvp_profile.h>
#include <ut_control_plane.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <binder/Status.h>
#include <sys/syscall.h>
#include <utils/String16.h>
#include <stdio.h>
#include <vector>
#include <optional>
#include <algorithm>
#include <unordered_set>
#include <tuple>
#include <map>
#include <chrono>


#include "com/rdk/hal/avbuffer/IAVBuffer.h"
#include "com/rdk/hal/avbuffer/BnAVBufferSpaceListener.h"

#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"
#include "vts_videosink_parse_testconfig.h"
#include "vts_videosink.h"


#include "IAVBufferHelper.h"

using namespace std::chrono;
using namespace std;

VideoSinkControllerListener::VideoSinkControllerListener()
    : mFirstFrameRendered(false),
      mFirstPTS(-1),
      mFlushComplete(false),
      mEosReceived(false),
      mEosPTS(-1),
      mUnderflowCount(0),
      mResumeCount(0)
{
}

VideoSinkControllerListener::~VideoSinkControllerListener() = default;

/**
 * @brief Callback invoked when the first video frame has been rendered.
 *
 * This function is triggered once the video sink renders its first frame. It records
 * the presentation timestamp (PTS) and signals any waiting test logic.
 *
 * @param nsPresentationTime Presentation timestamp (in nanoseconds) of the first rendered frame.
 * @return android::binder::Status Always returns Status::ok().
 */
android::binder::Status VideoSinkControllerListener::onFirstFrameRendered(int64_t nsPresentationTime)
{
    UT_LOG_INFO("[SinkID %d] [PTS %lld]  VideoSinkControllerListener : onFirstFrameRendered Callback Received", mSinkID, nsPresentationTime);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mFirstFrameRendered)
        {
            mFirstPTS = nsPresentationTime;
            mFirstFrameRendered = true;
        }
    }
    mCV.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Resets the first frame rendered tracking state.
 *
 * Clears the internal flag and timestamp used to track whether the first
 * frame has been rendered. Typically used before starting a new test case
 * or playback session.
 */
void VideoSinkControllerListener::resetFirstFrameRendered()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mFirstFrameRendered = false;
    mFirstPTS = -1;
}

/**
 * @brief Checks if the first frame has been rendered.
 *
 * @return true if the first frame has been rendered, false otherwise.
 */
bool VideoSinkControllerListener::hasFirstFrameRendered()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mFirstFrameRendered;
}

/**
 * @brief Retrieves the presentation timestamp (PTS) of the first rendered frame.
 *
 * @return The PTS of the first frame rendered, or -1 if no frame has been rendered yet.
 */
int64_t VideoSinkControllerListener::getFirstFrameRenderedPTS()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mFirstPTS;
}

/**
 * @brief Resets the flush complete flag to false.
 */
void VideoSinkControllerListener::resetFlushComplete()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mFlushComplete = false;
}

/**
 * @brief Checks whether the flush complete event has occurred.
 *
 * @return true if flush is complete, false otherwise.
 */
bool VideoSinkControllerListener::hasFlushComplete()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mFlushComplete;
}

/**
 * @brief Callback invoked when a flush operation is complete.
 *
 * Sets the flush complete flag and notifies waiting threads.
 *
 * @return android::binder::Status::ok() on success.
 */
android::binder::Status VideoSinkControllerListener::onFlushComplete()
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkControllerListener: onFlushComplete callback received ",mSinkID);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mFlushComplete = true;
    }
    mCV.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Callback invoked when End of Stream (EOS) is received.
 *
 * Records the EOS flag and presentation timestamp (PTS), then notifies waiting threads.
 *
 * @param nsPresentationTime Presentation timestamp of the EOS event in nanoseconds.
 * @return android::binder::Status::ok() on success.
 */
android::binder::Status VideoSinkControllerListener::onEndOfStream(int64_t nsPresentationTime)
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkControllerListener: onEndOfStream Received PTS[%ld]",mSinkID,nsPresentationTime);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mEosReceived = true;
        mEosPTS = nsPresentationTime;
    }
    mCV.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Checks if End of Stream (EOS) has been received.
 *
 * @return true if EOS received, false otherwise.
 */
bool VideoSinkControllerListener::hasEosReceived()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mEosReceived;
}

/**
 * @brief Retrieves the presentation timestamp (PTS) associated with the received End of Stream (EOS).
 *
 * @return The EOS PTS value.
 */
int64_t VideoSinkControllerListener::getEosPTS()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mEosPTS;
}

/**
 * @brief Resets the End of Stream (EOS) received status and associated PTS.
 *
 * After calling this, EOS is considered not received and PTS is reset.
 */
void VideoSinkControllerListener::resetEos()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mEosReceived = false;
    mEosPTS = -1;
}

/**
 * @brief Callback invoked when a video underflow event occurs.
 *
 * Increments the underflow count and notifies any waiting threads.
 *
 * @return android::binder::Status indicating success.
 */
android::binder::Status VideoSinkControllerListener::onVideoUnderflow()
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkControllerListener: onVideoUnderflow Callback Received",mSinkID);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mUnderflowCount++;
    }
    mCV.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Resets the video underflow count to zero.
 *
 * This method clears the recorded underflow events.
 */
void VideoSinkControllerListener::resetUnderflow()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mUnderflowCount = 0;
}

/**
 * @brief Retrieves the current count of video underflow events.
 *
 * @return int The number of underflow events recorded.
 */
int VideoSinkControllerListener::getUnderflowCount()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mUnderflowCount;
}

/**
 * @brief Callback invoked when video resumes after underflow.
 *
 * Increments the resume event count and notifies waiting threads.
 *
 * @return android::binder::Status Status indicating success.
 */
android::binder::Status VideoSinkControllerListener::onVideoResumed()
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkControllerListener: onVideoResumed Callback Received",mSinkID);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mResumeCount++;
    }
    mCV.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Resets the count of video resumed events to zero.
 *
 * Thread-safe reset of the internal resume event counter.
 */
void VideoSinkControllerListener::resetResumed()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mResumeCount = 0;
}

/**
 * @brief Returns the count of video resumed events received.
 *
 * Thread-safe retrieval of the resume event counter.
 *
 * @return Number of video resumed events received.
 */
int VideoSinkControllerListener::getResumedCount()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mResumeCount;
}

/**
 * @brief Callback for state changes in the video sink controller.
 *
 * This implementation currently does nothing and always returns success.
 *
 * @param oldState Previous state (unused)
 * @param newState New state (unused)
 * @return android::binder::Status Always returns Status::ok()
 */
android::binder::Status VideoSinkControllerListener::onStateChanged(State oldState, State newState)
{
    UT_LOG_INFO("[SinkId %d] VideoSinkControllerListener: State Change received: %d, %d ", mSinkID, oldState, newState);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mStateChanges.emplace_back(oldState, newState);
        mCurrentState = newState;
    }
    mCV.notify_all();
    return ::android::binder::Status::ok();
}

/**
 * @brief Waits for the flush complete event to occur within the given timeout.
 *
 * Blocks the calling thread until the flush complete flag is set or the timeout expires.
 *
 * @param timeout Duration to wait for the flush complete event.
 * @return true if flush complete event occurred within timeout, false otherwise.
 */
bool VideoSinkControllerListener::waitForFlushComplete(std::chrono::seconds timeout)
{
    std::unique_lock<std::mutex> lock(mMutex);
    return mCV.wait_for(lock, timeout, [this] { return mFlushComplete; });
}

/**
 * @brief Waits for the End-Of-Stream (EOS) event to be received within the specified timeout.
 *
 * Blocks the calling thread until the EOS event is received or the timeout expires.
 *
 * @param timeoutSeconds Number of seconds to wait for the EOS event.
 * @return true if EOS event received within timeout, false otherwise.
 */
bool VideoSinkControllerListener::waitForEosEvent(int timeoutSeconds)
{
    std::unique_lock<std::mutex> lock(mMutex);
    return mCV.wait_for(lock, std::chrono::seconds(timeoutSeconds), [this] { return mEosReceived; });
}

/**
 * @brief Waits for a Video Underflow event to be received within the specified timeout.
 *
 * Blocks the calling thread until an underflow event occurs or the timeout expires.
 *
 * @param timeoutSeconds Number of seconds to wait for the underflow event.
 * @return true if underflow event received within timeout, false otherwise.
 */
bool VideoSinkControllerListener::waitForUnderflowEvent(int timeoutSeconds)
{
    std::unique_lock<std::mutex> lock(mMutex);
    mCV.wait_for(lock, std::chrono::seconds(timeoutSeconds), [this] {
        return mUnderflowCount > 0;
    });
    return mUnderflowCount > 0;
}

/**
 * @brief Waits for a Video Resumed event to be received within the specified timeout.
 *
 * Blocks the calling thread until a resumed event occurs or the timeout expires.
 *
 * @param timeoutSeconds Number of seconds to wait for the resumed event.
 * @return true if resumed event received within timeout, false otherwise.
 */
bool VideoSinkControllerListener::waitForResumedEvent(int timeoutSeconds)
{
    std::unique_lock<std::mutex> lock(mMutex);
    mCV.wait_for(lock, std::chrono::seconds(timeoutSeconds), [this] {
        return mResumeCount > 0;
    });
    return mResumeCount > 0;
}

/**
 * @brief Sets the Sink ID for this listener instance.
 *
 * @param id The sink identifier to set.
 */
void VideoSinkControllerListener::setSinkID(int32_t id)
{
    mSinkID = id;
}

VideoSinkEventListener::VideoSinkEventListener()
    : mCurrentState(State::CLOSED),
      mFlushComplete(false)
{
}

/**
 * @brief Callback invoked when the first video frame is rendered.
 *
 * @param nsPresentationTime Presentation timestamp (in nanoseconds) of the first rendered frame.
 * @return android::binder::Status Indicates success of the callback handling.
 */
::android::binder::Status VideoSinkEventListener::onFirstFrameRendered(int64_t nsPresentationTime)
{
    UT_LOG_INFO("[SinkID %d] [PTS %lld] VideoSinkEventListener: onFirstFrameRendered Callback Received", mSinkID, nsPresentationTime);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mFirstFrameRendered)
        {
            mFirstPTS = nsPresentationTime;
            mFirstFrameRendered = true;
        }
    }
    mCV.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Checks if the End-Of-Stream (EOS) event has been received.
 *
 * @return true If EOS event has been received.
 * @return false Otherwise.
 */
bool VideoSinkEventListener::hasEosReceived()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mEosReceived;
}

/**
 * @brief Retrieves the Presentation Timestamp (PTS) of the End-Of-Stream (EOS) event.
 *
 * @return int64_t The EOS PTS value.
 */
int64_t VideoSinkEventListener::getEosPTS()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mEosPTS;
}

/**
 * @brief Resets the End-Of-Stream (EOS) status and PTS.
 */
void VideoSinkEventListener::resetEos()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mEosReceived = false;
    mEosPTS = -1;
}

/**
 * @brief Callback invoked when End-Of-Stream (EOS) is received.
 *
 * @param nsPresentationTime Presentation timestamp (in nanoseconds) of the EOS event.
 * @return android::binder::Status Status of the callback execution.
 */
::android::binder::Status VideoSinkEventListener::onEndOfStream(int64_t nsPresentationTime)
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkEventListener: onEndOfStream Received PTS[%ld]",mSinkID,nsPresentationTime);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mEosReceived = true;
        mEosPTS = nsPresentationTime;
    }
    mCV.notify_all();
    return android::binder::Status::ok();
}

bool VideoSinkEventListener::waitForEosEvent(int timeoutSeconds)
{
    std::unique_lock<std::mutex> lock(mMutex);
    return mCV.wait_for(lock, std::chrono::seconds(timeoutSeconds), [this] { return mEosReceived; });
}

/**
 * @brief Waits for the End-Of-Stream (EOS) event to be received or until timeout.
 *
 * @param timeoutSeconds Timeout duration in seconds to wait for the EOS event.
 * @return bool True if EOS event was received within the timeout, false otherwise.
 */
void VideoSinkEventListener::resetUnderflow()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mUnderflowCount = 0;
}

int VideoSinkEventListener::getUnderflowCount()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mUnderflowCount;
}

/**
 * @brief Retrieves the count of video underflow events received.
 *
 * @return int Number of underflow events.
 */
::android::binder::Status VideoSinkEventListener::onVideoUnderflow()
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkEventListener: onVideoUnderflow Callback Received",mSinkID);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mUnderflowCount++;
    }
    mCV.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Waits for the video resumed event within the specified timeout.
 *
 * @param timeoutSeconds Number of seconds to wait for the resumed event.
 * @return true if the resumed event was received within the timeout, false otherwise.
 */
bool VideoSinkEventListener::waitForResumedEvent(int timeoutSeconds)
{
    std::unique_lock<std::mutex> lock(mMutex);
    mCV.wait_for(lock, std::chrono::seconds(timeoutSeconds), [this] {
        return mResumeCount > 0;
    });
    return mResumeCount > 0;
}

/**
 * @brief Waits for the video underflow event within the specified timeout.
 *
 * @param timeoutSeconds Number of seconds to wait for the underflow event.
 * @return true if the underflow event was received within the timeout, false otherwise.
 */
bool VideoSinkEventListener::waitForUnderflowEvent(int timeoutSeconds)
{
    std::unique_lock<std::mutex> lock(mMutex);
    mCV.wait_for(lock, std::chrono::seconds(timeoutSeconds), [this] {
        return mUnderflowCount > 0;
    });
    return mUnderflowCount > 0;
}

/**
 * @brief Resets the count of video resumed events.
 */
void VideoSinkEventListener::resetResumed()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mResumeCount = 0;
}

/**
 * @brief Retrieves the count of video resumed events received.
 * @return Number of times the video resumed event was received.
 */
int VideoSinkEventListener::getResumedCount()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mResumeCount;
}

/**
 * @brief Callback invoked when video resumes after underflow.
 *        Increments the resume event count and notifies waiting threads.
 *
 * @return android::binder::Status indicating success.
 */
::android::binder::Status VideoSinkEventListener::onVideoResumed()
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkEventListener: onVideoResumed Callback Received",mSinkID);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mResumeCount++;
    }
    mCV.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Callback invoked when a flush operation is complete.
 *        Sets the flush complete flag and notifies waiting threads.
 *
 * @return ::android::binder::Status indicating success.
 */
::android::binder::Status VideoSinkEventListener::onFlushComplete()
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkEventListener: onFlushComplete callback received ",mSinkID);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mFlushComplete = true;
    }
    mCV.notify_all();
    return ::android::binder::Status::ok();
}

/**
 * @brief Callback invoked when the sink state changes.
 *        Logs the state change, records the transition, updates current state,
 *        and notifies any waiting threads.
 *
 * @param oldState Previous state of the sink.
 * @param newState New state of the sink.
 * @return ::android::binder::Status indicating success.
 */
::android::binder::Status VideoSinkEventListener::onStateChanged(State oldState, State newState)
{
    UT_LOG_INFO("[SinkId %d] VideoSinkEventListener: State Change received: %d, %d ", mSinkID, oldState, newState);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mStateChanges.emplace_back(oldState, newState);
        mCurrentState = newState;
    }
    mCV.notify_all();
    return ::android::binder::Status::ok();
}

/**
 * @brief Resets the flush complete status flag to false.
 *        Thread-safe operation with mutex locking.
 */
void VideoSinkEventListener::resetFlushComplete()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mFlushComplete = false;
}

/**
 * @brief Checks whether the flush complete event has been received.
 *        Thread-safe operation with mutex locking.
 *
 * @return true if flush is complete, false otherwise.
 */
bool VideoSinkEventListener::hasFlushComplete()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mFlushComplete;
}

/**
 * @brief Waits for the flush complete event to be signaled within the specified timeout.
 *        Uses condition variable to block until flush completion or timeout.
 *
 * @param timeout Duration to wait for the flush complete event.
 * @return true if flush complete event occurred before timeout, false otherwise.
 */
bool VideoSinkEventListener::waitForFlushComplete(std::chrono::seconds timeout)
{
    std::unique_lock<std::mutex> lock(mMutex);
    return mCV.wait_for(lock, timeout, [this] { return mFlushComplete; });
}

/**
 * @brief Waits for the listener to reach the specified state within the given timeout.
 *        Blocks using condition variable until the desired state is reached or timeout expires.
 *
 * @param desiredState The state to wait for.
 * @param timeout Duration to wait for the desired state.
 * @return true if the desired state was reached before timeout, false otherwise.
 */
bool VideoSinkEventListener::waitForState(State desiredState, std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mMutex);
    return mCV.wait_for(lock, timeout, [this] { return mFlushComplete; });
}

/**
 * @brief Retrieves a copy of all recorded state changes.
 *
 * @return A vector of pairs representing (oldState, newState) transitions.
 */
std::vector<std::pair<State, State>> VideoSinkEventListener::getStateChanges() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mStateChanges;
}

/**
 * @brief Clears all recorded state change entries.
 */
void VideoSinkEventListener::resetStateChanges()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mStateChanges.clear();
}

/**
 * @brief Checks if a specific state transition occurred.
 *
 * @param expOldState Expected old state.
 * @param expNewState Expected new state.
 * @return true if the transition from expOldState to expNewState was recorded, false otherwise.
 */
bool VideoSinkEventListener::checkStateTransistion(State expOldState, State expNewState)
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (const auto& [oldState, newState] : mStateChanges) {
        if (oldState == expOldState && newState == expNewState) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Retrieves the list of recorded error codes.
 *
 * @return A vector containing the error codes.
 */
std::vector<int32_t> VideoSinkEventListener::getErrors() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mErrors;
}

/**
 * @brief Sets the Sink ID.
 *
 * @param id The Sink ID to set.
 */
void VideoSinkEventListener::setSinkID(int32_t id)
{
    mSinkID = id;
}
/** @} */ // End of VIDEO_SINK_HALTEST_HEADER
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK
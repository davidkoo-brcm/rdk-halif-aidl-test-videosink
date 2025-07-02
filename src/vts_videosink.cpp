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
      mUnderflowReceived(false),
      mResumedReceived(false)
{
}

VideoSinkControllerListener::~VideoSinkControllerListener() = default;

android::binder::Status VideoSinkControllerListener::onFirstFrameRendered(int64_t nsPresentationTime)
{
    UT_LOG_INFO("\n=======onFirstFrameRendered=============\n");
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mFirstFrameRendered) {
            mFirstPTS = nsPresentationTime;
            mFirstFrameRendered = true;
        }
    }
    mCV.notify_all();
    return android::binder::Status::ok();
}

void VideoSinkControllerListener::resetFirstFrameRendered()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mFirstFrameRendered = false;
    mFirstPTS = -1;
}

bool VideoSinkControllerListener::hasFirstFrameRendered()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mFirstFrameRendered;
}

int64_t VideoSinkControllerListener::getFirstFrameRenderedPTS()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mFirstPTS;
}

void VideoSinkControllerListener::resetFlushComplete()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mFlushComplete = false;
}

bool VideoSinkControllerListener::hasFlushComplete()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mFlushComplete;
}

android::binder::Status VideoSinkControllerListener::onFlushComplete()
{
    UT_LOG_INFO("\n=======onFlushComplete=============\n");
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mFlushComplete = true;
    }
    mCV.notify_all();
    return android::binder::Status::ok();
}

android::binder::Status VideoSinkControllerListener::onEndOfStream(int64_t nsPresentationTime)
{
    UT_LOG_INFO("\n=======onEndOfStream=============\n");
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mEosReceived = true;
        mEosPTS = nsPresentationTime;
    }
    mCV.notify_all();
    return android::binder::Status::ok();
}

bool VideoSinkControllerListener::hasEosReceived()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mEosReceived;
}

int64_t VideoSinkControllerListener::getEosPTS()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mEosPTS;
}

void VideoSinkControllerListener::resetEos()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mEosReceived = false;
    mEosPTS = -1;
}

android::binder::Status VideoSinkControllerListener::onVideoUnderflow()
{
    UT_LOG_INFO("\n=======onVideoUnderflow=============\n");
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mUnderflowReceived = true;
    }
    mCV.notify_all();
    return android::binder::Status::ok();
}

bool VideoSinkControllerListener::hasUnderflowReceived()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mUnderflowReceived;
}

void VideoSinkControllerListener::resetUnderflow()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mUnderflowReceived = false;
}

android::binder::Status VideoSinkControllerListener::onVideoResumed()
{
    UT_LOG_INFO("\n=======onVideoResumed=============\n");
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mResumedReceived = true;
    }
    mCV.notify_all();
    return android::binder::Status::ok();
}

bool VideoSinkControllerListener::hasResumedReceived()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mResumedReceived;
}

void VideoSinkControllerListener::resetResumed()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mResumedReceived = false;
}

android::binder::Status VideoSinkControllerListener::onStateChanged(State /*oldState*/, State /*newState*/)
{
    return android::binder::Status::ok();
}

bool VideoSinkControllerListener::waitForFlushComplete(std::chrono::seconds timeout)
{
    std::unique_lock<std::mutex> lock(mMutex);
    return mCV.wait_for(lock, timeout, [this] { return mFlushComplete; });
}

bool VideoSinkControllerListener::waitForFlushCompleteEvent(std::chrono::seconds timeout)
{
    std::unique_lock<std::mutex> lock(mMutex);
    return mCV.wait_for(lock, timeout, [this]() { return mFlushComplete; });
}

bool VideoSinkControllerListener::waitForEosEvent(int timeoutSeconds)
{
    std::unique_lock<std::mutex> lock(mMutex);
    return mCV.wait_for(lock, std::chrono::seconds(timeoutSeconds), [this] { return mEosReceived; });
}

bool VideoSinkControllerListener::waitForUnderflowEvent(int timeoutSeconds)
{
    std::unique_lock<std::mutex> lock(mMutex);
    return mCV.wait_for(lock, std::chrono::seconds(timeoutSeconds), [this] { return mUnderflowReceived; });
}

bool VideoSinkControllerListener::waitForResumedEvent(int timeoutSeconds)
{
    std::unique_lock<std::mutex> lock(mMutex);
    return mCV.wait_for(lock, std::chrono::seconds(timeoutSeconds), [this] { return mResumedReceived; });
}

void VideoSinkControllerListener::setSinkID(int32_t id)
{
    mSinkID = id;
}

VideoSinkEventListener::VideoSinkEventListener()
    : mCurrentState(State::CLOSED),
      mFlushComplete(false)
{
}

::android::binder::Status VideoSinkEventListener::onFirstFrameRendered(int64_t /*nsPresentationTime*/) {
    UT_LOG_INFO("\n=======onFirstFrameRendered: %d, %d=============\n");
    return ::android::binder::Status::ok();
}

::android::binder::Status VideoSinkEventListener::onEndOfStream(int64_t /*nsPresentationTime*/) {
    UT_LOG_INFO("\n=======onEndOfStream: %d, %d=============\n");
    return ::android::binder::Status::ok();
}

::android::binder::Status VideoSinkEventListener::onVideoUnderflow() {
    UT_LOG_INFO("\n=======onVideoUnderflow: %d, %d=============\n");
    return ::android::binder::Status::ok();
}

::android::binder::Status VideoSinkEventListener::onVideoResumed() {
    UT_LOG_INFO("\n=======onVideoResumed: %d, %d=============\n");
    return ::android::binder::Status::ok();
}

::android::binder::Status VideoSinkEventListener::onFlushComplete() {
    UT_LOG_INFO("\n=======onFlushComplete: %d, %d=============\n");
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mFlushComplete = true;
    }
    mCV.notify_all();
    return ::android::binder::Status::ok();
}

::android::binder::Status VideoSinkEventListener::onStateChanged(State oldState, State newState) {
    UT_LOG("\n=======State Change received: %d, %d=============\n", oldState, newState);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mStateChanges.emplace_back(oldState, newState);
        mCurrentState = newState;
    }
    mCV.notify_all();
    return ::android::binder::Status::ok();
}

void VideoSinkEventListener::resetFlushComplete() {
    std::lock_guard<std::mutex> lock(mMutex);
    mFlushComplete = false;
}

bool VideoSinkEventListener::hasFlushComplete() {
    std::lock_guard<std::mutex> lock(mMutex);
    return mFlushComplete;
}

bool VideoSinkEventListener::waitForFlushComplete(std::chrono::seconds timeout) {
    std::unique_lock<std::mutex> lock(mMutex);
    return mCV.wait_for(lock, timeout, [this]() { return mFlushComplete; });
}

bool VideoSinkEventListener::waitForState(State desiredState, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mMutex);
    return mCV.wait_for(lock, timeout, [this, desiredState]() {
        return (mCurrentState == desiredState);
    });
}

std::vector<std::pair<State, State>> VideoSinkEventListener::getStateChanges() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mStateChanges;
}

void VideoSinkEventListener::resetStateChanges() {
    std::lock_guard<std::mutex> lock(mMutex);
    mStateChanges.clear();
}

bool VideoSinkEventListener::checkStateTransistion(State expOldState, State expNewState) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (const auto& [oldState, newState] : mStateChanges) {
        if (oldState == expOldState && newState == expNewState) {
            return true;
        }
    }
    return false;
}

std::vector<int32_t> VideoSinkEventListener::getErrors() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mErrors;
}

void VideoSinkEventListener::setSinkID(int32_t id)
{
    mSinkID = id;
}
/** @} */ // End of VIDEO_SINK_HALTEST_HEADER
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK
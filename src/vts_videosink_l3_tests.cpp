/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**
 * @addtogroup HPK Hardware Porting Kit
 * @{
 *
 */

/**
 * @addtogroup VIDEO_SINK Video Sink Module
 * @{
 *
 */

/**
 * @defgroup VIDEO_SINK_HALTEST Video Sink HAL Tests
 * @{
 *
 */

/**
 * @defgroup VIDEO_SINK_HALTEST_L3 Video Sink HAL Tests L3 File
 * @{
 * @parblock
 *
 * ### L3 Test Cases for Video Sink HAL :
 *
 * This module includes Level 3 functional tests (success and failure scenarios).
 * This is to ensure that the API meets the operational requirements of the module across all vendors.
 *
 * **Pre-Conditions:** @n
 * **Dependencies:** None@n
 *
 * @endparblock
 *
 */

/**
 * @file vts_videosink_l3_tests.c
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

#include "com/rdk/hal/videodecoder/IVideoDecoderManager.h"
#include "com/rdk/hal/videodecoder/BnVideoDecoderControllerListener.h"
#include "com/rdk/hal/videodecoder/BnVideoDecoderEventListener.h"

#include "com/rdk/hal/avbuffer/IAVBuffer.h"
#include "com/rdk/hal/avbuffer/BnAVBufferSpaceListener.h"
#include "com/rdk/hal/PropertyValue.h"


#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"
#include "vts_videosink_parse_testconfig.h"
#include "vts_videosink.h"

using namespace std::chrono;
using namespace std;

/* Test class registration with UT_Core */
UT_ADD_TEST_TO_GROUP(VTS_L3_VideoSink, UT_TESTS_L3)

static const char *gModuleName = "IVideoSinkManager";
static const char *gTestGroup = "L3";

sp<IVideoDecoderManager> gVideoDecoderManager;

UT_status_t getVideoDecoderService() {
    gVideoDecoderManager = android::interface_cast<IVideoDecoderManager>(
       defaultServiceManager()->getService(String16(IVideoDecoderManager::serviceName().c_str())));
   if (gVideoDecoderManager == nullptr) {
       printf("Service not available\n");
       return UT_STATUS_FAILURE;
   }
   return UT_STATUS_OK;
}

>>>>>>> 030df10 (gh #25 develop L3 test cases)
sp<IAVBuffer> vts_avbuffer_getService()
{
    return
        android::interface_cast<IAVBuffer>(
            android::defaultServiceManager()->getService(
                android::String16(IAVBuffer::serviceName().c_str())
            )
        );
}

class VideoDecoderControllerListener : public BnVideoDecoderControllerListener {
public:
    VideoDecoderControllerListener() = default;
    virtual ~VideoDecoderControllerListener() = default;

    android::binder::Status onFrameOutput(
        int64_t nsPresentationTime,
        int64_t frameBufferHandle,
        const std::optional<com::rdk::hal::videodecoder::FrameMetadata>& metadata) override
    {
        UT_LOG_INFO("onFrameOutput: PTS = %x, FrameBufferHandle = %x", nsPresentationTime, frameBufferHandle);

        if (metadata.has_value()) {
            const auto& meta = metadata.value();
            UT_LOG_INFO("Metadata: width=%d, height=%d, format=%d",
                        meta.activeWidth, meta.activeHeight, static_cast<int>(meta.pixelFormat));
        } else {
            UT_LOG_ERROR("onFrameOutput: No metadata provided.");
        }

        return android::binder::Status::ok();
    }

    android::binder::Status onUserDataOutput(
        int64_t nsPresentationTime,
        const std::vector<uint8_t>& userData) override
    {
        UT_LOG_INFO("onUserDataOutput: PTS = %x, UserDataSize = %zu", nsPresentationTime, userData.size());

        // Optionally print or parse user data
        if (!userData.empty()) {
            UT_LOG_INFO("First byte of user data: 0x%02X", userData[0]);
        }

        return android::binder::Status::ok();
    }
};

class VideoDecoderEventListener : public BnVideoDecoderEventListener {
    public:
    VideoDecoderEventListener() {};
    virtual ~VideoDecoderEventListener() = default;

    Status onDecodeError(ErrorCode errorCode, int32_t vendorErrorCode) override {
        return Status::ok();
    }
    Status onStateChanged(State oldState,State newState) override
        {
            UT_LOG("State Change received: %s -> %s", com::rdk::hal::toString(oldState).c_str(), com::rdk::hal::toString(newState).c_str());
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStateChanges.emplace_back(oldState, newState);
                mCurrentState = newState;
            }
            mCV.notify_all(); // Unblock any waiting test code
            return Status::ok();
        }
        /**
         * Wait for the sink to reach a particular State within a given timeout.
         *
         * @param desiredState The state we want to observe.
         * @param timeout      How long to wait, e.g. std::chrono::milliseconds(500).
         * @return true if the state was reached; false if we timed out waiting.
         */
        bool waitForState(State desiredState, std::chrono::milliseconds timeout)
        {
            std::unique_lock<std::mutex> lock(mMutex);
            // Wait until mCurrentState == desiredState or until timeout
            return mCV.wait_for(lock, timeout, [this, desiredState]() {
                return (mCurrentState == desiredState);
            });
        }
        // Accessors for test verifications if desired
        std::vector<std::pair<State, State>> getStateChanges() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mStateChanges;
        }

        void resetStateChanges()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStateChanges.clear();
        }

        bool checkStateTransistion(State expOldState, State expNewState)
        {
            bool foundClosedToOpening = false;
            for (const auto &[oldState, newState] : mStateChanges)
            {
                if (oldState == expOldState && newState == expNewState)
                {
                    foundClosedToOpening = true;
                    break;
                }
            }
            return foundClosedToOpening;
        }

        std::vector<int32_t> getErrors() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mErrors;
        }
    private:
        mutable std::mutex mMutex;
        std::condition_variable mCV;

        State mCurrentState;
        std::vector<std::pair<State, State>> mStateChanges;
        std::vector<int32_t> mErrors;
};


#define AVBUFFER_DESTROY_POOL_SUCCESS(avBufferManager, pool) \
    do { \
        bool _aidl_return = false; \
        Status status; \
        UT_LOG_INFO("Calling destroyPool(IN:poolHandle:[%d], OUT:_aidl_return:[])", pool); \
        status = avBufferManager->destroyPool(pool, &_aidl_return); \
        UT_LOG_INFO("Result destroyPool with (IN:poolHandle:[%d], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", \
                    pool.handle, UT_Control_GetMapString(bool_mapTable, _aidl_return), \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_TRUE(_aidl_return); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AVBUFFER_ALLOC_SUCCESS(avBufferManager, pool, size, bufferHandle) \
    do { \
        UT_LOG_INFO("Calling alloc(IN:poolHandle:[%d], IN:size:[%d], OUT:bufferHandle)", pool.handle, size); \
        Status status = avBufferManager->alloc(pool, size, &bufferHandle); \
        UT_LOG_INFO("Result alloc(IN:poolHandle:[%d], IN:size:[%d], OUT:bufferHandle:[0x%X]), status:[isOK:%s, exception:%s]", \
                    pool.handle, size, bufferHandle, UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_NOT_EQUAL(bufferHandle, IAVBuffer::INVALID_HANDLE); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AVBUFFER_FREE_SUCCESS(avBufferManager, bufferHandle) \
    do { \
        bool _aidl_return = false; \
        UT_LOG_INFO("Calling free(IN:bufferHandle:[0x%X], OUT:_aidl_return)", bufferHandle); \
        Status status = avBufferManager->free(bufferHandle, &_aidl_return); \
        UT_LOG_INFO("Result free(IN:bufferHandle:[0x%X], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", \
                    bufferHandle, UT_Control_GetMapString(bool_mapTable, _aidl_return), \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_TRUE(_aidl_return); \
    } while (0)

#define AVBUFFER_GETMETRICS(avBufferManager, secureHeap, heapMetrics) \
    do { \
        UT_LOG_INFO("Calling getHeapMetrics(IN:secureHeap:[%s], OUT:heapMetrics[])", \
                    UT_Control_GetMapString(bool_mapTable, secureHeap)); \
        Status status = avBufferManager->getHeapMetrics(secureHeap, &heapMetrics); \
        UT_LOG_INFO("Result getHeapMetrics(IN:secureHeap:[%s], OUT:heapMetrics[secure:%s, bytesUsed:%d, bytesTotal:%d]), status:[isOK:%s, exception:%s]", \
                    UT_Control_GetMapString(bool_mapTable, secureHeap), \
                    UT_Control_GetMapString(bool_mapTable, heapMetrics.secure), \
                    heapMetrics.bytesUsed, \
                    heapMetrics.bytesTotal, \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AVBUFFER_CREATE_VIDEOPOOL_SUCCESS(avBufferManager, secureHeap, videoDecoderId, listener, videoPool) \
do { \
    UT_LOG_INFO("Calling createVideoPool(IN:secureHeap:[%d], IN:videoDecoderId:[%d], IN:listener:[%p], OUT:videoPool)", \
                 secureHeap, videoDecoderId.value, listener.get()); \
    Status status = avBufferManager->createVideoPool(secureHeap, videoDecoderId, listener, &videoPool); \
    UT_LOG_INFO("Result createVideoPool(IN:secureHeap:[%d], IN:videoDecoderId:[%d], IN:listener:[%p], OUT:videoPool.handle:[%d]), status:[isOK:%s, exception:%s]", \
                 secureHeap, videoDecoderId.value, listener.get(), videoPool.handle, \
                 UT_Control_GetMapString(bool_mapTable, status.isOk()), UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
    UT_ASSERT_TRUE(status.isOk()); \
    UT_ASSERT_NOT_EQUAL(videoPool.handle, Pool::INVALID); \
    UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
} while (0)

#define VIDEODECODER_GETSTATE(videoDecoder, state) \
    do { \
        Status status; \
        UT_LOG_INFO("Calling getState(OUT:_aidl_return:[state])"); \
        status = (videoDecoder)->getState(&state); \
        UT_LOG_INFO("Result getState(OUT:_aidl_return:[state:%d]), status:[isOK:%s, exception:%s]", \
                    static_cast<int32_t>(state), \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()) << "getState binder call failed"; \
        UT_ASSERT_TRUE((state) == State::UNKNOWN || \
                       (state) == State::CLOSED || \
                       (state) == State::OPENING || \
                       (state) == State::READY || \
                       (state) == State::STARTING || \
                       (state) == State::STARTED || \
                       (state) == State::FLUSHING || \
                       (state) == State::STOPPING || \
                       (state) == State::CLOSING) \
            << "Invalid state returned: " << static_cast<int32_t>(state); \
    } while (0)

#define VIDEODECODER_WAIT_FOR_STATE(videoDecoder, targetState, lastState) \
    do { \
        int32_t sleep_time = 0; \
        do \
        { \
            std::this_thread::sleep_for(std::chrono::milliseconds(MAX_TRANSITORY_STATE_WAIT_MS)); \
            sleep_time += MAX_TRANSITORY_STATE_WAIT_MS; \
            /* Check the state of the decoder */ \
            VIDEODECODER_GETSTATE(videoDecoder, lastState); \
        } while (sleep_time < MAX_FINAL_STATE_WAIT_MS && lastState != targetState); \
    } while (0)

#define VIDEODECODER_MANAGER_GETVIDEODECODERIDS(decoderIds) \
    do { \
        Status status; \
        UT_LOG_INFO("Calling getVideoDecoderIds(OUT:decoderIds:[])"); \
        status = gVideoDecoderManager->getVideoDecoderIds(&decoderIds); \
        UT_LOG_INFO("Result getVideoDecoderIds(OUT:decoderIds:[size: %d]), status:[isOK:%s, exception:%s]", \
                                decoderIds.size(), UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_FALSE_FATAL(decoderIds.empty()); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEODECODER_MANAGER_GETVIDEODECODER(_decoderId, videoDecoder) \
    do { \
        Status status; \
        UT_LOG_INFO("Calling getVideoDecoder(IN:_decoderId:[%d], OUT:videoDecoder[])", _decoderId.value); \
        status = gVideoDecoderManager->getVideoDecoder(_decoderId, &videoDecoder); \
        UT_LOG_INFO("Result getVideoDecoder(IN:_decoderId:[%d], OUT:videoDecoder[0x%X]), status:[isOK:%s, exception:%s]", \
                                _decoderId.value, videoDecoder.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_TRUE_FATAL(videoDecoder.get() != nullptr); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEODECODER_REGISTER_EVENT_SUCCESS(videoDecoder, listener) \
    do { \
        Status status; \
        bool aidl_return; \
        sp<IVideoDecoderEventListener> tempListener = (listener); \
        void* listenerPtr = tempListener ? tempListener.get() : nullptr; \
        UT_LOG_INFO("Calling registerEventListener(IN:videoDecoderEventListener:[0x%p], OUT:_aidl_return[])", listenerPtr); \
        status = (videoDecoder)->registerEventListener((listener), &aidl_return); \
        UT_LOG_INFO("Result registerEventListener(IN:videoDecoderEventListener:[0x%p], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", \
                    listenerPtr, UT_Control_GetMapString(bool_mapTable, aidl_return), \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()) << "registerEventListener binder call failed"; \
        UT_ASSERT_TRUE(aidl_return) << "registerEventListener returned false"; \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE) << "registerEventListener returned an exception"; \
    } while (0)

#define VIDEODECODER_GETCAPABILITIES(videoDecoder, capabilities) \
     do { \
         Status status; \
         UT_LOG_INFO("Calling getCapabilities(OUT:capabilities)"); \
         status = videoDecoder->getCapabilities(capabilities); \
         UT_LOG_INFO("Result getCapabilities(OUT:capabilities), status:[isOK:%s, exception:%s]", \
                     UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                     UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
         UT_ASSERT_TRUE(status.isOk()); \
         UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
     } while (0)

#define VIDEODECODER_UNREGISTER_EVENT_SUCCESS(videoDecoder, listener) \
    do { \
        Status status; \
        bool aidl_return; \
        sp<IVideoDecoderEventListener> tempListener = (listener); \
        void* listenerPtr = tempListener ? tempListener.get() : nullptr; \
        UT_LOG_INFO("Calling unregisterEventListener(IN:videoDecoderEventListener:[0x%p], OUT:_aidl_return[])", listenerPtr); \
        status = (videoDecoder)->unregisterEventListener((listener), &aidl_return); \
        UT_LOG_INFO("Result unregisterEventListener(IN:videoDecoderEventListener:[0x%p], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", \
                    listenerPtr, UT_Control_GetMapString(bool_mapTable, aidl_return), \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()) << "unregisterEventListener binder call failed"; \
        UT_ASSERT_TRUE(aidl_return) << "unregisterEventListener returned false"; \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE) << "unregisterEventListener returned an exception"; \
    } while (0)

#define VIDEODECODER_OPEN_SUCCESS(videoDecoder, codec, secure, listener, controller, stateCheck) \
    do { \
        Status status; \
        UT_LOG_INFO("Calling open(IN:codec:[%d], IN:secure:[%s], IN:listener:[0x%p], OUT:controller[])", \
                    static_cast<int32_t>(codec), UT_Control_GetMapString(bool_mapTable, (secure)), (listener).get()); \
        status = (videoDecoder)->open((codec), (secure), (listener), &(controller)); \
        UT_LOG_INFO("Result open(IN:codec:[%d], IN:secure:[%s], IN:listener:[0x%p], OUT:controller:[0x%X]), status:[isOK:%s, exception:%s]", \
                    static_cast<int32_t>(codec), UT_Control_GetMapString(bool_mapTable, (secure)), (listener).get(), (controller).get(), \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()) << "open binder call failed"; \
        UT_ASSERT_NOT_EQUAL_FATAL((controller).get(), nullptr) << "Controller is null; codec or secure mode may not be supported"; \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE) << "open returned an exception"; \
        if (stateCheck) { \
            State lastState; \
            VIDEODECODER_WAIT_FOR_STATE(videoDecoder, State::READY, lastState); \
            UT_ASSERT_EQUAL(lastState, State::READY) << "Decoder did not reach READY state after open"; \
        } \
    } while (0)

#define VIDEODECODER_START_SUCCESS(videoDecoder, videoDecoderController, stateCheck) \
    do { \
        Status status; \
        UT_LOG_INFO("Calling start()"); \
        status = (videoDecoderController)->start(); \
        UT_LOG_INFO("Result start(), status:[isOK:%s, exception:%s]", \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()) << "start binder call failed"; \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE) << "start returned an exception"; \
        if (stateCheck) { \
            State lastState; \
            VIDEODECODER_WAIT_FOR_STATE(videoDecoder, State::STARTED, lastState); \
            UT_ASSERT_EQUAL(lastState, State::STARTED) << "Decoder did not reach STARTED state"; \
        } \
    } while (0)

#define VIDEODECODER_DECODEBUFFER_SUCCESS(videoDecoderHandle, nsPresentationTime, bufferHandle) \
=======
#define VIDEODECODER_DECODEBUFFER_SUCCESS(videoDecoderController, nsPresentationTime, bufferHandle) \
>>>>>>> 030df10 (gh #25 develop L3 test cases)
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_INFO("Calling decodeBuffer(IN:nsPresentationTime[%ld], IN:bufferHandle[0x%lX], OUT:_aidl_return[])", \
            nsPresentationTime, bufferHandle); \
        status = videoDecoderController->decodeBuffer(nsPresentationTime, bufferHandle, &_aidl_return); \
        UT_LOG_INFO("Result decodeBuffer(IN:nsPresentationTime[%ld], IN:bufferHandle[0x%lX], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            nsPresentationTime, bufferHandle, \
            UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEODECODER_DECODEBUFFER_FAILURE(videoDecoderController, nsPresentationTime, bufferHandle, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_INFO("Calling decodeBuffer(IN:nsPresentationTime[%ld], IN:bufferHandle[0x%lX], OUT:_aidl_return[])", \
            nsPresentationTime, bufferHandle); \
        status = videoDecoderController->decodeBuffer(nsPresentationTime, bufferHandle, &_aidl_return); \
        UT_LOG_INFO("Result decodeBuffer(IN:nsPresentationTime[%ld], IN:bufferHandle[0x%lX], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            nsPresentationTime, bufferHandle, \
            UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEODECODER_SETPROPERTY_SUCCESS(videoDecoderController, property, propertyValue) \
    do { \
        Status status; \
        bool result; \
        UT_LOG_INFO("Calling setProperty(IN:property[%d], IN:propertyValue=%d)", \
                    static_cast<int32_t>(property), propertyValue.value); \
        status = videoDecoderController->setProperty(property, propertyValue, &result); \
        UT_LOG_INFO("Result setProperty(IN:property[%d], OUT:result=%s), status:[isOK:%s, exception:%s]", \
                    static_cast<int32_t>(property), UT_Control_GetMapString(bool_mapTable, result), \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_TRUE(result); \
    } while (0)

#define VIDEODECODER_STOP_SUCCESS(videoDecoder, videoDecoderController, stateCheck) \
    do { \
        Status status; \
        UT_LOG_INFO("Calling stop()"); \
        status = videoDecoderController->stop(); \
        UT_LOG_INFO("Result stop(), status:[isOK:%s, exception:%s]", \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) { \
            State lastState; \
            VIDEODECODER_WAIT_FOR_STATE(videoDecoder, State::READY, lastState); \
            UT_ASSERT_EQUAL(lastState, State::READY) << "Decoder did not reach READY state"; \
        } \
    } while (0)

class AVBufferSpaceListener : public BnAVBufferSpaceListener
{
public:

    AVBufferSpaceListener() {};
    virtual ~AVBufferSpaceListener() = default;

    Status onSpaceAvailable() override
    {
        return Status::ok();
    }
};

class SinkTest
{
public:
    SinkTest(IVideoSink::Id sinkId, sp<IVideoSink> sinkHandle,
            sp<VideoSinkControllerListener> VideoSinkControllerListener,
            sp<VideoSinkEventListener> VideoSinkEventListner,
            sp<IVideoDecoder> decoderHandle,
            IVideoDecoder::Id decoderId,
            sp<IVideoDecoderControllerListener> decoderControllerListener,
            sp<IVideoDecoderEventListener> decoderEventListener,
            sp<IAVBuffer> AVBufferManager,
            bool secure,
            videoTestConfiguration_t& config)
    {
        mSinkId = sinkId;
        mSinkHandle = sinkHandle;
        mVideoSinkControlListener = VideoSinkControllerListener;
        mVideoSinkEventListner = VideoSinkEventListner;
        mDecoderHandle = decoderHandle;
        mDecoderId = decoderId;
        mDecoderControllerListener = decoderControllerListener;
        mDecoderEventListener = decoderEventListener;
        mAVBufferManager = AVBufferManager;
        mSecure = secure;
        mTestConfiguration = config;
    }


    void run ( bool isTunnel )
    {
        if(isTunnel)
        {
            videoSinkNonTunnelMode();
        }
        else
        {
            videoSinkTunnelMode();
        }
    }

    bool getSinkMetrics(int32_t& sinkQueueDepth, int32_t &framesReceived, int32_t &framesDroppedLate, int32_t &framesDroppedFrc, int32_t &framesRepeatedFrc,
                        int32_t& framesMissingFrame)
    {
        std::optional<com::rdk::hal::PropertyValue> propertyValue;

        sinkQueueDepth = framesReceived = framesDroppedLate = framesDroppedFrc = framesRepeatedFrc = framesMissingFrame = -1;

        VIDEOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, videosink::Property::SINK_QUEUE_DEPTH, propertyValue);
         if (!propertyValue.has_value() || !propertyValue->value.has_value())
        {
            return false;
        }

        sinkQueueDepth = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();

        VIDEOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, videosink::Property::METRIC_FRAMES_RECEIVED, propertyValue);
         if (!propertyValue.has_value() || !propertyValue->value.has_value())
        {
            return false;
        }

        framesReceived = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();

        VIDEOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, videosink::Property::METRIC_FRAMES_DROPPED_LATE, propertyValue);
         if (!propertyValue.has_value() || !propertyValue->value.has_value())
        {
            return false;
        }

        framesDroppedLate = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();


        VIDEOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, videosink::Property::METRIC_FRAMES_DROPPED_FRC, propertyValue);
        if (!propertyValue.has_value() || !propertyValue->value.has_value())
        {
            return false;
        }

        framesDroppedFrc = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();

        VIDEOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, videosink::Property::METRIC_FRAMES_REPEATED_FRC, propertyValue);
        if (!propertyValue.has_value() || !propertyValue->value.has_value())
        {
            return false;
        }

        framesRepeatedFrc = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();

        VIDEOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, videosink::Property::METRIC_FRAMES_REPEATED_FRC, propertyValue);
        if (!propertyValue.has_value() || !propertyValue->value.has_value())
        {
            return false;
        }

        framesRepeatedFrc = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();

        VIDEOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME, propertyValue);
        if (!propertyValue.has_value() || !propertyValue->value.has_value())
        {
            return false;
        }

        framesMissingFrame = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();

        return true;
    }

private:
    IVideoSink::Id mSinkId;
    sp<IVideoSink> mSinkHandle;
    sp<VideoSinkControllerListener> mVideoSinkControlListener;
    sp<VideoSinkEventListener> mVideoSinkEventListner;
    IVideoDecoder::Id mDecoderId;
    sp<IVideoDecoder> mDecoderHandle;
    sp<IAVBuffer> mAVBufferManager;
    sp<IVideoSinkController> mVideoSinkController;
    sp<IVideoDecoderControllerListener> mDecoderControllerListener;
    sp<IVideoDecoderEventListener> mDecoderEventListener;
    Pool mVideoPool;
    bool mSecure;
    videoTestConfiguration_t mTestConfiguration;

    void videoSinkNonTunnelMode()
    {
        sp<IVideoSinkController> controller;
        sp<IAVBufferSpaceListener> bufferListener = new AVBufferSpaceListener();

        // Open Sink
        VIDEOSINK_OPEN_SUCCESS(mSinkHandle, mVideoSinkControlListener, controller, stateCheck);
        assert(stateCheck == SinkState::READY);

        // Start Sink
        VIDEOSINK_START_SUCCESS(mSinkHandle, controller, stateCheck);
        assert(stateCheck == SinkState::STARTED);

        AVBUFFER_CREATE_VIDEOPOOL_SUCCESS(mAVBufferManager, mSecure, mDecoderId, mDecoderHandle, bufferListener, mVideoPool);

        std::vector<uint8_t> dummyFrame(mTestConfiguration.videoFrame.bufferSize, 0x80);
        VIDEOSINK_OPEN_SUCCESS(mSinkHandle, mVideoSinkControlListener, controller, true);

        // Start Sink
        VIDEOSINK_START_SUCCESS(mSinkHandle, controller, true);

        AVBUFFER_CREATE_VIDEOPOOL_SUCCESS(mAVBufferManager, mSecure, mDecoderId, bufferListener, mVideoPool);

        std::vector<uint8_t> dummyFrame(mTestConfiguration.frameMetadata.bufferSize, 0x80);

        /*Copy the dummy frame using mmap of avbuffer helper*/
        /*TODO Yet to implement the copying of the buffer*/

        int32_t repeatCount = mTestConfiguration.repeatPerFrame > 0 ? mTestConfiguration.repeatPerFrame : 1;

        for (const auto& frame : mTestConfiguration.frames)
        {
            for (int32_t repeat = 0; repeat < repeatCount; ++repeat)
            {
                int64_t pts = frame.pts;

                int64_t bufferHandle;
                AVBUFFER_ALLOC_SUCCESS(mAVBufferManager, mVideoPool, frame.size, bufferHandle);

                FrameMetadata metadata;
                metadata.parX = 1;
                metadata.parY = 1;
                metadata.sarX = 16;
                metadata.sarY = 9;
                metadata.codedWidth  = mTestConfiguration.frameMetadata.width;
                metadata.codedHeight = mTestConfiguration.frameMetadata.height;
                metadata.activeX = 0;
                metadata.activeY = 0;
                metadata.activeWidth  = mTestConfiguration.frameMetadata.width;
                metadata.activeHeight = mTestConfiguration.frameMetadata.height;
                metadata.colorDepth   = mTestConfiguration.frameMetadata.colorDepth;
                metadata.pixelFormat  = static_cast<PixelFormat>(mTestConfiguration.frameMetadata.pixelFormat);
                metadata.dynamicRange = DynamicRange::SDR;
                metadata.scanType     = ScanType::PROGRESSIVE_FRAME;
                metadata.afd = 0;
                metadata.frameRateNumerator = 24;
                metadata.frameRateDenominator = 1;
                metadata.endOfStream = (mTestConfiguration.eosPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.eosPts && repeat == repeatCount - 1);
                metadata.discontinuity = (mTestConfiguration.signalDiscontinuityPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.signalDiscontinuityPts && repeat == 0);
                metadata.lowLatency = false;
                metadata.source = AVSource::HDMI_1;
                metadata.sha1 = {0x12, 0x34, 0x56, static_cast<uint8_t>((pts + repeat) % 256)};

                uint64_t presentedTime = 0;
                VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_SUCCESS(controller, presentedTime, bufferHandle, metadata);

                if (repeat == 0)
                {
                    if (mTestConfiguration.flushPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.flushPts)
                        VIDEOSINK_FLUSH_SUCCESS(mSinkHandle, controller, false);

                    if (mTestConfiguration.flushResetPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.flushResetPts)
                    {
                        VIDEOSINK_FLUSH_SUCCESS(mSinkHandle, controller, true);
                        VIDEOSINK_START_SUCCESS(mSinkHandle, controller, true);
                    }

                    if (frame.errorFrame)
                    {
                        UT_LOG_ERROR("Injected error frame at PTS: %ld", pts);
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                AVBUFFER_FREE_SUCCESS(mAVBufferManager, bufferHandle);
            }
        }

        // Stop & Cleanup
        VIDEOSINK_STOP_SUCCESS(mSinkHandle, controller, true);

        VIDEOSINK_CLOSE_SUCCESS(mSinkHandle, controller, true);

        AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, mVideoPool);
    }


    void videoSinkTunnelMode()
    {
        sp<IVideoSinkController> controller;
        sp<IVideoDecoderController> videoDecoderController;
        sp<IAVBufferSpaceListener> bufferListener = new AVBufferSpaceListener();

        // Open Sink
        VIDEOSINK_OPEN_SUCCESS(mSinkHandle, mVideoSinkControlListener, controller, true);

        // Start Sink
        VIDEOSINK_START_SUCCESS(mSinkHandle, controller, true);

        AVBUFFER_CREATE_VIDEOPOOL_SUCCESS(mAVBufferManager, mSecure, mDecoderId, bufferListener, mVideoPool);

        std::ifstream inputFile(mTestConfiguration.input, std::ios::binary);
        if (!inputFile)
        {
            UT_LOG_ERROR("Failed to open file: %s", mTestConfiguration.input.c_str());
            return;
        }

        // Open Decoder
        VIDEODECODER_OPEN_SUCCESS(mDecoderHandle, mTestConfiguration.codec, mSecure, mDecoderControllerListener, videoDecoderController, true);

        // Set OPERATIONAL_MODE to TUNNELLED
        PropertyValue propertyValue;
        propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(
            static_cast<int32_t>(OperationalMode::TUNNELLED));
        VIDEODECODER_SETPROPERTY_SUCCESS(videoDecoderController, com::rdk::hal::videodecoder::Property::OPERATIONAL_MODE, propertyValue);

        // Start Decoder
        VIDEODECODER_START_SUCCESS(mDecoderHandle, videoDecoderController, true);

        for (const auto& frame : mTestConfiguration.frames)
        {
            int64_t pts = frame.pts;
            uint32_t frameSize = frame.size;
            bool isErrorFrame = frame.errorFrame;

            int64_t videoBufferHandle = -1;

            AVBUFFER_ALLOC_SUCCESS(mAVBufferManager, mVideoPool, frameSize, videoBufferHandle);

            //map the buffer

            // Handle control signals
            if (mTestConfiguration.flushPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.flushPts)
            {
                VIDEOSINK_FLUSH_SUCCESS(mSinkHandle, mVideoSinkController, false);
            }

            if (mTestConfiguration.flushResetPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.flushResetPts)
            {
                VIDEOSINK_FLUSH_SUCCESS(mSinkHandle, mVideoSinkController, true);
                VIDEOSINK_START_SUCCESS(mSinkHandle, mVideoSinkController, true);
            }

            if (mTestConfiguration.eosPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.eosPts)
            {
                // Send EOS (expect failure)
                VIDEODECODER_DECODEBUFFER_FAILURE(videoDecoderController, pts, videoBufferHandle, Status::EX_NONE);

                // Restart decoder
                VIDEODECODER_STOP_SUCCESS(mDecoderHandle, videoDecoderController, true);
                VIDEODECODER_START_SUCCESS(mDecoderHandle, videoDecoderController, true);
            }
            else
            {
                // Normal decode
                VIDEODECODER_DECODEBUFFER_SUCCESS(videoDecoderController, pts, videoBufferHandle);
            }

            AVBUFFER_FREE_SUCCESS(mAVBufferManager, videoBufferHandle);
        }
    }


};

class VTS_L3_VideoSink : public UTCore
{
public:
    VTS_L3_VideoSink() : UTCore() {}

    ~VTS_L3_VideoSink() override = default;

    vector<tuple<IVideoSink::Id, sp<IVideoSink>, sp<VideoSinkControllerListener>, sp<VideoSinkEventListener>>> mVideoSinkList;
    vector<tuple<IVideoDecoder::Id, sp<IVideoDecoder>, sp<IVideoDecoderControllerListener>, sp<IVideoDecoderEventListener>, videodecoder::Capabilities*>> mVideoDecoderList;

    sp<IAVBuffer> mAVBufferManager;

    void SetUp() override
    {
        vector<IVideoSink::Id> sinkIds;
        vector<IVideoDecoder::Id> videoDecoderIds;

        mAVBufferManager = vts_avbuffer_getService();
        gTestConfiguration = vts_kvp_create_instance(VTS_TEST_CONFIG_PATH);

        /*Get the Video Sink IDs*/
        VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

        /*Get the Video Decoder IDs*/
        VIDEODECODER_MANAGER_GETVIDEODECODERIDS(videoDecoderIds);

        android::ProcessState::self()->startThreadPool();

        for (const auto& _videoDecoderId : videoDecoderIds)
        {
            sp<IVideoDecoder> videoDecoder;

            videodecoder::Capabilities* videoDecoderCapabilities = new videodecoder::Capabilities();

            sp<IVideoDecoderEventListener> videoDecoderEventListener = new VideoDecoderEventListener();

            sp<IVideoDecoderControllerListener> videoDecoderControlListener = new VideoDecoderControllerListener();

            VIDEODECODER_MANAGER_GETVIDEODECODER(_videoDecoderId, videoDecoder);
            VIDEODECODER_REGISTER_EVENT_SUCCESS(videoDecoder, videoDecoderEventListener);
            VIDEODECODER_GETCAPABILITIES(videoDecoder, videoDecoderCapabilities);

            mVideoDecoderList.push_back({_videoDecoderId, videoDecoder, videoDecoderControlListener, videoDecoderEventListener, videoDecoderCapabilities});
        }

        for(const auto& _sinkId: sinkIds)
        {
            sp<IVideoSink> videoSink;


            sp<VideoSinkEventListener> videoSinkEventListener = new VideoSinkEventListener();

            sp<VideoSinkControllerListener> videoSinkControlListener = new VideoSinkControllerListener();

            /* Get Video Sink handle */
            VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

            /* Register events */
            VIDEOSINK_REGISTER_EVENT_SUCCESS(videoSink, videoSinkEventListener);

            mVideoSinkList.push_back({_sinkId, videoSink, videoSinkControlListener, videoSinkEventListener});

        }
    }

    void runSequentialSink(videoTestConfiguration_t& testConfig, bool secure, bool tunnel)
    {
        for (size_t i = 0; i < mVideoSinkList.size(); ++i)
        {
            auto& [_sinkId, videoSink, videoSinkControlListener, videoSinkEventListener] = mVideoSinkList[i];
            auto& [_decoderId, decoderHandle, decoderControlListener, decoderEventListener, decoderCapabilities] = mVideoDecoderList[i];

            HeapMetrics beforeHeapMetrics, afterHeapMetrics;

            // Skip if the platform doesn't support secure decode
            if (secure && !decoderCapabilities->supportsSecure)
            {
                UT_LOG_INFO("Decoder [%d] doesn't support secure decoding", _decoderId);
                continue;
            }

            // Find if codec supported by this decoder
            auto result = std::find_if(decoderCapabilities->supportedCodecs.begin(),
                                    decoderCapabilities->supportedCodecs.end(),
                                    [&](const com::rdk::hal::videodecoder::CodecCapabilities& cap) {
                                        return cap.codec == testConfig.codec;
                                    });

            if (result == decoderCapabilities->supportedCodecs.end())
            {
                UT_LOG_INFO("Decoder [%d] doesn't support Codec [%s]", _decoderId, videodecoder::toString(testConfig.codec).c_str());
                continue;
            }

            AVBUFFER_GETMETRICS(mAVBufferManager, secure, beforeHeapMetrics);

            SinkTest sinkTest(_sinkId, videoSink,
                videoSinkControlListener, videoSinkEventListener,
                decoderHandle, _decoderId,
                decoderControlListener, decoderEventListener,
                mAVBufferManager, secure, testConfig);

            sinkTest.run(tunnel);

            AVBUFFER_GETMETRICS(mAVBufferManager, secure, afterHeapMetrics);
            UT_ASSERT_TRUE(beforeHeapMetrics.secure == afterHeapMetrics.secure);
            UT_ASSERT_TRUE(beforeHeapMetrics.bytesUsed == afterHeapMetrics.bytesUsed);
            UT_ASSERT_TRUE(beforeHeapMetrics.bytesTotal == afterHeapMetrics.bytesTotal);
        }
    }


    void runConcurrentSink(videoTestConfiguration_t& testConfig, bool secure, bool tunnel)
    {
        std::vector<std::thread> threads;
        std::vector<SinkTest> sinkTests;  // Store SinkTest objects here

        HeapMetrics beforeHeapMetrics, afterHeapMetrics;
        AVBUFFER_GETMETRICS(mAVBufferManager, secure, beforeHeapMetrics);

        for (size_t i = 0; i < mVideoSinkList.size(); ++i)
        {
            auto& [_sinkId, videoSink, videoSinkControlListener, videoSinkEventListener] = mVideoSinkList[i];
            auto& [_decoderId, decoderHandle, decoderControlListener, decoderEventListener, decoderCapabilities] = mVideoDecoderList[i];

            // Skip if the platform doesn't support secure decode
            if (secure && !decoderCapabilities->supportsSecure)
            {
                UT_LOG_INFO("Decoder [%d] doesn't support secure decoding", _decoderId);
                continue;
            }

            // Find if codec supported by this decoder
            auto result = std::find_if(decoderCapabilities->supportedCodecs.begin(),
                                    decoderCapabilities->supportedCodecs.end(),
                                    [&](const com::rdk::hal::videodecoder::CodecCapabilities& cap) {
                                        return cap.codec == testConfig.codec;
                                    });

            if (result == decoderCapabilities->supportedCodecs.end())
            {
                UT_LOG_INFO("Decoder [%d] doesn't support Codec [%s]", _decoderId, videodecoder::toString(testConfig.codec).c_str());
                continue;
            }

            // Construct SinkTest object and keep it alive
            sinkTests.emplace_back(_sinkId, videoSink,
                                videoSinkControlListener, videoSinkEventListener,
                                decoderHandle, _decoderId,
                                decoderControlListener, decoderEventListener,
                                mAVBufferManager, secure, testConfig);
        }

        // Launch threads after creating all SinkTest objects
        for (auto& sinkTest : sinkTests)
        {
            threads.emplace_back(&SinkTest::run, &sinkTest, tunnel);
        }

        for (auto& t : threads)
        {
            t.join();
        }

        AVBUFFER_GETMETRICS(mAVBufferManager, secure, afterHeapMetrics);

        UT_ASSERT_TRUE(beforeHeapMetrics.secure == afterHeapMetrics.secure);
        UT_ASSERT_TRUE(beforeHeapMetrics.bytesUsed == afterHeapMetrics.bytesUsed);
        UT_ASSERT_TRUE(beforeHeapMetrics.bytesTotal == afterHeapMetrics.bytesTotal);
    }


    void runSingleProcessTest(const char *testName, bool secureMode, bool isTunnel, bool concurrentMode)
    {
        vector<videoTestConfiguration_t> testConfiguration;

        vts_videosink_parse_testconfig(gTestConfiguration, gModuleName, gTestGroup, testName, testConfiguration);

        if(concurrentMode)
        {
            for(auto& test: testConfiguration)
            {
                runConcurrentSink(test, secureMode, isTunnel);
            }
        }
        else
        {
            for(auto& test: testConfiguration)
            {
                runSequentialSink(test, secureMode, isTunnel);
            }
        }
    }

    void TearDown() override
    {
        for (const auto& [_videoDecoderId, videoDecoder, videoDecoderControlListener, videoDecoderEventListener, videoDecoderCapabilities] : mVideoDecoderList)
        {
            /* Un-register the events */
            VIDEODECODER_UNREGISTER_EVENT_SUCCESS(videoDecoder, videoDecoderEventListener);
        }
        mVideoDecoderList.clear();

        for (const auto& [_sinkId, sink, videoSinkControlListener, videoSinkEventListener] : mVideoSinkList)
        {
            /* Un-register the events */
            VIDEOSINK_UNREGISTER_EVENT_SUCCESS(sink, videoSinkEventListener);
        }
        mVideoSinkList.clear();

        // TODO: need to clear capabilities and control listener
        //TODO: check right place to call joinThreadPool
        //android::IPCThreadState::self()->joinThreadPool();
    }
};

/**
 * @brief Test the ability to render video first frames
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 001@n
 *
 * **Test Procedure:**
 * Refer to UT specification documentation [audioDecoder_High-Level_TestSpecification.md](../../docs/pages/videoSink_High-Level_TestSpecification.md)
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Video_FirstFrame_NonTunnel)
{
    const char *testName = "Render_Video_FirstFrame_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
 /** @} */ // End of VIDEO_SINK_HALTEST L3 File
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK Module
/** @} */ // End of HPK

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
 * @addtogroup VIDEO_SINK Video Sink Module
 * @{
 */

/**
 * @addtogroup VIDEO_SINK_HALTEST Video Sink HAL Tests
 * @{
 */

/**
 * @defgroup VIDEO_SINK_HALTEST_HEADER Video Sink HAL Tests helper functions Header File
 * @{
 * @parblock
 *
 * ### Common functions/macros required for the audio module :
 *
 * Common functions/macros required for the audio module .
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_videosink.h
 *
 */

#ifndef __VTS_VIDEOSINK_H__
#define __VTS_VIDEOSINK_H__

#include <thread>
#include <chrono>
#include <ut.h>

#include "com/rdk/hal/videosink/IVideoSinkManager.h"
#include "com/rdk/hal/videosink/BnVideoSinkControllerListener.h"
#include "com/rdk/hal/videosink/BnVideoSinkEventListener.h"

#include "com/rdk/hal/avbuffer/IAVBuffer.h"
#include "com/rdk/hal/avbuffer/BnAVBufferSpaceListener.h"

using android::sp;
using android::String16;
using android::defaultServiceManager;
using android::binder::Status;

using namespace com::rdk::hal;
using namespace com::rdk::hal::videosink;
using namespace com::rdk::hal::videodecoder;
using namespace com::rdk::hal::avbuffer;

extern sp<IVideoSinkManager> gVideoSinkManager;
extern void *gTestConfiguration;

/* Status::Exception */
const static ut_control_keyStringMapping_t statusException_mapTable[] =
{
    {"EX_NONE",                  (int32_t) Status::EX_NONE},
    {"EX_SECURITY",              (int32_t) Status::EX_SECURITY},
    {"EX_BAD_PARCELABLE",        (int32_t) Status::EX_BAD_PARCELABLE},
    {"EX_ILLEGAL_ARGUMENT",      (int32_t) Status::EX_ILLEGAL_ARGUMENT},
    {"EX_NULL_POINTER",          (int32_t) Status::EX_NULL_POINTER},
    {"EX_ILLEGAL_STATE",         (int32_t) Status::EX_ILLEGAL_STATE},
    {"EX_NETWORK_MAIN_THREAD",   (int32_t) Status::EX_NETWORK_MAIN_THREAD},
    {"EX_UNSUPPORTED_OPERATION", (int32_t) Status::EX_UNSUPPORTED_OPERATION},
    {"EX_SERVICE_SPECIFIC",      (int32_t) Status::EX_SERVICE_SPECIFIC},
    {"EX_PARCELABLE",            (int32_t) Status::EX_PARCELABLE},
    {"EX_HAS_REPLY_HEADER",      (int32_t) Status::EX_HAS_REPLY_HEADER},
    {"EX_TRANSACTION_FAILED",    (int32_t) Status::EX_TRANSACTION_FAILED},
    {NULL, -1}
};

/* bool */
const static ut_control_keyStringMapping_t bool_mapTable[] =
{
    {"false", (int32_t) false},
    {"true",  (int32_t) true},
    {NULL, -1}
};

#define MAX_TRANSITORY_STATE_WAIT_MS  5
#define MAX_FINAL_STATE_WAIT_MS  500


#define VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getVideoSinkIds(OUT:sinkIds:[])"); \
        status = gVideoSinkManager->getVideoSinkIds(&sinkIds); \
        UT_LOG_INFO("Result getVideoSinkIds(OUT:sinkIds:[size: %d]), status:[isOK:%s, exception:%s]", \
                                sinkIds.size(), UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_FALSE(sinkIds.empty()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)
#define VIDEOSINK_MANAGER_GETVIDEOSINKIDS_FAILURE(sinkIds,exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getVideoSinkIds(OUT:sinkIds:[])"); \
        status = gVideoSinkManager->getVideoSinkIds(sinkIds); \
        UT_LOG_INFO("Result getVideoSinkIds(OUT: status:[isOK:%s, exception:%s]", \
                                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getVideoSink(IN:_sinkId:[%d], OUT:videoSink[])", _sinkId.value); \
        status = gVideoSinkManager->getVideoSink(_sinkId, &videoSink); \
        UT_LOG_INFO("Result getVideoSink(IN:_sinkId:[%d], OUT:videoSink[0x%X]), status:[isOK:%s, exception:%s]", \
                                _sinkId.value, videoSink.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_TRUE(videoSink.get() != nullptr); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_MANAGER_GETVIDEOSINK_FAILURE(_sinkId, videoSink) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getVideoSink(IN:_sinkId:[%d], OUT:videoSink[])", _sinkId.value); \
        status = gVideoSinkManager->getVideoSink(_sinkId, &videoSink); \
        UT_LOG_INFO("Result getVideoSink(IN:_sinkId:[%d], OUT:videoSink[0x%X]), status:[isOK:%s, exception:%s]", \
                                _sinkId.value, videoSink.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_TRUE(videoSink.get() == nullptr); \
    } while (0)

#define VIDEOSINK_CONTROLLER_SETVIDEODECODER_SUCCESS(videoSinkController,videodecoderID) \
    do { \
        ::android::binder::Status status; \
        bool aidl_return = false;\
        UT_LOG_INFO("Calling setVideosink(IN:videodecoderId:[%d], OUT:aidl_return)", videodecoderId.value);\
        status = videoSinkController->setVideoDecoder(videodecoderId, &aidl_return);\
        UT_LOG_INFO("Result setVideosink(IN:videosinkId:[%d], OUT:aidl_return:[%s]), status:[isOK:%s, exception:%s]",\
                    videodecoderId.value, aidl_return ? "true" : "false",\
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),\
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));\
        UT_ASSERT_TRUE(status.isOk());\
        UT_ASSERT_TRUE(aidl_return);\
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_CONTROLLER_SETVIDEODECODER_FAILURE(videoSinkController,videodecoderID,exception) \
    do { \
        ::android::binder::Status status; \
        bool aidl_return = false;\
        UT_LOG_INFO("Calling setVideosink(IN:videodecoderId:[%d], OUT:_aidl_return)", videodecoderID.value);\
        status = videoSinkController->setVideoDecoder(videodecoderID, &aidl_return);\
        UT_LOG_INFO("Result setVideosink(IN:videodecoderId:[%d], OUT:_aidl_return:[%d]), status:[isOK:%s, exception:%s]",\
                    videodecoderID.value, aidl_return, UT_Control_GetMapString(bool_mapTable, status.isOk()),\
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));\
        UT_ASSERT_FALSE(aidl_return);\
        UT_ASSERT_FALSE(status.isOk());\
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_CONTROLLER_GETVIDEODECODER_SUCCESS(videoSinkController,videodecoderID) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getVideoDecoder(OUT:videoDecoderId:[%d], OUT:aidl_return)", videodecoderId.value);\
        status = videoSinkController->getVideoDecoder(&videodecoderId);\
        UT_LOG_INFO("Result getVideosink(OUT:videodecoderId:[%d]), status:[isOK:%s, exception:%s]",\
                    videodecoderId.value,\
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),\
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));\
        UT_ASSERT_TRUE(status.isOk());\
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_CONTROLLER_GETVIDEODECODER_FAILURE(videoSinkController,videodecoderID,exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getVideosink(OUT:videodecoderId)");\
        status = videoSinkController->getVideoDecoder(videodecoderID);\
        UT_LOG_INFO("Result getVideosink(out:videodecoderId:[]), status:[isOK:%s, exception:%s]",\
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),\
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));\
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_GETCAPABILITIES(videoSink, capabilities) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getCapabilities(OUT:capabilities)"); \
            videoSink->getCapabilities(capabilities); \
        UT_LOG_INFO("Result getCapabilities(OUT:capabilities), status:[isOK:%s, exception:%s]", \
                                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_GETCAPABILITIES_FAILURE(videoSink, capabilities,exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getCapabilities(OUT:capabilities)"); \
            videoSink->getCapabilities(capabilities); \
        UT_LOG_INFO("Result getCapabilities(OUT:capabilities), status:[isOK:%s, exception:%s]", \
                                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)
#define VIDEOSINK_GET_PROPERTY_SUCCESS(videoSink, property, propertyValue) \
    do { \
        ::android::binder::Status status; \
        ::com::rdk::hal::State state; \
        UT_LOG_INFO("Calling getProperty(IN:property[%s], OUT:propertyValue[]", videosink::toString(property).c_str()); \
        status = (videoSink)->getProperty(property, &propertyValue); \
        switch (propertyValue->value.value().getTag()) \
        { \
            case PropertyValue::Value::Tag::intValue: \
                UT_LOG_INFO("Result getProperty(IN:property[%s], OUT:propertyValue[%d]), status:[isOK:%s, exception:%s]", videosink::toString(property).c_str(), \
                propertyValue->value->get<PropertyValue::Value::Tag::intValue>(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
                break; \
            case PropertyValue::Value::Tag::stringValue: \
                UT_LOG_INFO("Result getProperty(IN:property[%s], OUT:propertyValue[%s]), status:[isOK:%s, exception:%s]", videosink::toString(property).c_str(), \
                propertyValue->value->get<PropertyValue::Value::Tag::stringValue>().string(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
                break; \
        } \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_GET_PROPERTY_FAILURE(videoSink, property, propertyValue, exception) \
    do { \
        ::android::binder::Status status; \
        ::com::rdk::hal::State state; \
        UT_LOG_INFO("Calling getProperty(IN:property[%s], OUT:propertyValue[]", videosink::toString(property).c_str()); \
        status = (videoSink)->getProperty(property, &propertyValue); \
        UT_LOG_INFO("Result getProperty(IN:property[%s], OUT:propertyValue[]), status:[isOK:%s, exception:%s]", videosink::toString(property).c_str(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_GET_STATE(videoSink, state) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getState(OUT:state[]"); \
        status = (videoSink)->getState(&state); \
        UT_LOG_INFO("Result getState(OUT:state[%s]), status:[isOK:%s, exception:%s]", com::rdk::hal::toString(state).c_str(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_GET_STATE_FAILURE(videoSink, state, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getState(OUT:state[]"); \
        status = (videoSink)->getState(state); \
        UT_LOG_INFO("Result getState(OUT:state[]), status:[isOK:%s, exception:%s]", \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_WAIT_FOR_STATE(videoSink, desiredState, lastState) \
    do { \
        int32_t sleep_time = 0; \
        do \
        { \
            std::this_thread::sleep_for(std::chrono::milliseconds(MAX_TRANSITORY_STATE_WAIT_MS)); \
            sleep_time += MAX_TRANSITORY_STATE_WAIT_MS; \
            /* Check the state of the sink */ \
            VIDEOSINK_GET_STATE((videoSink), lastState); \
        } while (sleep_time < MAX_FINAL_STATE_WAIT_MS && lastState != desiredState); \
    } while (0)

#define VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_INFO("Calling open(IN:controllerListener[0x%p], OUT:videoSinkController[])", \
            controllerListener); \
        status = (videoSink)->open(controllerListener,&videoSinkController); \
        UT_LOG_INFO("Result open(IN:controllerListener[0x%p], OUT:videoSinkController[0x%X]), status:[exception:%s]", \
            controllerListener, videoSinkController.get(), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_NOT_EQUAL(videoSinkController, nullptr); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        \
        if (stateCheck) \
        { \
            ::com::rdk::hal::State lastState; \
            VIDEOSINK_WAIT_FOR_STATE(videoSink, State::READY, lastState); \
            UT_ASSERT_EQUAL(lastState, State::READY); \
        } \
    } while (0)

#define VIDEOSINK_OPEN_FAILURE(videoSink, controllerListener, videoSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling open(IN:controllerListener[0x%p])", \
            controllerListener); \
        status = videoSink->open(controllerListener, &videoSinkController); \
        UT_LOG_INFO("Result open(IN:controllerListener[0x%p], OUT:videoSinkController[0x%X]), status:[exception:%s]", \
            controllerListener, videoSinkController.get(), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_EQUAL(videoSinkController.get(), nullptr); \
    } while (0);

#define VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_INFO("Calling close(IN:videoSinkController[0x%p])", videoSinkController.get()); \
        status = videoSink->close(videoSinkController,&_aidl_return); \
        UT_LOG_INFO("Result close(IN:videoSinkController[0x%p]), status:[isOK:%s, exception:%s]", \
            videoSinkController.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_TRUE(_aidl_return); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) \
        { \
            ::com::rdk::hal::State lastState; \
            VIDEOSINK_WAIT_FOR_STATE(videoSink, State::CLOSED, lastState); \
            UT_ASSERT_EQUAL(lastState, State::CLOSED); \
        } \
    } while (0)

#define VIDEOSINK_CLOSE_FAILURE(videoSink, videoSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_INFO("Calling close(IN:videoSinkController[0x%p], OUT:_aidl_return[])", videoSinkController.get()); \
        status = videoSink->close(videoSinkController, &_aidl_return); \
        UT_LOG_INFO("Result close(IN:videoSinkController[0x%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            videoSinkController.get(), UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(_aidl_return); \
    } while (0)

#define VIDEOSINK_REGISTER_EVENT_SUCCESS(videoSink, eventListener) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_INFO("Calling registerEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = videoSink->registerEventListener(eventListener, &_aidl_return); \
        UT_LOG_INFO("Result registerEventListener(IN:eventListener[0x%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            eventListener.get(), UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_TRUE(_aidl_return); \
    } while (0)

#define VIDEOSINK_REGISTER_EVENT_FAILURE(videoSink, eventListener, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return=false; \
        UT_LOG_INFO("Calling registerEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = videoSink->registerEventListener(eventListener, &_aidl_return); \
        UT_LOG_INFO("Result registerEventListener(IN:eventListener[0x%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            eventListener.get(), UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        if (exception == Status::EX_NONE) \
            UT_ASSERT_TRUE(status.isOk()); \
        else \
            UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(_aidl_return); \
    } while (0)

#define VIDEOSINK_UNREGISTER_EVENT_SUCCESS(videoSink, eventListener) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_INFO("Calling unregisterEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = videoSink->unregisterEventListener(eventListener, &_aidl_return); \
        UT_LOG_INFO("Result unregisterEventListener(IN:eventListener[0x%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            eventListener.get(), UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_TRUE(_aidl_return); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_UNREGISTER_EVENT_FAILURE(videoSink, eventListener, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_INFO("Calling unregisterEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = videoSink->unregisterEventListener(eventListener, &_aidl_return); \
        UT_LOG_INFO("Result unregisterEventListener(IN:eventListener[0x%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            eventListener.get(), UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(_aidl_return); \
    } while (0)

#define VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling start()"); \
        status = videoSinkController->start(); \
        UT_LOG_INFO("Result start(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) \
        { \
            ::com::rdk::hal::State lastState; \
            VIDEOSINK_WAIT_FOR_STATE(videoSink, State::STARTED, lastState); \
            UT_ASSERT_EQUAL(lastState, State::STARTED); \
        } \
    } while (0)

#define VIDEOSINK_START_FAILURE(videoSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling start()"); \
        status = videoSinkController->start(); \
        UT_LOG_INFO("Result start(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling stop()"); \
        status = videoSinkController->stop(); \
        UT_LOG_INFO("Result stop(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) \
        { \
            ::com::rdk::hal::State lastState; \
            VIDEOSINK_WAIT_FOR_STATE(videoSink, State::READY, lastState); \
            UT_ASSERT_EQUAL(lastState, State::READY); \
        } \
    } while (0)

#define VIDEOSINK_STOP_FAILURE(videoSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling stop()"); \
        status = videoSinkController->stop(); \
        UT_LOG_INFO("Result stop(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define VIDEOSINK_FLUSH_SUCCESS(videoSink, videoSinkController, holdLastFrame) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling flush(IN: holdLastFrame[%s])", UT_Control_GetMapString(bool_mapTable, holdLastFrame)); \
        status = videoSinkController->flush(holdLastFrame); \
        UT_LOG_INFO("Result flush(IN: holdLastFrame[%s]), status:[isOK:%s, exception:%s]", UT_Control_GetMapString(bool_mapTable, holdLastFrame), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_FLUSH_FAILURE(videoSinkController, holdLastFrame, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling flush(IN: holdLastFrame[%s])", UT_Control_GetMapString(bool_mapTable, holdLastFrame)); \
        status = videoSinkController->flush(holdLastFrame); \
        UT_LOG_INFO("Result flush(IN: holdLastFrame[%s]), status:[isOK:%s, exception:%s]", UT_Control_GetMapString(bool_mapTable, holdLastFrame), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define VIDEOSINK_SIGNAL_DISCARDFRAMESTILL_SUCCESS(videoSinkController, nsPresentationTime) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling discardFramesUntil(IN:nsPresentationTime:[%lld])", nsPresentationTime);\
        status = videoSinkController->discardFramesUntil(nsPresentationTime);\
        UT_LOG_INFO("Result discardFramesUntil(IN:nsPresentationTime:[%lld]), status:[isOK:%s, exception:%s]",\
                    nsPresentationTime, UT_Control_GetMapString(bool_mapTable, status.isOk()),\
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));\
        UT_ASSERT_TRUE(status.isOk());\
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_SIGNAL_DISCARDFRAMESTILL_FAILURE(videoSinkController, nsPresentationTime, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling discardFramesUntil(IN:nsPresentationTime:[%lld])", nsPresentationTime); \
        status = videoSinkController->discardFramesUntil(nsPresentationTime); \
        UT_LOG_INFO("Result discardFramesUntil(IN:nsPresentationTime:[%lld]), status:[isOK:%s, exception:%s]", \
                    nsPresentationTime, UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_FAILURE(videoSinkController, nsPresentationTime, frameBufferHandle, metadata, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return=false; \
        UT_LOG_INFO("Calling queueVideoFrame(IN:nsPresentationTime:[%lld], IN:frameBufferHandle:[%lld], IN:metadata:[...])", nsPresentationTime, frameBufferHandle); \
        status = videoSinkController->queueVideoFrame(nsPresentationTime, frameBufferHandle, metadata, &_aidl_return); \
        UT_LOG_INFO("Result queueVideoFrame(IN:nsPresentationTime:[%lld], IN:frameBufferHandle:[%lld], IN:metadata:[...]), status:[isOK:%s, exception:%s]", \
                    nsPresentationTime, frameBufferHandle,\
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(_aidl_return); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_SUCCESS(videoSinkController, nsPresentationTime, frameBufferHandle, metadata) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return=false; \
        UT_LOG_INFO("Calling queueVideoFrame(IN:nsPresentationTime:[%lld], IN:frameBufferHandle:[%lld], IN:metadata:[...])", nsPresentationTime, frameBufferHandle); \
        status = videoSinkController->queueVideoFrame(nsPresentationTime, frameBufferHandle, metadata, &_aidl_return); \
        UT_LOG_INFO("Result queueVideoFrame(IN:nsPresentationTime:[%lld], IN:frameBufferHandle:[%lld], IN:metadata:[...]), status:[isOK:%s, exception:%s]", \
                    nsPresentationTime, frameBufferHandle,\
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_TRUE(_aidl_return); \
        UT_ASSERT_TRUE(status.isOk()); \
    } while (0)

#define VIDEOSINK_SETPROPERTY_SUCCESS(videoSinkController, property, propertyValue) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return=false; \
        UT_LOG_INFO("Calling setProperty(IN:property[%s], IN:propertyValue[0x%X], OUT:_aidl_return[])", com::rdk::hal::videosink::toString(property).c_str(), propertyValue.value); \
        status = videoSinkController->setProperty(property, propertyValue, &_aidl_return); \
        UT_LOG_INFO("Result setProperty(IN:property[%s], IN:propertyValue[0x%X], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", com::rdk::hal::videosink::toString(property).c_str(), propertyValue.value, \
            UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_TRUE(_aidl_return); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_SETPROPERTY_FAILURE(videoSinkController, property, propertyValue, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_INFO("Calling setProperty(IN:property[%s], IN:propertyValue[0x%X], OUT:_aidl_return[])", com::rdk::hal::videosink::toString(property).c_str(), propertyValue.value); \
        status = videoSinkController->setProperty(property, propertyValue, &_aidl_return); \
        UT_LOG_INFO("Result setProperty(IN:property[%s], IN:propertyValue[0x%X], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", com::rdk::hal::videosink::toString(property).c_str(), propertyValue.value, \
            UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(_aidl_return); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define VIDEOSINK_SETPROPERTY_LIST_SUCCESS(videoSinkController, properyList, type) \
    do { \
        for (const auto& [property, value] : properyList) \
        { \
            PropertyValue propertyValue; \
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::type>(value); \
            VIDEOSINK_SETPROPERTY_SUCCESS(videoSinkController, property, propertyValue); \
        } \
    } while (0)

#define VIDEOSINK_SETPROPERTY_STRING_LIST_SUCCESS(videoSinkController, properyList) \
    do { \
        for (const auto& [property, value] : properyList) \
        { \
            PropertyValue propertyValue; \
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::stringValue>(value); \
            VIDEOSINK_SETPROPERTY_SUCCESS(videoSinkController, property, propertyValue); \
        } \
    } while (0)

#define VIDEOSINK_SETPROPERTY_LIST_FAILURE(videoSinkController, properyList, type, exception) \
    do { \
        for (const auto& [property, value] : properyList) \
        { \
            PropertyValue propertyValue; \
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::type>(value); \
            VIDEOSINK_SETPROPERTY_FAILURE(videoSinkController, property, propertyValue, exception); \
        } \
    } while (0)

#define VIDEOSINK_SETPROPERTY_STRING_LIST_FAILURE(videoSinkController, properyList, exception) \
    do { \
        for (const auto& [property, value] : properyList) \
        { \
            PropertyValue propertyValue; \
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::stringValue>(value); \
            VIDEOSINK_SETPROPERTY_FAILURE(videoSinkController, property, propertyValue, exception); \
        } \
    } while (0)

#define VIDEOSINK_QUEUEVIDEOFRAME_SUCCESS(videoSinkController, nsPresentationTime, frameBufferHandle, metadata, expectedReturn) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_INFO("Calling queueVideoFrame(IN:nsPresentationTime[%ld], IN:frameBufferHandle[0x%lX], IN:metadata{width:%d, height:%d, format:%d}, OUT:_aidl_return[])", \
            nsPresentationTime, frameBufferHandle, metadata.width, metadata.height, metadata.format); \
        status = videoSinkController->queueVideoFrame(nsPresentationTime, frameBufferHandle, metadata, &_aidl_return); \
        UT_LOG_INFO("Result queueVideoFrame(..., OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_EQUAL(_aidl_return, expectedReturn); \
    } while (0)

struct VideoSinkData
{

};

struct DecodedVideoFrame
{

};

class VideoSinkControllerListener : public BnVideoSinkControllerListener
{
    public:
        VideoSinkControllerListener() = default;
        virtual ~VideoSinkControllerListener() = default;

        android::binder::Status onFirstFrameRendered(int64_t nsPresentationTime) override
        {
            UT_LOG_INFO("\n=======onFirstFrameRendered: %d, %d=============\n");
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

        // Reset first-frame rendered flag and PTS for reuse
        void resetFirstFrameRendered()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mFirstFrameRendered = false;
            mFirstPTS = -1;
        }

        // Check if first frame has been rendered
        bool hasFirstFrameRendered()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mFirstFrameRendered;
        }

        // Get the PTS of the first rendered frame
        int64_t getFirstFrameRenderedPTS()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mFirstPTS;
        }
        void resetFlushComplete()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mFlushComplete = false;
        }

        bool hasFlushComplete()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mFlushComplete;
        }

        android::binder::Status onFlushComplete() override
        {
            UT_LOG_INFO("\n=======onFlushComplete: %d, %d=============\n");
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mFlushComplete = true;
            }
            mCV.notify_all();
            return android::binder::Status::ok();
        }

        android::binder::Status onEndOfStream(int64_t nsPresentationTime) override
        {
            UT_LOG_INFO("\n=======onEndOfStream: %d, %d=============\n");
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mEosReceived = true;
                mEosPTS = nsPresentationTime;
            }
            mCV.notify_all();
            return android::binder::Status::ok();
        }

        bool hasEosReceived()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mEosReceived;
        }

        int64_t getEosPTS()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mEosPTS;
        }

        void resetEos()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mEosReceived = false;
            mEosPTS = -1;
        }

        android::binder::Status onVideoUnderflow() override {
            UT_LOG_INFO("\n=======onVideoUnderflow: %d, %d=============\n");
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mUnderflowReceived = true;
            }
            mCV.notify_all();
            return android::binder::Status::ok();
        }

        bool hasUnderflowReceived() {
            std::lock_guard<std::mutex> lock(mMutex);
            return mUnderflowReceived;
        }

        void resetUnderflow() {
            std::lock_guard<std::mutex> lock(mMutex);
            mUnderflowReceived = false;
        }

        android::binder::Status onVideoResumed() override {
             UT_LOG_INFO("\n=======onVideoUnderflow: %d, %d=============\n");
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mResumedReceived = true;
            }
            mCV.notify_all();
            return android::binder::Status::ok();
        }

        bool hasResumedReceived() {
            std::lock_guard<std::mutex> lock(mMutex);
            return mResumedReceived;
        }

        void resetResumed() {
            std::lock_guard<std::mutex> lock(mMutex);
            mResumedReceived = false;
        }
        android::binder::Status onStateChanged(State /*oldState*/, State /*newState*/) override { return android::binder::Status::ok(); }

        bool waitForFlushComplete(std::chrono::seconds timeout)
        {
            std::unique_lock<std::mutex> lock(mMutex);
            return mCV.wait_for(lock, std::chrono::seconds(timeout), [this] {
                return mFlushComplete;
            });
        }

        bool waitForFlushCompleteEvent(std::chrono::seconds timeout)
        {
            std::unique_lock<std::mutex> lock(mMutex);
            return mCV.wait_for(lock, timeout, [this]() { return mFlushComplete; });
        }

        bool waitForEosEvent(int timeoutSeconds)
        {
            std::unique_lock<std::mutex> lock(mMutex);
            return mCV.wait_for(lock, std::chrono::seconds(timeoutSeconds), [this]
            {
                return mEosReceived;
            });
        }

        bool waitForUnderflowEvent(int timeoutSeconds) {
            std::unique_lock<std::mutex> lock(mMutex);
            return mCV.wait_for(lock, std::chrono::seconds(timeoutSeconds), [this] {
                return mUnderflowReceived;
            });
        }

        bool waitForResumedEvent(int timeoutSeconds) {
            std::unique_lock<std::mutex> lock(mMutex);
            return mCV.wait_for(lock, std::chrono::seconds(timeoutSeconds), [this] {
            return mResumedReceived;
            });
        }

    private:
        mutable std::mutex mMutex;
        std::condition_variable mCV;
        bool mFirstFrameRendered = false;
        int64_t mFirstPTS = -1;
        bool mFlushComplete = false;
        bool mEosReceived = false;
        int64_t mEosPTS = -1;
        bool mUnderflowReceived = false;
        bool mResumedReceived = false;
    };


class VideoSinkEventListener : public BnVideoSinkEventListener
{
public:
    VideoSinkEventListener()
        : mCurrentState(State::CLOSED),
          mFlushComplete(false) {}

    virtual ~VideoSinkEventListener() = default;

    Status onFirstFrameRendered(int64_t /*nsPresentationTime*/) override {
        UT_LOG_INFO("\n=======onFirstFrameRendered: %d, %d=============\n");
        return ::android::binder::Status::ok();
    }

    Status onEndOfStream(int64_t /*nsPresentationTime*/) override {
        UT_LOG_INFO("\n=======onEndOfStream: %d, %d=============\n");
        return ::android::binder::Status::ok();
    }

    Status onVideoUnderflow() override {
        UT_LOG_INFO("\n=======onVideoUnderflow: %d, %d=============\n");
        return ::android::binder::Status::ok();
    }

    Status onVideoResumed() override {
        UT_LOG_INFO("\n=======onVideoResumed: %d, %d=============\n");
        return ::android::binder::Status::ok();
    }

    Status onFlushComplete() override {
        UT_LOG_INFO("\n=======onFlushComplete: %d, %d=============\n");
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mFlushComplete = true;
        }
        mCV.notify_all();
        return ::android::binder::Status::ok();
    }

    Status onStateChanged(State oldState, State newState) override {
        UT_LOG("\n=======State Change received: %d, %d=============\n", oldState, newState);
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStateChanges.emplace_back(oldState, newState);
            mCurrentState = newState;
        }
        mCV.notify_all(); // Unblock any waiting test code
        return Status::ok();
    }

    // ================= Flush helpers =================

    void resetFlushComplete() {
        std::lock_guard<std::mutex> lock(mMutex);
        mFlushComplete = false;
    }

    bool hasFlushComplete() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mFlushComplete;
    }

    bool waitForFlushComplete(std::chrono::seconds timeout) {
        std::unique_lock<std::mutex> lock(mMutex);
        return mCV.wait_for(lock, timeout, [this]() { return mFlushComplete; });
    }

    // ================= State transition helpers =================

    bool waitForState(State desiredState, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mMutex);
        return mCV.wait_for(lock, timeout, [this, desiredState]() {
            return (mCurrentState == desiredState);
        });
    }

    std::vector<std::pair<State, State>> getStateChanges() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return mStateChanges;
    }

    void resetStateChanges() {
        std::lock_guard<std::mutex> lock(mMutex);
        mStateChanges.clear();
    }

    bool checkStateTransistion(State expOldState, State expNewState) {
        std::lock_guard<std::mutex> lock(mMutex);
        for (const auto &[oldState, newState] : mStateChanges) {
            if (oldState == expOldState && newState == expNewState) {
                return true;
            }
        }
        return false;
    }

    std::vector<int32_t> getErrors() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return mErrors;
    }

private:
    mutable std::mutex mMutex;
    std::condition_variable mCV;

    State mCurrentState;
    std::vector<std::pair<State, State>> mStateChanges;
    std::vector<int32_t> mErrors;

    // Flush tracking
    bool mFlushComplete;
};


#endif //__VTS_VIDEOSINK_H__

/** @} */ // End of VIDEO_SINK_HALTEST_HEADER
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK
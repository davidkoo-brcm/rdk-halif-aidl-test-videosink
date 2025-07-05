#ifndef VTS_L3_VIDEO_DECODER_H
#define VTS_L3_VIDEO_DECODER_H

#include "vts_l3_common.h"
#include <mutex>
#include <condition_variable>
#include <vector>
#include <optional>

#include "com/rdk/hal/videodecoder/IVideoDecoderManager.h"
#include "com/rdk/hal/videodecoder/BnVideoDecoderControllerListener.h"
#include "com/rdk/hal/videodecoder/BnVideoDecoderEventListener.h"
#include "com/rdk/hal/videodecoder/DynamicRange.h"
#include "com/rdk/hal/videodecoder/FrameMetadata.h"
#include "com/rdk/hal/videodecoder/Codec.h"
#include "com/rdk/hal/videodecoder/CodecCapabilities.h"
#include "com/rdk/hal/videodecoder/OperationalMode.h"
#include "com/rdk/hal/videodecoder/ErrorCode.h"
#include "com/rdk/hal/State.h"

#include "com/rdk/hal/videodecoder/IVideoDecoder.h"
#include "com/rdk/hal/avbuffer/IAVBuffer.h"
#include "IAVBufferHelper.h"

using namespace com::rdk::hal;
using namespace com::rdk::hal::videodecoder;
using com::rdk::hal::videodecoder::IVideoDecoder;
using com::rdk::hal::videodecoder::IVideoDecoderControllerListener;
using com::rdk::hal::videodecoder::IVideoDecoderEventListener;
using com::rdk::hal::avbuffer::IAVBuffer;
using android::sp;

extern android::sp<com::rdk::hal::videodecoder::IVideoDecoderManager> gVideoDecoderManager;

UT_status_t getVideoDecoderService();

#define MAX_TRANSITORY_STATE_WAIT_MS  5
#define MAX_FINAL_STATE_WAIT_MS  500

typedef struct InputVideoFrame
{
    int64_t framePTS;
    std::string reference;
    double rms_threshold;
    bool isErrorFrame;
    std::chrono::steady_clock::time_point sendTime;
}InputVideoFrame_t;

struct testConfiguration_t {
    std::string input;
    std::string streamName;
    videodecoder::Codec codec;
    int32_t codedWidth;
    int32_t codedHeight;
    uint32_t frameRateNumerator;
    uint32_t frameRateDenominator;
    videodecoder::PixelFormat pixelFormat;
    videodecoder::DynamicRange dynamicRange;
    std::string colourSpace;
    uint32_t colorDepth;
    uint32_t bitRate;
    uint32_t no_of_frames;
    std::vector<std::tuple<int32_t, int64_t, bool>> frameDetails;
    double rms_diff_threshold;
    uint32_t decoderDelay;
    int64_t stopPts;
    int64_t discontinuityPts;
    int64_t flushPts;
    int64_t flushResetPts;
    int64_t eosPts;
    int64_t decode_time_threshold;

    // Sink control fields
    int64_t sink_discardFramesUntilPts;
    int64_t sink_underflowPts;
    int64_t sink_resumeTriggerPts;
    int32_t sink_repeatPerFrame;
    int64_t sink_flush_pts;
    int64_t sink_flush_reset_pts;
    int64_t sink_eos_pts;
    int64_t sink_interFrameDelayMs;
    int64_t sink_discontinuityPTS;
    std::string sink_testName;
    bool sink_UnderflowCheck;
};


struct videoFrameMetadata {
        PixelFormat pixelFormat;
        int32_t width;
        int32_t height;
        Codec sourceCodec;
        float frameRate;
        DynamicRange dynamicRange;
        int32_t bitDepth;
        bool lowLatency;
        bool endOfStream;
        bool discontinuity;
        AVSource source;
};

struct DecodedVideoFrame_t {
    std::vector<uint8_t> data;
    int64_t framePTS;
    std::chrono::steady_clock::time_point receiveTime;
    bool isMetaAvailable;
    videoFrameMetadata metadata;
};

size_t vts_bytesPerPixel(PixelFormat format);
UT_status_t vts_videodecoder_parse_testconfig(void* pTestInstance, const char *module, const char *testGroup, const char *testName, std::vector<testConfiguration_t>& testConfiguration);
Codec videoDecoderStringToCodec(const std::string& codecStr);
DynamicRange videoDecoderStringToDynamicRange(const std::string& rangeStr);
PixelFormat videoDecoderStringToPixelFormat(const std::string& formatStr);

#define VIDEODECODER_GETSTATE(videoDecoder, state) \
    do { \
        Status status; \
        UT_LOG_DEBUG("Calling getState(OUT:_aidl_return:[state])"); \
        status = (videoDecoder)->getState(&state); \
        UT_LOG_DEBUG("Result getState(OUT:_aidl_return:[state:%d]), status:[isOK:%s, exception:%s]", \
                    static_cast<int32_t>(state), \
                    UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
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
        UT_LOG_DEBUG("Calling getVideoDecoderIds(OUT:decoderIds:[])"); \
        status = gVideoDecoderManager->getVideoDecoderIds(&decoderIds); \
        UT_LOG_DEBUG("Result getVideoDecoderIds(OUT:decoderIds:[size: %d]), status:[isOK:%s, exception:%s]", \
                                decoderIds.size(), UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_FALSE_FATAL(decoderIds.empty()); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEODECODER_MANAGER_GETVIDEODECODER(_decoderId, videoDecoder) \
    do { \
        Status status; \
        UT_LOG_DEBUG("Calling getVideoDecoder(IN:_decoderId:[%d], OUT:videoDecoder[])", _decoderId.value); \
        status = gVideoDecoderManager->getVideoDecoder(_decoderId, &videoDecoder); \
        UT_LOG_DEBUG("Result getVideoDecoder(IN:_decoderId:[%d], OUT:videoDecoder[0x%X]), status:[isOK:%s, exception:%s]", \
                                _decoderId.value, videoDecoder.get(), UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
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
        UT_LOG_DEBUG("Calling registerEventListener(IN:videoDecoderEventListener:[0x%p], OUT:_aidl_return[])", listenerPtr); \
        status = (videoDecoder)->registerEventListener((listener), &aidl_return); \
        UT_LOG_DEBUG("Result registerEventListener(IN:videoDecoderEventListener:[0x%p], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", \
                    listenerPtr, UT_Control_GetMapString(bool_mapTable_, aidl_return), \
                    UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()) << "registerEventListener binder call failed"; \
        UT_ASSERT_TRUE(aidl_return) << "registerEventListener returned false"; \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE) << "registerEventListener returned an exception"; \
    } while (0)

#define VIDEODECODER_GETCAPABILITIES(videoDecoder, capabilities) \
     do { \
         Status status; \
         UT_LOG_DEBUG("Calling getCapabilities(OUT:capabilities)"); \
         status = videoDecoder->getCapabilities(capabilities); \
         UT_LOG_DEBUG("Result getCapabilities(OUT:capabilities), status:[isOK:%s, exception:%s]", \
                     UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                     UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
         UT_ASSERT_TRUE(status.isOk()); \
         UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
     } while (0)

#define VIDEODECODER_UNREGISTER_EVENT_SUCCESS(videoDecoder, listener) \
    do { \
        Status status; \
        bool aidl_return; \
        sp<IVideoDecoderEventListener> tempListener = (listener); \
        void* listenerPtr = tempListener ? tempListener.get() : nullptr; \
        UT_LOG_DEBUG("Calling unregisterEventListener(IN:videoDecoderEventListener:[0x%p], OUT:_aidl_return[])", listenerPtr); \
        status = (videoDecoder)->unregisterEventListener((listener), &aidl_return); \
        UT_LOG_DEBUG("Result unregisterEventListener(IN:videoDecoderEventListener:[0x%p], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", \
                  listenerPtr, UT_Control_GetMapString(bool_mapTable_, aidl_return), \
                    UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()) << "unregisterEventListener binder call failed"; \
        UT_ASSERT_TRUE(aidl_return) << "unregisterEventListener returned false"; \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE) << "unregisterEventListener returned an exception"; \
    } while (0)

#define VIDEODECODER_OPEN_SUCCESS(videoDecoder, codec, secure, listener, controller, stateCheck) \
    do { \
        Status status; \
        UT_LOG_DEBUG("Calling open(IN:codec:[%d], IN:secure:[%s], IN:listener:[0x%p], OUT:controller[])", \
                    static_cast<int32_t>(codec), UT_Control_GetMapString(bool_mapTable_, (secure)), (listener).get()); \
        status = (videoDecoder)->open((codec), (secure), (listener), &(controller)); \
        UT_LOG_DEBUG("Result open(IN:codec:[%d], IN:secure:[%s], IN:listener:[0x%p], OUT:controller:[0x%X]), status:[isOK:%s, exception:%s]", \
                    static_cast<int32_t>(codec), UT_Control_GetMapString(bool_mapTable_, (secure)), (listener).get(), (controller).get(), \
                    UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
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
        UT_LOG_DEBUG("Calling start()"); \
        status = (videoDecoderController)->start(); \
        UT_LOG_DEBUG("Result start(), status:[isOK:%s, exception:%s]", \
                    UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()) << "start binder call failed"; \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE) << "start returned an exception"; \
        if (stateCheck) { \
            State lastState; \
            VIDEODECODER_WAIT_FOR_STATE(videoDecoder, State::STARTED, lastState); \
            UT_ASSERT_EQUAL(lastState, State::STARTED) << "Decoder did not reach STARTED state"; \
        } \
    } while (0)

#define VIDEODECODER_DECODEBUFFER_SUCCESS(videoDecoderController, nsPresentationTime, bufferHandle) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_DEBUG("Calling decodeBuffer(IN:nsPresentationTime[%ld], IN:bufferHandle[0x%lX], OUT:_aidl_return[])", \
            nsPresentationTime, bufferHandle); \
        status = videoDecoderController->decodeBuffer(nsPresentationTime, bufferHandle, &_aidl_return); \
        UT_LOG_DEBUG("Result decodeBuffer(IN:nsPresentationTime[%ld], IN:bufferHandle[0x%lX], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            nsPresentationTime, bufferHandle, \
            UT_Control_GetMapString(bool_mapTable_, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEODECODER_DECODEBUFFER_FAILURE(videoDecoderController, nsPresentationTime, bufferHandle, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_DEBUG("Calling decodeBuffer(IN:nsPresentationTime[%ld], IN:bufferHandle[0x%lX], OUT:_aidl_return[])", \
            nsPresentationTime, bufferHandle); \
        status = videoDecoderController->decodeBuffer(nsPresentationTime, bufferHandle, &_aidl_return); \
        UT_LOG_DEBUG("Result decodeBuffer(IN:nsPresentationTime[%ld], IN:bufferHandle[0x%lX], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            nsPresentationTime, bufferHandle, \
            UT_Control_GetMapString(bool_mapTable_, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEODECODER_SETPROPERTY_SUCCESS(videoDecoderController, property, propertyValue) \
    do { \
        Status status; \
        bool result; \
        UT_LOG_DEBUG("Calling setProperty(IN:property[%d], IN:propertyValue=%d)", \
                    static_cast<int32_t>(property), propertyValue.value); \
        status = videoDecoderController->setProperty(property, propertyValue, &result); \
        UT_LOG_DEBUG("Result setProperty(IN:property[%d], OUT:result=%s), status:[isOK:%s, exception:%s]", \
                    static_cast<int32_t>(property), UT_Control_GetMapString(bool_mapTable_, result), \
                    UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_TRUE(result); \
    } while (0)

#define VIDEODECODER_STOP_SUCCESS(videoDecoder, videoDecoderController, stateCheck) \
    do { \
        Status status; \
        UT_LOG_DEBUG("Calling stop()"); \
        status = videoDecoderController->stop(); \
        UT_LOG_DEBUG("Result stop(), status:[isOK:%s, exception:%s]", \
                    UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) { \
            State lastState; \
            VIDEODECODER_WAIT_FOR_STATE(videoDecoder, State::READY, lastState); \
            UT_ASSERT_EQUAL(lastState, State::READY) << "Decoder did not reach READY state"; \
        } \
    } while (0)

#define VIDEODECODER_CLOSE_SUCCESS(videoDecoder, controller, stateCheck) \
    do { \
        Status status; \
        bool aidl_return; \
        sp<IVideoDecoderController> tempController = (controller); \
        void* controllerPtr = tempController ? tempController.get() : nullptr; \
        UT_LOG_DEBUG("Calling close(IN:videoDecoderController:[0x%p], OUT:_aidl_return[])", controllerPtr); \
        status = (videoDecoder)->close((controller), &aidl_return); \
        UT_LOG_DEBUG("Result close(IN:videoDecoderController:[0x%p], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", \
                    controllerPtr, UT_Control_GetMapString(bool_mapTable_, aidl_return), \
                    UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()) << "close binder call failed"; \
        UT_ASSERT_TRUE_FATAL(aidl_return) << "close returned false"; \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE) << "close returned an exception"; \
        if (stateCheck) { \
            State lastState; \
            VIDEODECODER_WAIT_FOR_STATE(videoDecoder, State::CLOSED, lastState); \
            UT_ASSERT_EQUAL(lastState, State::CLOSED) << "Decoder did not reach CLOSED state after close"; \
        } \
    } while (0)

#define VIDEODECODER_SETPROPERTY_LIST_SUCCESS(videoDecoderController, propertyList, type) \
    do { \
        for (const auto& [property, value] : propertyList) \
        { \
            PropertyValue propertyValue; \
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::type>(value); \
            VIDEODECODER_SETPROPERTY_SUCCESS(videoDecoderController, property, propertyValue); \
        } \
    } while (0)

#define VIDEODECODER_GETPROPERTY(videoDecoder, property, aidl_return) \
do { \
    Status status; \
    UT_LOG_DEBUG("Calling getProperty(IN:property:[%d], OUT:_aidl_return[]) ", static_cast<int32_t>(property)); \
    status = (videoDecoder)->getProperty((property), &(aidl_return)); \
    UT_LOG_DEBUG("Result getProperty(IN:property:[%d], OUT:_aidl_return[0x%X]), status:[isOK:%s, exception:%s]", \
                static_cast<int32_t>(property), (aidl_return).has_value(), \
                UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
    UT_ASSERT_TRUE(status.isOk()) << "getProperty failed for property " << static_cast<int32_t>(property); \
    UT_ASSERT_TRUE((aidl_return).has_value()) << "No value returned for property " << static_cast<int32_t>(property); \
} while (0)

#define VIDEODECODER_GET_PROPERTY_SUCCESS(videoDecoder, property, propertyValue) \
    do { \
        ::android::binder::Status status; \
        ::com::rdk::hal::State state; \
        UT_LOG_DEBUG("Calling getProperty(IN:property[%s], OUT:propertyValue[]", videodecoder::toString(property).c_str()); \
        status = (videoDecoder)->getProperty(property, &propertyValue); \
        switch (propertyValue->value.value().getTag()) \
        { \
            case PropertyValue::Value::Tag::intValue: \
                UT_LOG_DEBUG("Result getProperty(IN:property[%s], OUT:propertyValue[%d]), status:[isOK:%s, exception:%s]", videodecoder::toString(property).c_str(), \
                propertyValue->value->get<PropertyValue::Value::Tag::intValue>(), \
                UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
                break; \
            case PropertyValue::Value::Tag::stringValue: \
                UT_LOG_DEBUG("Result getProperty(IN:property[%s], OUT:propertyValue[%s]), status:[isOK:%s, exception:%s]", videodecoder::toString(property).c_str(), \
                propertyValue->value->get<PropertyValue::Value::Tag::stringValue>().string(), \
                UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
                break; \
        } \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEODECODER_SIGNAL_DISCONTINUITY_SUCCESS(videoDecoder, videoDecoderController, expectSuccess) \
    do { \
        Status status; \
        UT_LOG_DEBUG("Calling signalDiscontinuity()"); \
        status = (videoDecoderController)->signalDiscontinuity(); \
        UT_LOG_DEBUG("Result signalDiscontinuity(), status:[isOK:%s, exception:%s]", \
                    UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk() == (expectSuccess)) << "signalDiscontinuity " \
            << ((expectSuccess) ? "failed unexpectedly" : "succeeded unexpectedly"); \
    } while (0)

#define VIDEODECODER_FLUSH_SUCCESS(videoDecoder, videoDecoderController, reset, stateCheck) \
    do { \
        State lastState; \
        Status status = (videoDecoderController)->flush(reset); \
        UT_LOG_DEBUG("flush(%s) status=%s", reset ? "true" : "false", UT_Control_GetMapString(bool_mapTable_, status.isOk())); \
        UT_ASSERT_TRUE(status.isOk()) << "flush(" << reset << ") failed"; \
        VIDEODECODER_WAIT_FOR_STATE(videoDecoder, State::READY, lastState); \
        if (stateCheck) { \
            State lastState; \
            if (reset){ \
                VIDEODECODER_WAIT_FOR_STATE(videoDecoder, State::READY, lastState); \
                UT_ASSERT_EQUAL(lastState, State::READY) << "Decoder did not reach READY state"; \
            } \
            else { \
                VIDEODECODER_WAIT_FOR_STATE(videoDecoder, State::STARTED, lastState); \
                UT_ASSERT_EQUAL(lastState, State::STARTED) << "Decoder did not reach STARTED state"; \
            } \
        } \
    } while (0)

/**
 * @brief Tests that signalEOS succeeds in the STARTED state.
 * @param[in] videoDecoder The IVideoDecoder instance (for state checking).
 * @param[in] videoDecoderController The IVideoDecoderController instance.
 * @param[in] expectSuccess Expect the call to succeed (true for positive test).
 */
#define VIDEODECODER_SIGNAL_EOS_SUCCESS(videoDecoder, videoDecoderController, expectSuccess) \
    do { \
        Status status; \
        UT_LOG_DEBUG("Calling signalEOS()"); \
        status = (videoDecoderController)->signalEOS(); \
        UT_LOG_DEBUG("Result signalEOS(), status:[isOK:%s, exception:%s]", \
                    UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk() == (expectSuccess)) << "signalEOS " \
            << ((expectSuccess) ? "failed unexpectedly" : "succeeded unexpectedly"); \
    } while (0)

class VideoDecoderControllerListener : public com::rdk::hal::videodecoder::BnVideoDecoderControllerListener {
public:
    VideoDecoderControllerListener();
    ~VideoDecoderControllerListener() override;

    android::binder::Status onFrameOutput(int64_t nsPresentationTime,
                                          int64_t frameBufferHandle,
                                          const std::optional<com::rdk::hal::videodecoder::FrameMetadata>& metadata) override;

    android::binder::Status onUserDataOutput(int64_t nsPresentationTime,
                                             const std::vector<uint8_t>& userData) override;
};

class VideoDecoderEventListener : public com::rdk::hal::videodecoder::BnVideoDecoderEventListener {
public:
    VideoDecoderEventListener();
    ~VideoDecoderEventListener() override;

    android::binder::Status onDecodeError(com::rdk::hal::videodecoder::ErrorCode errorCode,
                                          int32_t vendorErrorCode) override;

    // Corrected namespace for State here
    android::binder::Status onStateChanged(com::rdk::hal::State oldState,
                                           com::rdk::hal::State newState) override;

    bool waitForState(com::rdk::hal::State desiredState, std::chrono::milliseconds timeout);
    std::vector<std::pair<com::rdk::hal::State, com::rdk::hal::State>> getStateChanges() const;
    void resetStateChanges();
    bool checkStateTransistion(com::rdk::hal::State expOldState, com::rdk::hal::State expNewState);
    std::vector<int32_t> getErrors() const;

private:
    mutable std::mutex mMutex;
    std::condition_variable mCV;

    // Use correct State namespace here as well
    com::rdk::hal::State mCurrentState;
    std::vector<std::pair<com::rdk::hal::State, com::rdk::hal::State>> mStateChanges;
    std::vector<int32_t> mErrors;
};

class DecoderTest
{
public:
    DecoderTest(IVideoDecoder::Id decoderId,
                android::sp<IVideoDecoder> decoderHandle,
                android::sp<VideoDecoderControllerListener> controlListener,
                android::sp<VideoDecoderEventListener> eventListener,
                android::sp<IAVBuffer> AVBufferManager,
                bool secureDecode,
                bool tunnelMode,
                testConfiguration_t& config);

    void decodeAndConsumeVideoStreams();
    void decoderThreadFunc();
    std::vector<std::tuple<uint64_t, uint32_t, int64_t>> parseFrames(const std::vector<uint8_t>& inputBuffer, uint64_t fileSize, com::rdk::hal::videodecoder::Codec codec);
    bool getDecoderMetrics(int32_t &framesDecoded, int32_t &decodeErrors, int32_t &framesDropped);
    void run(bool concurrentDecodeConsume);

private:
    std::thread mDecoderThread, mConsumerThread;
    IVideoDecoder::Id mDecoderId;
    android::sp<IVideoDecoder> mDecoderHandle;
    android::sp<VideoDecoderControllerListener> mControlListener;
    android::sp<VideoDecoderEventListener> mEventListener;
    android::sp<IAVBuffer> mAVBufferManager;
    bool mSecureDecode;
    bool mTunnelMode;
    testConfiguration_t mTestConfiguration;
    std::deque<InputVideoFrame_t> mInputFrame;
    std::mutex mDecodeMutex;
    std::atomic<bool> mDecodingStream;
    bool mConcurrentDecodeConsume;
    std::mutex mAVbufferHelperLock;
};

#endif // VTS_L3_VIDEO_DECODER_H

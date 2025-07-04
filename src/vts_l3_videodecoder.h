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
    std::string input; // Path to input .es file
    std::string streamName; //
    videodecoder::Codec codec; // H264_AVC, HEVC, etc.
    int32_t codedWidth; // Video width
    int32_t codedHeight; // Video height
    uint32_t frameRateNumerator; // Frame rate numerator
    uint32_t frameRateDenominator; // Frame rate denominator
    videodecoder::PixelFormat pixelFormat; // YUV, etc.
    videodecoder::DynamicRange dynamicRange; // SDR, HDR, etc.
    std::string colourSpace; // BT709, etc.
    uint32_t colorDepth; // Bit depth
    uint32_t bitRate; // Bit rate
    uint32_t no_of_frames; // Total frames
    std::vector<std::tuple<int32_t, int64_t, bool>> frameDetails; // {size, PTS, isErrorFrame}
    double rms_diff_threshold; // RMS difference threshold
    uint32_t decoderDelay; // Delay between decode calls (ms)
    int64_t stopPts; // Stop/restart PTS
    int64_t discontinuityPts; // Discontinuity PTS
    int64_t flushPts; // Flush PTS
    int64_t flushResetPts; // Flush with reset PTS
    int64_t eosPts; // EOS PTS
    int64_t decode_time_threshold; // Decode time budget per frame (ns)
    double rms_diff_threashold; // RMS difference threshold
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
        UT_LOG_INFO("Calling getState(OUT:_aidl_return:[state])"); \
        status = (videoDecoder)->getState(&state); \
        UT_LOG_INFO("Result getState(OUT:_aidl_return:[state:%d]), status:[isOK:%s, exception:%s]", \
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
        UT_LOG_INFO("Calling getVideoDecoderIds(OUT:decoderIds:[])"); \
        status = gVideoDecoderManager->getVideoDecoderIds(&decoderIds); \
        UT_LOG_INFO("Result getVideoDecoderIds(OUT:decoderIds:[size: %d]), status:[isOK:%s, exception:%s]", \
                                decoderIds.size(), UT_Control_GetMapString(bool_mapTable_, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable_, status.exceptionCode())); \
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
        UT_LOG_INFO("Calling registerEventListener(IN:videoDecoderEventListener:[0x%p], OUT:_aidl_return[])", listenerPtr); \
        status = (videoDecoder)->registerEventListener((listener), &aidl_return); \
        UT_LOG_INFO("Result registerEventListener(IN:videoDecoderEventListener:[0x%p], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", \
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
         UT_LOG_INFO("Calling getCapabilities(OUT:capabilities)"); \
         status = videoDecoder->getCapabilities(capabilities); \
         UT_LOG_INFO("Result getCapabilities(OUT:capabilities), status:[isOK:%s, exception:%s]", \
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
        UT_LOG_INFO("Calling unregisterEventListener(IN:videoDecoderEventListener:[0x%p], OUT:_aidl_return[])", listenerPtr); \
        status = (videoDecoder)->unregisterEventListener((listener), &aidl_return); \
        UT_LOG_INFO("Result unregisterEventListener(IN:videoDecoderEventListener:[0x%p], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", \
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
        UT_LOG_INFO("Calling open(IN:codec:[%d], IN:secure:[%s], IN:listener:[0x%p], OUT:controller[])", \
                    static_cast<int32_t>(codec), UT_Control_GetMapString(bool_mapTable_, (secure)), (listener).get()); \
        status = (videoDecoder)->open((codec), (secure), (listener), &(controller)); \
        UT_LOG_INFO("Result open(IN:codec:[%d], IN:secure:[%s], IN:listener:[0x%p], OUT:controller:[0x%X]), status:[isOK:%s, exception:%s]", \
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
        UT_LOG_INFO("Calling start()"); \
        status = (videoDecoderController)->start(); \
        UT_LOG_INFO("Result start(), status:[isOK:%s, exception:%s]", \
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
        UT_LOG_INFO("Calling decodeBuffer(IN:nsPresentationTime[%ld], IN:bufferHandle[0x%lX], OUT:_aidl_return[])", \
            nsPresentationTime, bufferHandle); \
        status = videoDecoderController->decodeBuffer(nsPresentationTime, bufferHandle, &_aidl_return); \
        UT_LOG_INFO("Result decodeBuffer(IN:nsPresentationTime[%ld], IN:bufferHandle[0x%lX], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
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
        UT_LOG_INFO("Calling decodeBuffer(IN:nsPresentationTime[%ld], IN:bufferHandle[0x%lX], OUT:_aidl_return[])", \
            nsPresentationTime, bufferHandle); \
        status = videoDecoderController->decodeBuffer(nsPresentationTime, bufferHandle, &_aidl_return); \
        UT_LOG_INFO("Result decodeBuffer(IN:nsPresentationTime[%ld], IN:bufferHandle[0x%lX], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
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
        UT_LOG_INFO("Calling setProperty(IN:property[%d], IN:propertyValue=%d)", \
                    static_cast<int32_t>(property), propertyValue.value); \
        status = videoDecoderController->setProperty(property, propertyValue, &result); \
        UT_LOG_INFO("Result setProperty(IN:property[%d], OUT:result=%s), status:[isOK:%s, exception:%s]", \
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
        UT_LOG_INFO("Calling stop()"); \
        status = videoDecoderController->stop(); \
        UT_LOG_INFO("Result stop(), status:[isOK:%s, exception:%s]", \
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
        UT_LOG_INFO("Calling close(IN:videoDecoderController:[0x%p], OUT:_aidl_return[])", controllerPtr); \
        status = (videoDecoder)->close((controller), &aidl_return); \
        UT_LOG_INFO("Result close(IN:videoDecoderController:[0x%p], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", \
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

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
#include "vts_l3_common.h"
#include "vts_l3_avbuffer.h"
#include "vts_l3_videodecoder.h"

#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"
#include "vts_videosink_parse_testconfig.h"
#include "vts_videosink.h"

using namespace std::chrono;
using namespace std;

#define UT_LOG_WARN

/* Test class registration with UT_Core */
UT_ADD_TEST_TO_GROUP(VTS_L3_VideoSink, UT_TESTS_L3)

static const char *gModuleName = "IVideoSinkManager";
static const char *gTestGroup = "L3";

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

    void verifyNoUnexpectedUnderflow(int64_t pts);;
    void verifyUnderflow();
    void verifyResume();
    void verifyQueueDepth();
    void verifyFramesReceived();
    void verifyFramesDroppedLate();
    void verifyFramesDroppedFRC();
    void verifyFramesRepeatedFRC();
    void verifyFramesMissed();
    void verifyFlushComplete();
    void handleEosFrame(const FrameMetadata& metadata, int64_t pts, int repeat, int repeatCount);
    void runPostVerification();

    void run ( bool isTunnel )
    {
        if(!isTunnel)
        {
            videoSinkNonTunnelMode();
        }
        else
        {
            videoSinkTunnelMode();
        }
    }

    bool getSinkPropertyInt(videosink::Property property, int32_t& outValue)
    {

        std::optional<com::rdk::hal::PropertyValue> propertyValue;
        ::android::binder::Status status;

        outValue = -1;

        status = mSinkHandle->getProperty(property, &propertyValue);
        if (!status.isOk() ||
            status.exceptionCode() != ::android::binder::Status::EX_NONE ||
            !propertyValue.has_value() ||
            !propertyValue->value.has_value())
        {
            UT_LOG_ERROR("Failed to get property %d", static_cast<int>(property));
            UT_LOG_ERROR("Result getProperty(IN:property value[%d], property value has Value[%d]), status:[isOK:%s, exception:%s]", videosink::toString(property).c_str(), \
                propertyValue.has_value(), \
                propertyValue->value.has_value(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \

            return false;
        }

        outValue = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();
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
    sp<IVideoDecoderControllerListener> mDecoderControllerListener;
    sp<IVideoDecoderEventListener> mDecoderEventListener;
    Pool mVideoPool;
    bool mSecure;
    videoTestConfiguration_t mTestConfiguration;

    float getBytesPerPixelMultiplier(const std::string& basePixelFormat, int colorDepth) const
    {
        int bytesPerChannel = (colorDepth <= 8) ? 1 : 2;

        if (basePixelFormat == "YCBCR420")
            return 1.5f * bytesPerChannel;
        if (basePixelFormat == "YCBCR422")
            return 2.0f * bytesPerChannel;
        if (basePixelFormat == "YCBCR444")
            return 3.0f * bytesPerChannel;
        if (basePixelFormat == "RGB")
            return 3.0f * bytesPerChannel;
        if (basePixelFormat == "NATIVE")
            return 0.0f;

        return 0.0f;
    }

    PixelFormat getPixelFormatEnum(const std::string& basePixelFormat)
    {
        if (basePixelFormat == "YCBCR420")
            return PixelFormat::YCBCR420;
        if (basePixelFormat == "YCBCR422")
            return PixelFormat::YCBCR422;
        if (basePixelFormat == "YCBCR444")
            return PixelFormat::YCBCR444;
        if (basePixelFormat == "RGB")
            return PixelFormat::RGB;
        if (basePixelFormat == "NATIVE")
            return PixelFormat::NATIVE;

        // Default fallback, can throw or return a special value if needed
        return PixelFormat::NATIVE;
    }

    void videoSinkNonTunnelMode()
    {
        sp<IVideoSinkController> controller;
        sp<IAVBufferSpaceListener> bufferListener = new AVBufferSpaceListener();
        int32_t sinkQueueDepth, framesReceived, framesDroppedLate, framesDroppedFrc, framesRepeatedFrc, framesMissingFrame;

        UT_LOG_INFO("VIDEOSINK_OPEN_SUCCESS ctrl listner 0x%x", mVideoSinkControlListener);
        VIDEOSINK_OPEN_SUCCESS(mSinkHandle, mVideoSinkControlListener, controller, true);
        UT_LOG_INFO("VIDEOSINK_OPEN_SUCCESS ctrl listner 0x%x", mVideoSinkControlListener);

        mVideoSinkControlListener->resetUnderflow();
        mVideoSinkControlListener->resetResumed();

        VIDEOSINK_START_SUCCESS(mSinkHandle, controller, true);
        UT_LOG_INFO("Sink opened and started");

        AVBUFFER_CREATE_VIDEOPOOL_SUCCESS(mAVBufferManager, mSecure, mDecoderId, bufferListener, mVideoPool);
        UT_LOG_INFO("Buffer pool created");

        int32_t repeatCount = mTestConfiguration.repeatPerFrame > 0 ? mTestConfiguration.repeatPerFrame : 1;

        if (mTestConfiguration.discardFramesUntilPts != static_cast<uint64_t>(-1))
        {
            ::com::rdk::hal::State currentState;
            VIDEOSINK_GET_STATE(mSinkHandle, currentState);

            if (currentState == com::rdk::hal::State::STARTED)
            {
                UT_LOG_INFO("Calling discardFramesUntil with PTS %ld", mTestConfiguration.discardFramesUntilPts);
                VIDEOSINK_SIGNAL_DISCARDFRAMESTILL_SUCCESS(controller, mTestConfiguration.discardFramesUntilPts);
            }
            else
            {
                UT_LOG_ERROR("Skipping discardFramesUntil - sink is not in STARTED state, current state = %s",
                            com::rdk::hal::toString(currentState).c_str());
            }
        }

        UT_LOG_INFO("Metadata Details: name=%s, width=%u, height=%u, input=%s, codec=%d, numFrames=%zu, repeatPerFrame=%u eos %d underflowPts=%d",
            mTestConfiguration.frameMetadata.name.c_str(),
            mTestConfiguration.frameMetadata.width,
            mTestConfiguration.frameMetadata.height,
            mTestConfiguration.input.c_str(),
            mTestConfiguration.codec,
            mTestConfiguration.frames.size(),
            mTestConfiguration.repeatPerFrame,
            mTestConfiguration.eosPts,
            mTestConfiguration.underflowPts );


        for (const auto& frame : mTestConfiguration.frames)
        {
            for (int32_t repeat = 0; repeat < repeatCount; ++repeat)
            {
                int64_t pts = frame.pts;

                int64_t bufferHandle;
                std::string baseFormat = mTestConfiguration.input;
                float bytesPerPixel = getBytesPerPixelMultiplier(baseFormat, mTestConfiguration.frameMetadata.colorDepth);

                if (bytesPerPixel <= 0.0f)
                {
                    UT_LOG_ERROR("Unsupported pixel format: %s - cannot calculate buffer size", baseFormat.c_str());
                    // Handle error or default allocation size here if needed
                    continue;
                }

                size_t allocSize = static_cast<size_t>(
                    mTestConfiguration.frameMetadata.width *
                    mTestConfiguration.frameMetadata.height *
                    bytesPerPixel
                );

                AVBUFFER_ALLOC_SUCCESS(mAVBufferManager, mVideoPool, allocSize, bufferHandle);

                FrameMetadata metadata = {};
                metadata.codedWidth = mTestConfiguration.frameMetadata.width;
                metadata.codedHeight = mTestConfiguration.frameMetadata.height;
                metadata.activeWidth = mTestConfiguration.frameMetadata.width;
                metadata.activeHeight = mTestConfiguration.frameMetadata.height;
                metadata.pixelFormat = getPixelFormatEnum(mTestConfiguration.input);
                metadata.colorDepth = mTestConfiguration.frameMetadata.colorDepth;
                metadata.frameRateNumerator = frame.frameRate;
                metadata.frameRateDenominator = 1;
                metadata.endOfStream = (mTestConfiguration.eosPts != static_cast<uint64_t>(-1) &&
                                        pts == mTestConfiguration.eosPts && repeat == repeatCount - 1);
                metadata.discontinuity = (mTestConfiguration.signalDiscontinuityPts != static_cast<uint64_t>(-1) &&
                                        pts == mTestConfiguration.signalDiscontinuityPts && repeat == 0);
                metadata.scanType = static_cast<ScanType>(mTestConfiguration.frameMetadata.scanType);
                metadata.dynamicRange = static_cast<DynamicRange>(mTestConfiguration.frameMetadata.dynamicRange);
                metadata.source = AVSource::HDMI_1;
                metadata.sha1 = std::vector<uint8_t>(20, 0x00);
                metadata.sha1[0] = 0x12;
                metadata.sha1[1] = 0x34;
                metadata.sha1[2] = 0x56;
                metadata.sha1[3] = static_cast<uint8_t>((pts + repeat) % 256);

                UT_LOG_WARN("Queueing frame: PTS=%ld, Handle=%ld, EOS=%d, Discontinuity=%d",
                            pts, bufferHandle, metadata.endOfStream, metadata.discontinuity, mTestConfiguration.underflowPts);

                if (mTestConfiguration.underflowPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.underflowPts)
                {
                    uint32_t frameDurationMs = static_cast<uint32_t>(1000.0 / frame.frameRate);
                    UT_LOG_WARN("Injecting delay at PTS %ld to simulate underflow", pts);

                    std::this_thread::sleep_for(std::chrono::milliseconds(frameDurationMs));
                }

                VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_SUCCESS(controller, pts, bufferHandle, metadata);

                if (mTestConfiguration.underflowPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.underflowPts)
                {
                    verifyUnderflow();
                }

                if (mTestConfiguration.resumeTriggerPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.resumeTriggerPts)
                {
                    verifyResume();
                    mVideoSinkControlListener->resetUnderflow();
                }

                handleEosFrame(metadata, pts, repeat, repeatCount);

                if (mTestConfiguration.interFrameDelayMs > 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(mTestConfiguration.interFrameDelayMs));
                }

                if (mTestConfiguration.underflowPts == static_cast<uint64_t>(-1))
                {
                    verifyNoUnexpectedUnderflow(pts);
                }

                if (repeat == 0)
                {
                    if (mTestConfiguration.flushPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.flushPts)
                    {
                        UT_LOG_WARN("Flushing video sink at PTS %ld", pts);
                        VIDEOSINK_FLUSH_SUCCESS(mSinkHandle, controller, false);
                        verifyFlushComplete();
                    }

                    if (mTestConfiguration.flushResetPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.flushResetPts)
                    {
                        UT_LOG_WARN("Flush+Reset video sink at PTS %ld", pts);
                        VIDEOSINK_FLUSH_SUCCESS(mSinkHandle, controller, true);
                        VIDEOSINK_START_SUCCESS(mSinkHandle, controller, true);
                    }

                    if (frame.errorFrame)
                    {
                        UT_LOG_ERROR("Injected error frame at PTS: %ld", pts);
                    }
                }

                AVBUFFER_FREE_SUCCESS(mAVBufferManager, bufferHandle);
                UT_LOG_WARN("Freed buffer handle %ld", bufferHandle);
            }
        }

        runPostVerification();

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        VIDEOSINK_STOP_SUCCESS(mSinkHandle, controller, true);
        VIDEOSINK_CLOSE_SUCCESS(mSinkHandle, controller, true);
        AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, mVideoPool);
        UT_LOG_WARN("Video sink test cleanup completed");
    }

    void videoSinkTunnelMode()
    {
#if 0
        sp<IVideoSinkController> controller;
        sp<IVideoDecoderController> videoDecoderController;
        sp<IAVBufferSpaceListener> bufferListener = new AVBufferSpaceListener();

        // Open Sink
        VIDEOSINK_OPEN_SUCCESS(mSinkHandle, mVideoSinkControlListener, controller, true);
        UT_LOG_INFO("Video sink opened in tunneled mode");

        // Start Sink
        VIDEOSINK_START_SUCCESS(mSinkHandle, controller, true);
        UT_LOG_INFO("Video sink started");

        // Create buffer pool
        AVBUFFER_CREATE_VIDEOPOOL_SUCCESS(mAVBufferManager, mSecure, mDecoderId, bufferListener, mVideoPool);
        UT_LOG_INFO("Buffer pool created for tunneled mode");

        std::ifstream inputFile(mTestConfiguration.input, std::ios::binary);
        if (!inputFile)
        {
            UT_LOG_ERROR("Failed to open file: %s", mTestConfiguration.input.c_str());
            return;
        }

        // Open Decoder
        VIDEODECODER_OPEN_SUCCESS(mDecoderHandle, mTestConfiguration.codec, mSecure, mDecoderControllerListener, videoDecoderController, true);
        UT_LOG_INFO("Video decoder opened");

        // Set OPERATIONAL_MODE to TUNNELLED
        PropertyValue propertyValue;
        propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(
            static_cast<int32_t>(OperationalMode::TUNNELLED));
        VIDEODECODER_SETPROPERTY_SUCCESS(videoDecoderController, com::rdk::hal::videodecoder::Property::OPERATIONAL_MODE, propertyValue);
        UT_LOG_INFO("Decoder set to TUNNELLED mode");

        // Start Decoder
        VIDEODECODER_START_SUCCESS(mDecoderHandle, videoDecoderController, true);
        UT_LOG_INFO("Video decoder started");

        for (const auto& frame : mTestConfiguration.frames)
        {
            int64_t pts = frame.pts;
            uint32_t frameSize = frame.size;
            bool isErrorFrame = frame.errorFrame;

            int64_t videoBufferHandle = -1;

            AVBUFFER_ALLOC_SUCCESS(mAVBufferManager, mVideoPool, frameSize, videoBufferHandle);
            UT_LOG_INFO("Allocated buffer handle %ld for PTS %ld", videoBufferHandle, pts);

            // Read frame data
            std::vector<uint8_t> inputBuffer(frameSize);
            inputFile.read(reinterpret_cast<char*>(inputBuffer.data()), frameSize);

            if (!inputFile)
            {
                UT_LOG_ERROR("Failed to read frame data for PTS %ld", pts);
                AVBUFFER_FREE_SUCCESS(mAVBufferManager, videoBufferHandle);
                continue;
            }

            // Inject frame data into buffer
            if (mSecure)
            {
                if (!gAVbufferHelper->writeSecureHandle(videoBufferHandle, inputBuffer.data(), frameSize))
                {
                    UT_LOG_ERROR("Failed to write data to secure buffer at PTS %ld", pts);
                    AVBUFFER_FREE_SUCCESS(mAVBufferManager, videoBufferHandle);
                    continue;
                }
            }
            else
            {
                void* mapBuffer = nullptr;
                uint32_t mapSize = 0;
                mapBuffer = gAVbufferHelper->mapHandle(videoBufferHandle, &mapSize);

                if (!mapBuffer || mapSize < frameSize)
                {
                    UT_LOG_ERROR("Buffer map failed or insufficient size at PTS %ld", pts);
                    AVBUFFER_FREE_SUCCESS(mAVBufferManager, videoBufferHandle);
                    continue;
                }

                memcpy(mapBuffer, inputBuffer.data(), frameSize);
            }

            if (mTestConfiguration.eosPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.eosPts)
            {
                UT_LOG_INFO("Sending EOS frame at PTS %ld", pts);
                VIDEODECODER_DECODEBUFFER_FAILURE(videoDecoderController, pts, videoBufferHandle, Status::EX_NONE);

                UT_LOG_INFO("Restarting decoder after EOS");
                VIDEODECODER_STOP_SUCCESS(mDecoderHandle, videoDecoderController, true);
                VIDEODECODER_START_SUCCESS(mDecoderHandle, videoDecoderController, true);
            }
            else
            {
                if (isErrorFrame)
                {
                    UT_LOG_ERROR("Injected error frame at PTS: %ld", pts);
                }

                VIDEODECODER_DECODEBUFFER_SUCCESS(videoDecoderController, pts, videoBufferHandle);
            }

            // Handle flush signals after decode
            if (mTestConfiguration.flushPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.flushPts)
            {
                UT_LOG_INFO("Flushing video sink at PTS %ld", pts);
                VIDEOSINK_FLUSH_SUCCESS(mSinkHandle, controller, false);
            }

            if (mTestConfiguration.flushResetPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.flushResetPts)
            {
                UT_LOG_INFO("Flush+Reset video sink at PTS %ld", pts);
                VIDEOSINK_FLUSH_SUCCESS(mSinkHandle, controller, true);
                VIDEOSINK_START_SUCCESS(mSinkHandle, controller, true);
            }

            // Unmap and free the buffer
            if (!mSecure)
            {
                gAVbufferHelper->unmapHandle(videoBufferHandle);
            }
            AVBUFFER_FREE_SUCCESS(mAVBufferManager, videoBufferHandle);
            UT_LOG_INFO("Freed buffer handle %ld", videoBufferHandle);
        }

        // Teardown and cleanup
        VIDEODECODER_STOP_SUCCESS(mDecoderHandle, videoDecoderController, true);
        VIDEODECODER_CLOSE_SUCCESS(mDecoderHandle, videoDecoderController, true);
        VIDEOSINK_STOP_SUCCESS(mSinkHandle, controller, true);
        VIDEOSINK_CLOSE_SUCCESS(mSinkHandle, controller, true);
        AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, mVideoPool);
        UT_LOG_INFO("Tunnel mode video sink test cleanup completed");
#endif
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


        if(getVideoDecoderService() != UT_STATUS_OK)
        {
            UT_LOG_ERROR("getVideoDecoderService failed %d",__LINE__);
        }
        else
        {
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
        }

        for(const auto& _sinkId: sinkIds)
        {
            sp<IVideoSink> videoSink;

            sp<VideoSinkEventListener> videoSinkEventListener = new VideoSinkEventListener();

            sp<VideoSinkControllerListener> videoSinkControlListener = new VideoSinkControllerListener();


            videoSinkControlListener->setSinkID(_sinkId.value);
            videoSinkEventListener->setSinkID(_sinkId.value);

            /* Get Video Sink handle */
            VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

            /* Register events */
            VIDEOSINK_REGISTER_EVENT_SUCCESS(videoSink, videoSinkEventListener);

            mVideoSinkList.push_back({_sinkId, videoSink, videoSinkControlListener, videoSinkEventListener});

        }
    }

    void runSequentialSink(videoTestConfiguration_t sinkConfig, testConfiguration_t decoderConfig, bool secure, bool tunnel)
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


            AVBUFFER_GETMETRICS(mAVBufferManager, secure, beforeHeapMetrics);

            DecoderTest decoderTest(_decoderId, decoderHandle,
                                    sp<VideoDecoderControllerListener>::cast(decoderControlListener),
                                    sp<VideoDecoderEventListener>::cast(decoderEventListener),
                                    sp<IAVBuffer>::cast(mAVBufferManager)
                                    , secure, tunnel, decoderConfig);

            decoderTest.decodeAndConsumeVideoStreams();

            SinkTest sinkTest(_sinkId, videoSink,
                videoSinkControlListener, videoSinkEventListener,
                decoderHandle, _decoderId,
                decoderControlListener, decoderEventListener,
                mAVBufferManager, secure, sinkConfig);


            sinkTest.run(tunnel);



            AVBUFFER_GETMETRICS(mAVBufferManager, secure, afterHeapMetrics);

            UT_ASSERT_TRUE(beforeHeapMetrics.secure == afterHeapMetrics.secure);
            UT_ASSERT_TRUE(beforeHeapMetrics.bytesUsed == afterHeapMetrics.bytesUsed);
            UT_ASSERT_TRUE(beforeHeapMetrics.bytesTotal == afterHeapMetrics.bytesTotal);
        }
    }


    void runConcurrentSink(videoTestConfiguration_t& testConfig,testConfiguration_t decoderConfig, bool secure, bool tunnel)
    {
        std::vector<std::thread> threads;
        std::vector<SinkTest> sinkTests;  // Store SinkTest objects here

        HeapMetrics beforeHeapMetrics, afterHeapMetrics;
        AVBUFFER_GETMETRICS(mAVBufferManager, secure, beforeHeapMetrics);

        for (size_t i = 0; i < mVideoSinkList.size(); ++i)
        {
            auto& [_sinkId, videoSink, videoSinkControlListener, videoSinkEventListener] = mVideoSinkList[i];
            auto& [_decoderId, decoderHandle, decoderControlListener, decoderEventListener, decoderCapabilities] = mVideoDecoderList[i];

#if 0 /*Need to enable during tunnel mode implementation*/
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
#endif
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
        vector<videoTestConfiguration_t> sinkConfigs;
        vector<testConfiguration_t> decoderConfigs;

        vts_videodecoder_parse_testconfig(gTestConfiguration, gModuleName, gTestGroup, testName, decoderConfigs);
        vts_videosink_parse_testconfig(gTestConfiguration, gModuleName, gTestGroup, testName, sinkConfigs);

        for (auto& sinkConfig : sinkConfigs)
        {
            for (auto& decoderConfig : decoderConfigs)
            {
                if (concurrentMode)
                {
                    runConcurrentSink(sinkConfig, decoderConfig, secureMode, isTunnel);
                }
                else
                {
                    runSequentialSink(sinkConfig, decoderConfig, secureMode, isTunnel);
                }
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

void SinkTest::verifyNoUnexpectedUnderflow(int64_t pts)
{
    int count = mVideoSinkControlListener->getUnderflowCount();
    if (count > 0)
    {
        UT_LOG_ERROR("Unexpected underflow at PTS %ld! Count: %d", pts, count);
        UT_ASSERT_TRUE(false);
    }
}

void SinkTest::verifyUnderflow()
{
    auto now = []() {
        std::time_t t = std::time(nullptr);
        char mbstr[100];
        std::strftime(mbstr, sizeof(mbstr), "%H:%M:%S", std::localtime(&t));
        return std::string(mbstr);
    };

    UT_LOG_INFO("Waiting for Video Underflow event at %s...", now().c_str());
    bool underflowOccurred = mVideoSinkControlListener->waitForUnderflowEvent(2);
    if (!underflowOccurred)
    {
        UT_LOG_ERROR("Video Underflow event NOT received within timeout at %s", now().c_str());
        UT_ASSERT_TRUE(false);
    }
    else
    {
        UT_LOG_INFO("Video Underflow event received successfully at %s", now().c_str());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Wait for metric to settle

    int actualUnderflowed = -1;
    bool success = getSinkPropertyInt(videosink::Property::METRIC_UNDERFLOWED, actualUnderflowed);
    if (!success || actualUnderflowed == -1)
    {
        UT_LOG_ERROR("Failed to read METRIC_UNDERFLOWED at %s", now().c_str());
        UT_ASSERT_TRUE(false);
    }
    else if (actualUnderflowed > 0)
    {
        UT_LOG_INFO("Detected %d underflow events at %s", actualUnderflowed, now().c_str());
    }
    else
    {
        UT_LOG_ERROR("Expected at least 1 underflow, got 0 at %s", now().c_str());
        UT_ASSERT_TRUE(false);
    }
}

void SinkTest::verifyResume()
{
    auto now = []() {
        std::time_t t = std::time(nullptr);
        char mbstr[100];
        std::strftime(mbstr, sizeof(mbstr), "%H:%M:%S", std::localtime(&t));
        return std::string(mbstr);
    };

    UT_LOG_INFO("Waiting for Video Resume event after underflow at %s...", now().c_str());
    bool resumedOccurred = mVideoSinkControlListener->waitForResumedEvent(2);
    if (!resumedOccurred)
    {
        UT_LOG_ERROR("Video Resume event NOT received within timeout at %s", now().c_str());
        UT_ASSERT_TRUE(false);
    }
    else
    {
        UT_LOG_INFO("Video Resume event received successfully at %s", now().c_str());
    }
}

void SinkTest::handleEosFrame(const FrameMetadata& metadata, int64_t pts, int repeat, int repeatCount)
{
    if (metadata.endOfStream)
    {
        UT_LOG_INFO("End-of-stream frame queued, waiting for EOS callback...");

        bool eosArrived = mVideoSinkControlListener->waitForEosEvent(2);  // Wait up to 2 seconds

        if (!eosArrived)
        {
            UT_LOG_ERROR("EOS callback not received within timeout.");
            UT_ASSERT_TRUE(false);
        }
        else
        {
            int64_t eosPTS = mVideoSinkControlListener->getEosPTS();
            UT_LOG_INFO("EOS received. nsPresentationTime = %ld", eosPTS);

            uint64_t expectedPts = mTestConfiguration.eosPts;
            if (eosPTS != static_cast<int64_t>(expectedPts))
            {
                UT_LOG_ERROR("Mismatch in EOS PTS. Expected: %lu, Got: %ld", expectedPts, eosPTS);
                UT_ASSERT_TRUE(false);
            }
        }
    }
}

void SinkTest::verifyFlushComplete()
{
    UT_LOG_INFO("Waiting for flush complete event...");

    bool flushReceived = mVideoSinkControlListener->waitForFlushComplete(std::chrono::seconds(3));
    if (!flushReceived)
    {
        UT_LOG_ERROR("Flush complete event not received within timeout.");
        UT_ASSERT_TRUE(false);
    }
    else
    {
        UT_LOG_INFO("Flush complete event received.");
    }
}

void SinkTest::verifyQueueDepth()
{
    int actualQueueDepth = -1;

    bool success = getSinkPropertyInt(videosink::Property::SINK_QUEUE_DEPTH, actualQueueDepth);
    if (!success)
    {
        UT_LOG_ERROR("Failed to get SINK_QUEUE_DEPTH property");
        UT_ASSERT_TRUE(false);
    }

    UT_LOG_INFO(" Actual Queue Depth: %d", actualQueueDepth);
    if (actualQueueDepth < 0)
    {
        UT_LOG_ERROR("No Queue Depth value!: %d", actualQueueDepth);
        UT_ASSERT_TRUE(false);
    }
    else
    {
        UT_LOG_INFO("Queue Depth matches expected value.");
    }
}


void SinkTest::verifyFramesReceived()
{
    int expectedFramesReceived = 0;
    int actualFramesReceived = -1;
    int repeatValue = mTestConfiguration.repeatPerFrame > 0 ? mTestConfiguration.repeatPerFrame : 1;
    for (const auto& frame : mTestConfiguration.frames)
    {
        expectedFramesReceived += repeatValue;
    }

    bool success = getSinkPropertyInt(videosink::Property::METRIC_FRAMES_RECEIVED, actualFramesReceived);
    if (!success)
    {
        UT_LOG_ERROR("Failed to get METRIC_FRAMES_RECEIVED property");
        UT_ASSERT_TRUE(false);
    }

    if (actualFramesReceived == expectedFramesReceived)
    {
        UT_LOG_INFO("Frames received matches expected: %d", actualFramesReceived);
    }
    else
    {
        UT_LOG_ERROR("Frames received mismatch. Expected: %d, Actual: %d", expectedFramesReceived, actualFramesReceived);
        UT_ASSERT_TRUE(false);
    }
}

void SinkTest::verifyFramesDroppedLate()
{
    int actualFramesDroppedLate = -1;

    UT_LOG_INFO("Waiting briefly to allow metrics to update...");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    bool success = getSinkPropertyInt(videosink::Property::METRIC_FRAMES_DROPPED_LATE, actualFramesDroppedLate);
    if (!success)
    {
        UT_LOG_ERROR("Failed to get METRIC_FRAMES_DROPPED_LATE property");
        UT_ASSERT_TRUE(false);
    }

    if (actualFramesDroppedLate >= 1)
    {
        UT_LOG_INFO("Detected late frame drops as expected: %d", actualFramesDroppedLate);
    }
    else
    {
        UT_LOG_ERROR("Expected late frame drops, but got: %d", actualFramesDroppedLate);
        UT_ASSERT_TRUE(false);
    }
}

void SinkTest::verifyFramesDroppedFRC()
{
    int actualDroppedFRC = -1;

    UT_LOG_INFO("Waiting briefly to allow metrics to update...");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    bool success = getSinkPropertyInt(videosink::Property::METRIC_FRAMES_DROPPED_FRC, actualDroppedFRC);
    if (!success)
    {
        UT_LOG_ERROR("Failed to get METRIC_FRAMES_DROPPED_FRC property");
        UT_ASSERT_TRUE(false);
    }

    if (actualDroppedFRC > 0)
    {
        UT_LOG_INFO("Detected frame drops due to FRC as expected: %d", actualDroppedFRC);
    }
    else
    {
        UT_LOG_ERROR("Expected frame drops due to FRC, but got: %d", actualDroppedFRC);
        UT_ASSERT_TRUE(false);
    }
}

void SinkTest::verifyFramesRepeatedFRC()
{
    int actualRepeatedFRC = -1;

    UT_LOG_INFO("Waiting briefly to allow metrics to update...");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    bool success = getSinkPropertyInt(videosink::Property::METRIC_FRAMES_REPEATED_FRC, actualRepeatedFRC);
    if (!success)
    {
        UT_LOG_ERROR("Failed to get METRIC_FRAMES_REPEATED_FRC property");
        UT_ASSERT_TRUE(false);
    }

    if (actualRepeatedFRC > 0)
    {
        UT_LOG_INFO("Detected frame repetitions due to FRC as expected: %d", actualRepeatedFRC);
    }
    else
    {
        UT_LOG_ERROR("Expected frame repetitions due to FRC, but got: %d", actualRepeatedFRC);
        UT_ASSERT_TRUE(false);
    }
}

void SinkTest::verifyFramesMissed()
{
    int actualRepeatMissing = -1;

    UT_LOG_INFO("Waiting briefly to allow metrics to update...");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    bool success = getSinkPropertyInt(videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME, actualRepeatMissing);
    if (!success || actualRepeatMissing == -1)
    {
        UT_LOG_ERROR("METRIC_FRAMES_REPEATED_MISSING_FRAME not supported or fetch failed.");
        UT_ASSERT_TRUE(false);
    }
    else if (actualRepeatMissing > 0)
    {
        UT_LOG_INFO("Frames repeated due to missing frame: %d", actualRepeatMissing);
    }
    else
    {
        UT_LOG_ERROR("Expected some frames to be repeated due to missing frame, but got 0");
        UT_ASSERT_TRUE(false);
    }
}

void SinkTest::runPostVerification()
{
    if ( mTestConfiguration.testName == "Render_Sequential_QueueDepth_NonTunnel")
    {
        verifyQueueDepth();
    }
    else if (mTestConfiguration.testName == "Render_Sequential_FramesReceived_NonTunnel")
    {
        verifyFramesReceived();
    }
    else if (mTestConfiguration.testName == "Render_Sequential_FramesDroppedLate_NonTunnel")
    {
        verifyFramesDroppedLate();
    }
    else if (mTestConfiguration.testName == "Render_Sequential_FramesDroppedFRC_NonTunnel")
    {
        verifyFramesDroppedFRC();
    }
    else if (mTestConfiguration.testName == "Render_Sequential_FramesRepeatedFRC_NonTunnel")
    {
        verifyFramesRepeatedFRC();
    }
    else if (mTestConfiguration.testName == "Render_Sequential_FramesMiss_NonTunnel")
    {
        verifyFramesMissed();
    }
    else if ( mTestConfiguration.testName == "Render_Concurrent_QueueDepth_NonTunnel")
    {
        verifyQueueDepth();
    }
    else if (mTestConfiguration.testName == "Render_Concurrent_FramesReceived_NonTunnel")
    {
        verifyFramesReceived();
    }
    else if (mTestConfiguration.testName == "Render_Concurrent_FramesDroppedLate_NonTunnel")
    {
        verifyFramesDroppedLate();
    }
    else if (mTestConfiguration.testName == "Render_Concurrent_FramesDroppedFRC_NonTunnel")
    {
        verifyFramesDroppedFRC();
    }
    else if (mTestConfiguration.testName == "Render_Concurrent_FramesRepeatedFRC_NonTunnel")
    {
        verifyFramesRepeatedFRC();
    }
    else if (mTestConfiguration.testName == "Render_Concurrent_FramesMiss_NonTunnel")
    {
        verifyFramesMissed();
    }
}


/**
 * @brief Test the sink's ability to report correct queue depth
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 001@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Sequential_QueueDepth_NonTunnel)
{
    const char *testName = "Render_Sequential_QueueDepth_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test the number of frames received by video sink
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 002@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Sequential_FramesReceived_NonTunnel)
{
    const char *testName = "Render_Sequential_FramesReceived_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames dropped due to lateness
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 004@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Sequential_FramesDroppedLate_NonTunnel)
{
    const char *testName = "Render_Sequential_FramesDroppedLate_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames repeated due to frame rate changes
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 006@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Sequential_FramesRepeatedFRC_NonTunnel)
{
    const char *testName = "Render_Sequential_FramesRepeatedFRC_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}


/**
 * @brief Test for repeated frames due to missing frame detection
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 007@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Sequential_FramesMiss_NonTunnel)
{
    const char *testName = "Render_Sequential_FramesMiss_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for repeated frames due to missing frame detection
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 008@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Sequential_FramesDroppedFRC_NonTunnel)
{
    const char *testName = "Render_Sequential_FramesDroppedFRC_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 009@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Sequential_VideoFlush_NonTunnel)
{
    const char *testName = "Render_Sequential_VideoFlush_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 010@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Sequential_EndToEnd_NonTunnel)
{
    const char *testName = "Render_Sequential_EndToEnd_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Video sink playback with underflow detection
 *        Modes: Non-Tunnel, Non-Secure
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Sequential_UnderflowResumed_NonTunnel)
{
    const char *testName = "Render_Sequential_UnderflowResumed_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify discardFramesUntil behavior in video sink
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 011@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Sequential_DiscardFramesUntil_NonTunnel)
{
    const char *testName = "Render_Sequential_DiscardFramesUntil_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify discardFramesUntil behavior in video sink
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 011@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Sequential_UnderFlowCheck_NonTunnel)
{
    const char *testName = "Render_Sequential_UnderFlowCheck_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test the sink's ability to report correct queue depth
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 012@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Concurrent_QueueDepth_NonTunnel)
{
    const char *testName = "Render_Concurrent_QueueDepth_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test the number of frames received by video sink
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 013@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Concurrent_FramesReceived_NonTunnel)
{
    const char *testName = "Render_Concurrent_FramesReceived_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames dropped due to lateness
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 014@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Concurrent_FramesDroppedLate_NonTunnel)
{
    const char *testName = "Render_Concurrent_FramesDroppedLate_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames repeated due to frame rate changes
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 015@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Concurrent_FramesRepeatedFRC_NonTunnel)
{
    const char *testName = "Render_Concurrent_FramesRepeatedFRC_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}


/**
 * @brief Test for repeated frames due to missing frame detection
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 016@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Concurrent_FramesMiss_NonTunnel)
{
    const char *testName = "Render_Concurrent_FramesMiss_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for repeated frames due to missing frame detection
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 017@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Concurrent_FramesDroppedFRC_NonTunnel)
{
    const char *testName = "Render_Concurrent_FramesDroppedFRC_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 018@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Concurrent_VideoFlush_NonTunnel)
{
    const char *testName = "Render_Concurrent_VideoFlush_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Decoding type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 010@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Concurrent_EndToEnd_NonTunnel)
{
    const char *testName = "Render_Concurrent_EndToEnd_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Video sink playback with underflow detection
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 011@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Concurrent_UnderflowResumed_NonTunnel)
{
    const char *testName = "Render_Concurrent_UnderflowResumed_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify discardFramesUntil behavior in video sink
 *        Decoding type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 012@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Render_Concurrent_DiscardFramesUntil_NonTunnel)
{
    const char *testName = "Render_Concurrent_DiscardFramesUntil_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify underflow check behavior in the normal video sink scenario
 *        Decoding type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 013@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, vts_l3_Render_Concurrent_UnderFlowCheck_NonTunnel)
{
    const char *testName = "vts_l3_Render_Concurrent_UnderFlowCheck_NonTunnel";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/** @} */ // End of VIDEO_SINK_HALTEST L3 File
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK Module
/** @} */ // End of HPK

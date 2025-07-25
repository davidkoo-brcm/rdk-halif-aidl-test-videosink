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
#include "vts_common.h"

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
static const char *gVideoDecoderModuleName = "IVideoDecoderManager";
const char *gTestGroup = "L3";
IAVBufferHelper* gAVbufferHelper;

extern std::vector<int32_t> gAvailableVideosinks_hfp;
extern std::vector<int32_t> gAvailableVideodecoders_hfp;

static sp<IAVBuffer> vts_avbuffer_getService()
{
    return
        android::interface_cast<IAVBuffer>(
            android::defaultServiceManager()->getService(
                android::String16(IAVBuffer::serviceName().c_str())
            )
        );
}

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
            videoDecoderTestConfiguration_t& config)
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

    void verifyNoUnexpectedUnderflow();
    void verifyUnderflow();
    void verifyResume();
    void verifyQueueDepth();
    void verifyFramesReceived();
    void verifyFramesDroppedLate();
    void verifyFramesDroppedFRC();
    void verifyFramesRepeatedFRC();
    void verifyFramesMissed();
    void verifyFlushComplete();
    void handleEosFrame(const videodecoder::FrameMetadata& metadata, int64_t pts);
    void runPostVerification();
    Status queueDecodedFrame(int64_t pts, int64_t frameBufferHandle, const videodecoder::FrameMetadata& metadata);

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
    videoDecoderTestConfiguration_t mTestConfiguration;
    std::set<int64_t> receivedPtsSet;
    int actualFramesReceived = 0;

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
        try {
            sp<IVideoSinkController> controller;

            UT_LOG_WARNING("[SinkID: %d] Start of Testing Stream: %s", mSinkId.value, mTestConfiguration.input.c_str());

            /*Open the Video Sink*/
            VIDEOSINK_OPEN_SUCCESS(mSinkHandle, mVideoSinkControlListener, controller, true);

            /*Reset the Flush, EOS, Underflow and Resume Counts*/
            mVideoSinkControlListener->resetUnderflow();
            mVideoSinkEventListner->resetUnderflow();
            mVideoSinkControlListener->resetResumed();
            mVideoSinkEventListner->resetResumed();
            mVideoSinkControlListener->resetEos();
            mVideoSinkEventListner->resetEos();
            mVideoSinkControlListener->resetFlushComplete();
            mVideoSinkEventListner->resetFlushComplete();

            /*Start the Video Sink*/
            VIDEOSINK_START_SUCCESS(mSinkHandle, controller, true);

            int64_t frameTimeoutMs = 500; // Wait up to 5 seconds for a frame

            /*Set the "discard until PTS" value for the relevant test cases*/
            if (mTestConfiguration.sink_discardFramesUntilPts != static_cast<uint64_t>(-1))
            {
                VIDEOSINK_SIGNAL_DISCARDFRAMESTILL_SUCCESS(controller, mTestConfiguration.sink_discardFramesUntilPts);
            }

            while (true)
            {
                auto optFrame = gFrameQueue.waitAndPop(frameTimeoutMs);

                if (!optFrame)
                {
                    UT_LOG_DEBUG("[SinkID: %d]Timeout waiting for frame from decoder, exiting loop",mSinkId.value);
                    break;  // No frame received within timeout - exit or handle accordingly
                }

                FrameData& frame = *optFrame;
                int64_t pts = frame.pts;
                int64_t bufferHandle = frame.bufferHandle;
                videodecoder::FrameMetadata& metadata = frame.metadata;
                metadata.endOfStream = (mTestConfiguration.sink_eos_pts != static_cast<uint64_t>(-1) &&
                            pts == mTestConfiguration.sink_eos_pts);

                UT_LOG_DEBUG("[SinkID: %d]Queueing live frame: PTS=%ld, Handle=%ld, EOS=%d, Discontinuity=%d underFlow_pts=%d flush %d",
                                                                                                                                mSinkId.value,
                                                                                                                                pts,
                                                                                                                                bufferHandle,
                                                                                                                                metadata.endOfStream,
                                                                                                                                metadata.discontinuity,
                                                                                                                                mTestConfiguration.sink_underflowPts,
                                                                                                                                mTestConfiguration.sink_flush_pts);

                /*Introducing a delay to simulate an underflow event in the sink queue.*/
                if (mTestConfiguration.sink_underflowPts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.sink_underflowPts)
                {
                    uint32_t frameDurationMs = static_cast<uint32_t>( 1000.0 * metadata.frameRateDenominator / metadata.frameRateNumerator);
                    UT_LOG_DEBUG("[SinkID: %d]Injecting delay at PTS %ld to simulate underflow",mSinkId.value, pts);
                    std::this_thread::sleep_for(std::chrono::milliseconds(frameDurationMs));
                }

                VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_SUCCESS(controller, pts, bufferHandle, metadata);

                receivedPtsSet.insert(pts);
                ++actualFramesReceived;

                if (metadata.endOfStream)
                {
                    UT_LOG_DEBUG("EOS frame received, stopping video sink input loop");
                    break;
                }


                if (mTestConfiguration.sink_interFrameDelayMs > 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(mTestConfiguration.sink_interFrameDelayMs));
                }

                if (mTestConfiguration.sink_flush_pts != static_cast<uint64_t>(-1) && pts == mTestConfiguration.sink_flush_pts)
                {
                    VIDEOSINK_FLUSH_SUCCESS(mSinkHandle, controller, false);
                }

            }

            /*Validation of EOS*/
            if (mTestConfiguration.sink_eos_pts >= 0)
            {
                videodecoder::FrameMetadata dummyMeta{};
                dummyMeta.endOfStream = true;
                handleEosFrame(dummyMeta, mTestConfiguration.sink_eos_pts);
            }

            /*Validation of Simulated Underflow*/
            if (mTestConfiguration.sink_underflowPts >= 0)
            {
                verifyUnderflow();
            }

            /*Validation of Simulated Resume*/
            if (mTestConfiguration.sink_resumeTriggerPts >= 0)
            {
                verifyResume();
            }

            /*Validation of Flush*/
            if (mTestConfiguration.sink_flush_pts >= 0)
            {
                verifyFlushComplete();
            }

            /*Validation of whether UnderFlow occured in normal Queuing*/
            verifyNoUnexpectedUnderflow();

            /* Validating the metrics data after the queuing process is complete. */
            runPostVerification();

            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            VIDEOSINK_STOP_SUCCESS(mSinkHandle, controller, true);
            VIDEOSINK_CLOSE_SUCCESS(mSinkHandle, controller, true);
        }
        catch (const std::exception& ex) {
            UT_LOG_ERROR("[SinkID: %d] Exception in videoSinkNonTunnelMode: %s", mSinkId.value, ex.what());
            UT_ASSERT_TRUE(false);
        } catch (...) {
            UT_LOG_ERROR("[SinkID: %d] Unknown exception in videoSinkNonTunnelMode", mSinkId.value);
            UT_ASSERT_TRUE(false);
        }
    }

    void videoSinkTunnelMode()
    {
    //TODO: Need to implement the tunnel test.
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
        gVideoSinkTestConfiguration = vts_kvp_create_instance(VIDEO_SINK_TEST_CONFIG_PATH);

        gAVbufferHelper = getAVBufferHelperInstance();

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

                /*Get Video Decoder details*/
                VIDEODECODER_MANAGER_GETVIDEODECODER(_videoDecoderId, videoDecoder);
                /*Register the events */
                VIDEODECODER_REGISTER_EVENT_SUCCESS(videoDecoder, videoDecoderEventListener);
                /*Get Capabilities of Video Decoder */
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

            //TODO: VideoSink Get capabilities are yet to implement in the video sink

            mVideoSinkList.push_back({_sinkId, videoSink, videoSinkControlListener, videoSinkEventListener});

        }
    }

    bool isVideoSinkUsable(const IVideoSink::Id& sinkId)
    {
        return std::find(gAvailableVideosinks_hfp.begin(), gAvailableVideosinks_hfp.end(), sinkId.value) != gAvailableVideosinks_hfp.end();
    }

    bool isVideoDecoderUsable(const IVideoDecoder::Id& decoderId)
    {
        return std::find(gAvailableVideodecoders_hfp.begin(),
                            gAvailableVideodecoders_hfp.end(),
                            decoderId.value) != gAvailableVideodecoders_hfp.end();
    }


    void runSequentialSink(videoDecoderTestConfiguration_t& testConfig, bool secureDecode, bool isTunnel)
    {
        try {
            for (const auto& [_sinkId, sinkHandle, controlListener, eventListener] : mVideoSinkList)
            {
                if (!isVideoSinkUsable(_sinkId)) {
                    UT_LOG_INFO("[SinkID: %d] is not usable, skipping.", _sinkId.value);
                    continue;
                }

                // Find a usable decoder
                IVideoDecoder::Id decoderId;
                sp<IVideoDecoder> decoderHandle;
                sp<VideoDecoderControllerListener> decoderControlListener;
                sp<VideoDecoderEventListener> decoderEventListener;
                bool foundDecoder = false;

                for (const auto& [_decoderId, _decoderHandle, _controlListener, _eventListener, _capabilities] : mVideoDecoderList)
                {
                    if (isVideoDecoderUsable(_decoderId)) {
                        decoderId = _decoderId;
                        decoderHandle = _decoderHandle;
                        decoderControlListener = sp<VideoDecoderControllerListener>::cast(_controlListener);
                        decoderEventListener = sp<VideoDecoderEventListener>::cast(_eventListener);
                        foundDecoder = true;
                        break;
                    }
                }

                if (!foundDecoder) {
                    UT_LOG_WARNING("No usable decoder found for Sink ID %d, skipping.", _sinkId.value);
                    continue;
                }

                HeapMetrics beforeHeapMetrics, afterHeapMetrics;
                AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, beforeHeapMetrics);

                videoDecoderTest decoderTest(decoderId, decoderHandle, sp<VideoDecoderControllerListener>::cast(decoderControlListener),
                                        sp<VideoDecoderEventListener>::cast(decoderEventListener), sp<IAVBuffer>::cast(mAVBufferManager), secureDecode, isTunnel, testConfig,gAVbufferHelper);

                SinkTest sinkTest(_sinkId, sinkHandle, controlListener, eventListener,
                                decoderHandle, decoderId, decoderControlListener, decoderEventListener,
                                mAVBufferManager, secureDecode, testConfig);

                gFrameQueue.clear();

                std::thread decoderThread([&decoderTest]() {
                    decoderTest.run(true);
                });

                std::thread sinkThread([&sinkTest, isTunnel]() {
                    sinkTest.run(isTunnel);
                });

                decoderThread.join();
                sinkThread.join();

                AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, afterHeapMetrics);
                UT_ASSERT_TRUE(beforeHeapMetrics.secure == afterHeapMetrics.secure);
                UT_ASSERT_TRUE(beforeHeapMetrics.bytesUsed == afterHeapMetrics.bytesUsed);
                UT_ASSERT_TRUE(beforeHeapMetrics.bytesTotal == afterHeapMetrics.bytesTotal);
            }
        } catch (const std::exception& ex) {
            UT_LOG_ERROR("Exception in runSequentialSink: %s", ex.what());
            UT_ASSERT_TRUE(false);
        } catch (...) {
            UT_LOG_ERROR("Unknown exception in runSequentialSink");
            UT_ASSERT_TRUE(false);
        }
    }



    void runConcurrentSink(videoDecoderTestConfiguration_t decoderConfig, bool secure, bool tunnel)
    {

        try{
            std::vector<std::thread> threads;
            vector<thread> decoderThreads;
            vector<unique_ptr<SinkTest>> sinkTests;
            vector<thread> sinkThreads;
            vector<unique_ptr<videoDecoderTest>> decoderTests;
            HeapMetrics beforeHeapMetrics, afterHeapMetrics;
            vector<std::tuple<IVideoDecoder::Id, sp<IVideoDecoder>, sp<VideoDecoderControllerListener>, sp<VideoDecoderEventListener>>> availableDecoderList;

            /* Get heap metrics */
            AVBUFFER_GETMETRICS(mAVBufferManager, secure, beforeHeapMetrics);

            for (const auto& [_decoderId, decoderHandle, decoderControlListener, decoderEventListener, decoderCapabilities] : mVideoDecoderList)
            {

                auto decoderTest = make_unique<videoDecoderTest>(_decoderId, decoderHandle,
                        sp<VideoDecoderControllerListener>::cast(decoderControlListener),
                        sp<VideoDecoderEventListener>::cast(decoderEventListener),
                        sp<IAVBuffer>::cast(mAVBufferManager),
                        secure, tunnel, decoderConfig,gAVbufferHelper);

                decoderThreads.emplace_back(&videoDecoderTest::run, decoderTest.get(), true);

                decoderTests.push_back(move(decoderTest));

                availableDecoderList.push_back({
                                                _decoderId,
                                                decoderHandle,
                                                sp<VideoDecoderControllerListener>::cast(decoderControlListener),
                                                sp<VideoDecoderEventListener>::cast(decoderEventListener)
                                            });
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            for(int32_t i = 0; i < availableDecoderList.size(); i++)
            {
                auto& [_sinkId, videoSink, videoSinkControlListener, videoSinkEventListener] = mVideoSinkList[i];

                if(!isVideoSinkUsable(_sinkId))
                {
                    UT_LOG_INFO("[SinkID: %d] is not usable for this test configuration, skipping.", _sinkId.value);
                    continue;
                }

                auto& [_decoderId, decoderHandle, decoderControlListener, decoderEventListener] = availableDecoderList[i];

                auto sinkTest = std::make_unique<SinkTest>(_sinkId, videoSink,
                                    videoSinkControlListener, videoSinkEventListener,
                                    decoderHandle, _decoderId,
                                    decoderControlListener, decoderEventListener,
                                    mAVBufferManager, secure, decoderConfig);

                sinkThreads.emplace_back(&SinkTest::run, sinkTest.get(), tunnel);

                sinkTests.push_back(std::move(sinkTest));
            }

            for (auto& t : decoderThreads)
            {
                if (t.joinable())
                    t.join();
            }
            for (auto& t : sinkThreads)
            {
                if (t.joinable())
                    t.join();
            }
            /* Get heap metrics */
            AVBUFFER_GETMETRICS(mAVBufferManager, secure, afterHeapMetrics);

            /* check if the decoder freed all the buffer pools */
            UT_ASSERT_TRUE(beforeHeapMetrics.secure == afterHeapMetrics.secure);
            UT_ASSERT_TRUE(beforeHeapMetrics.bytesUsed == afterHeapMetrics.bytesUsed);
            UT_ASSERT_TRUE(beforeHeapMetrics.bytesTotal == afterHeapMetrics.bytesTotal);
        } catch (const std::exception& ex) {
            UT_LOG_ERROR("Exception in runConcurrentSink: %s", ex.what());
            UT_ASSERT_TRUE(false);
        } catch (...) {
            UT_LOG_ERROR("Unknown exception in runConcurrentSink");
            UT_ASSERT_TRUE(false);
        }
    }

    void runSingleProcessTest(const char *testName, bool secureMode, bool isTunnel, bool concurrentMode)
    {
        vector<videoDecoderTestConfiguration_t> decoderConfigs;

        if(secureMode)
        {
            UT_LOG_WARNING("Video Sink does not support secure mode, skipping test!");
            return;
        }

        if(isTunnel)
        {
            UT_LOG_WARNING("Video Sink does not support tunnel mode, skipping test!");
            return;
        }

        vts_videodecoder_parse_testconfig(gVideoSinkTestConfiguration, VIDEODECODERMGR, gTestGroup, testName, decoderConfigs);

        for (auto& decoderConfig : decoderConfigs)
        {
            if (concurrentMode)
            {
                runConcurrentSink(decoderConfig, secureMode, isTunnel);
            }
            else
            {
                runSequentialSink(decoderConfig, secureMode, isTunnel);
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
        //TODO: check right place to call joinThreadPool
        //android::IPCThreadState::self()->joinThreadPool();
    }
};

/**
 * @brief Verifies that no unexpected video underflow events have occurred.
 *
 * This function checks the underflow event counts from both the control and
 * event listeners after the frame queuing loop has completed. If any underflow
 * events are detected and no underflow was explicitly expected (i.e.,
 * sink_underflowPts is not set), it logs an error and fails the test.
 *
 * This check is performed in a non-blocking manner using count-based validation.
 */
void SinkTest::verifyNoUnexpectedUnderflow()
{
    if (mTestConfiguration.sink_underflowPts == static_cast<uint64_t>(-1)) {
        int countCtrl = mVideoSinkControlListener->getUnderflowCount();
        int countEvent = mVideoSinkEventListner->getUnderflowCount();
        if (countCtrl > 0 || countEvent > 0) {
            UT_LOG_ERROR("[SinkID: %d] Unexpected underflows occurred! Control listner: %d, Event listner: %d for Stream[%s]\n", mSinkId.value, countCtrl, countEvent,mTestConfiguration.input.c_str());
            UT_ASSERT_TRUE(false);
        }
    }
}

/**
 * @brief Verifies that a video underflow event occurred as expected.
 *
 * This non-blocking check verifies whether underflow events were detected
 * by both the control and event listeners by comparing their internal underflow counts.
 * It assumes underflow was expected and should have occurred by this point.
 *
 * Logs an error and fails the test if underflow was not observed by either listener.
 * Otherwise, logs a warning with the detected underflow counts.
 */
void SinkTest::verifyUnderflow()
{
    /*Verify both Control and Event listner callbacks*/
    int countCtrl = mVideoSinkControlListener->getUnderflowCount();
    int countEvent = mVideoSinkEventListner->getUnderflowCount();

    if (countCtrl <= 0 || countEvent <= 0)
    {
        UT_LOG_ERROR("[SinkID: %d] Underflow NOT detected properly (Ctrl: %d, Event: %d)", mSinkId.value, countCtrl, countEvent);
        UT_ASSERT_TRUE(false);
    }
    else
    {
        UT_LOG_WARNING("[SinkID: %d] Underflows detected (Control listner: %d, Event listner: %d)", mSinkId.value, countCtrl, countEvent);
    }
}

/**
 * @brief Verifies that a video resume event occurred after an underflow.
 *
 * This non-blocking version checks whether resume events were detected
 * by both the control and event listeners using their internal counters.
 * It assumes a resume was expected and should have already occurred.
 *
 * Logs an error and fails the test if resume was not detected by either listener.
 * Otherwise, logs a warning showing the detected resume counts.
 */
void SinkTest::verifyResume()
{
    /*Verify both Control and Event listner callbacks*/
    int countCtrl = mVideoSinkControlListener->getResumedCount();
    int countEvent = mVideoSinkEventListner->getResumedCount();

    if (countCtrl <= 0 || countEvent <= 0)
    {
        UT_LOG_ERROR("[SinkID: %d] Resume NOT detected properly (Ctrl: %d, Event: %d)", mSinkId.value, countCtrl, countEvent);
        UT_ASSERT_TRUE(false);
    }
    else
    {
        UT_LOG_WARNING("[SinkID: %d] Resume detected (Control listner: %d, Event listner: %d)", mSinkId.value, countCtrl, countEvent);
    }
}


/**
 * @brief Handles End-of-Stream (EOS) frame processing and verification.
 *
 * Checks if the given frame metadata indicates EOS, waits for the EOS event callback
 * from the sink, and verifies the EOS presentation timestamp (PTS).
 *
 * @param metadata Frame metadata containing EOS flag.
 * @param pts Presentation timestamp of the frame.
 */
void SinkTest::handleEosFrame(const videodecoder::FrameMetadata& metadata, int64_t pts)
{
    if (!metadata.endOfStream)
        return;

    // Wait for EOS callback from control listener
    bool eosArrived = mVideoSinkControlListener->waitForEosEvent(2);
    if (!eosArrived)
    {
        UT_LOG_ERROR("[SinkID: %d] Timeout waiting for EOS callback from control listener.", mSinkId.value);
        UT_ASSERT_TRUE(false);
        return;
    }

    int64_t eosPTS = mVideoSinkControlListener->getEosPTS();
    if (eosPTS != pts)
    {
        UT_LOG_ERROR("[SinkID: %d] EOS PTS mismatch from control listener. Expected: %ld, Got: %ld", mSinkId.value, pts, eosPTS);
        UT_ASSERT_TRUE(false);
        return;
    }

    // Wait for EOS callback from event listener
    eosArrived = mVideoSinkEventListner->waitForEosEvent(2);
    if (!eosArrived)
    {
        UT_LOG_ERROR("[SinkID: %d] Timeout waiting for EOS callback from event listener.", mSinkId.value);
        UT_ASSERT_TRUE(false);
        return;
    }

    eosPTS = mVideoSinkEventListner->getEosPTS();
    if (eosPTS != pts)
    {
        UT_LOG_ERROR("[SinkID: %d] EOS PTS mismatch from event listener. Expected: %ld, Got: %ld", mSinkId.value, pts, eosPTS);
        UT_ASSERT_TRUE(false);
        return;
    }

    // Confirm EOS PTS match
    UT_LOG_WARNING("[SinkID: %d] EOS PTS match confirmed: %ld", mSinkId.value, pts);
}


/**
 * @brief Verifies that a flush complete event occurred in the video sink.
 *
 * This non-blocking version checks internal counters from both the control
 * and event listeners to confirm flush completion. It assumes the flush
 * operation has already been triggered earlier.
 *
 * Logs an error and fails the test if flush completion was not observed.
 * Otherwise, logs the detected flush complete counts.
 */
void SinkTest::verifyFlushComplete()
{
    /*Verify both Control and Event listner callbacks*/
    int countCtrl = mVideoSinkControlListener->getFlushCompleteCount();
    int countEvent = mVideoSinkEventListner->getFlushCompleteCount();

    if (countCtrl <= 0 || countEvent <= 0)
    {
        UT_LOG_ERROR("[SinkID: %d] FlushComplete NOT detected properly (Ctrl: %d, Event: %d)", mSinkId.value, countCtrl, countEvent);
        UT_ASSERT_TRUE(false);
    }
    else
    {
        UT_LOG_WARNING("[SinkID: %d] FlushComplete detected (ControlListner: %d, Event Listner: %d)", mSinkId.value, countCtrl, countEvent);
    }
}

/**
 * @brief Verifies the current queue depth reported by the video sink.
 *
 * This function retrieves the `SINK_QUEUE_DEPTH` property from the sink and validates
 * that it is a non-negative value. The test fails if the property cannot be retrieved
 * or if the value is invalid.
 */
void SinkTest::verifyQueueDepth()
{
    int actualQueueDepth = -1;

    /* Verifying Video Sink QueueDepth Metrics */
    bool success = getSinkPropertyInt(videosink::Property::SINK_QUEUE_DEPTH, actualQueueDepth);
    if (!success)
    {
        UT_LOG_ERROR("[SinkID: %d] Failed to get SINK_QUEUE_DEPTH property", mSinkId.value);
        UT_ASSERT_TRUE(false);
        return;
    }

    UT_LOG_WARNING("[SinkID: %d] Num of Frames in Queue Depth: %d", mSinkId.value, actualQueueDepth);

    if (actualQueueDepth < 0)
    {
        UT_LOG_ERROR("[SinkID: %d] No Queue Depth value!: %d", mSinkId.value, actualQueueDepth);
        UT_ASSERT_TRUE(false);
    }
}


/**
 * @brief Verifies the number of frames received by the video sink.
 *
 * This function checks the `METRIC_FRAMES_RECEIVED` reported by the sink and compares it
 * with the actual number of frames tracked by the test. The test fails if there is a mismatch
 * or if the property cannot be retrieved.
 */
void SinkTest::verifyFramesReceived()
{
    int sinkFramesReported = -1;

    // Verifying Video Sink Frame received Metrics
    bool success = getSinkPropertyInt(videosink::Property::METRIC_FRAMES_RECEIVED, sinkFramesReported);
    if (!success)
    {
        UT_LOG_ERROR("[SinkID: %d] Failed to get METRIC_FRAMES_RECEIVED property", mSinkId.value);
        UT_ASSERT_TRUE(false);
        return;
    }

    UT_LOG_WARNING("[Sink ID %d]Sink metrics reported frames: %d, Tracked frames : %d, PTS count at sink: %zu, Total PTS sent to decoder: %d",
                   mSinkId.value, sinkFramesReported, actualFramesReceived,
                   receivedPtsSet.size(), mTestConfiguration.no_of_frames);

    // Validate against counted frames
    if (sinkFramesReported != actualFramesReceived)
    {
        UT_LOG_ERROR("[SinkID: %d] Mismatch in received frames: Sink=%d, Counted=%d",
                     mSinkId.value, sinkFramesReported, actualFramesReceived);
        UT_ASSERT_TRUE(false);
    }

    // Validate against configured expected number of frames
    if (mTestConfiguration.no_of_frames >= 0 && actualFramesReceived != static_cast<int>(mTestConfiguration.no_of_frames))
    {
        UT_LOG_ERROR("[SinkID: %d] Frame count mismatch against configuration: Expected=%d, Received=%d",
                     mSinkId.value, mTestConfiguration.no_of_frames, actualFramesReceived);
        UT_ASSERT_TRUE(false);
    }
}


/**
 * @brief Verifies if any frames were dropped due to late arrival.
 *
 * This function retrieves the `METRIC_FRAMES_DROPPED_LATE` property from the video sink
 * and asserts that one or more frames were dropped because they arrived too late for rendering.
 * The test fails if the metric cannot be retrieved or if no late drops are detected when expected.
 */
void SinkTest::verifyFramesDroppedLate()
{
    int actualFramesDroppedLate = -1;

    /* Verifying Video Sink Frame Dropped Metrics */

    UT_LOG_DEBUG("[SinkID: %d] Waiting briefly to allow metrics to update...", mSinkId.value);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    bool success = getSinkPropertyInt(videosink::Property::METRIC_FRAMES_DROPPED_LATE, actualFramesDroppedLate);
    if (!success)
    {
        UT_LOG_ERROR("[SinkID: %d] Failed to get METRIC_FRAMES_DROPPED_LATE property", mSinkId.value);
        UT_ASSERT_TRUE(false);
        return;
    }

    if (actualFramesDroppedLate >= 1)
    {
        UT_LOG_WARNING("[SinkID: %d] Detected late frame drops as expected: %d", mSinkId.value, actualFramesDroppedLate);
    }
    else
    {
        UT_LOG_ERROR("[SinkID: %d] Expected late frame drops, but got: %d", mSinkId.value, actualFramesDroppedLate);
        UT_ASSERT_TRUE(false);
    }
}

/**
 * @brief Verifies if frames were dropped due to Frame Rate Conversion (FRC).
 *
 * This function queries the sink for the `METRIC_FRAMES_DROPPED_FRC` property
 * and checks whether any frames were dropped as a result of FRC behavior.
 * It fails the test if the metric is unavailable or if no drops are detected
 * when drops are expected.
 */
void SinkTest::verifyFramesDroppedFRC()
{
    int actualDroppedFRC = -1;

    /* Verifying Video Sink Frames Dropped FRC Metrics */

    UT_LOG_DEBUG("[SinkID: %d] Waiting briefly to allow metrics to update...", mSinkId.value);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    bool success = getSinkPropertyInt(videosink::Property::METRIC_FRAMES_DROPPED_FRC, actualDroppedFRC);
    if (!success)
    {
        UT_LOG_ERROR("[SinkID: %d] Failed to get METRIC_FRAMES_DROPPED_FRC property", mSinkId.value);
        UT_ASSERT_TRUE(false);
        return;
    }

    if (actualDroppedFRC > 0)
    {
        UT_LOG_WARNING("[SinkID: %d] Detected frame drops due to FRC as expected: %d", mSinkId.value, actualDroppedFRC);
    }
    else
    {
        UT_LOG_ERROR("[SinkID: %d] Expected frame drops due to FRC, but got: %d", mSinkId.value, actualDroppedFRC);
        UT_ASSERT_TRUE(false);
    }
}


/**
 * @brief Verifies if frames were repeated due to Frame Rate Conversion (FRC).
 *
 * This function queries the sink for the `METRIC_FRAMES_REPEATED_FRC` property
 * and validates that frame repetitions occurred due to FRC. It fails the test
 * if the metric retrieval fails or if no FRC-related repetitions are detected
 * when they are expected.
 */
void SinkTest::verifyFramesRepeatedFRC()
{
    int actualRepeatedFRC = -1;

    /* Verifying Video Sink Frames Repeated FRC Metrics */

    UT_LOG_DEBUG("[SinkID: %d] Waiting briefly to allow metrics to update...", mSinkId.value);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    bool success = getSinkPropertyInt(videosink::Property::METRIC_FRAMES_REPEATED_FRC, actualRepeatedFRC);
    if (!success)
    {
        UT_LOG_ERROR("[SinkID: %d] Failed to get METRIC_FRAMES_REPEATED_FRC property", mSinkId.value);
        UT_ASSERT_TRUE(false);
        return;
    }

    if (actualRepeatedFRC > 0)
    {
        UT_LOG_WARNING("[SinkID: %d] Detected frame repetitions due to FRC as expected: %d", mSinkId.value, actualRepeatedFRC);
    }
    else
    {
        UT_LOG_ERROR("[SinkID: %d] Expected frame repetitions due to FRC, but got: %d", mSinkId.value, actualRepeatedFRC);
        UT_ASSERT_TRUE(false);
    }
}


/**
 * @brief Verifies if any frames were repeated due to missing frames.
 *
 * This function checks the `METRIC_FRAMES_REPEATED_MISSING_FRAME` sink property
 * to determine if frames were repeated as a result of missing input frames.
 * It asserts failure if the metric is unsupported or if no missing frame repetitions
 * are reported when expected.
 */
void SinkTest::verifyFramesMissed()
{
    int actualRepeatMissing = -1;

    /* Verifying Video Sink Frames Missed Metrics */

    UT_LOG_DEBUG("[SinkID: %d] Waiting briefly to allow metrics to update...", mSinkId.value);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    bool success = getSinkPropertyInt(videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME, actualRepeatMissing);
    if (!success || actualRepeatMissing == -1)
    {
        UT_LOG_ERROR("[SinkID: %d] METRIC_FRAMES_REPEATED_MISSING_FRAME not supported or fetch failed.", mSinkId.value);
        UT_ASSERT_TRUE(false);
        return;
    }

    if (actualRepeatMissing > 0)
    {
        UT_LOG_WARNING("[SinkID: %d] Frames repeated due to missing frame: %d", mSinkId.value, actualRepeatMissing);
    }
    else
    {
        UT_LOG_ERROR("[SinkID: %d] Expected some frames to be repeated due to missing frame, but got 0", mSinkId.value);
        UT_ASSERT_TRUE(false);
    }
}


/**
 * @brief Performs post-verification based on the current sink test name.
 *
 * Determines which verification routine to execute depending on the test scenario
 * defined in `mTestConfiguration.sink_testName`. Handles various cases including
 * queue depth, frames received, dropped frames, repeated frames, and missed frames,
 * for both sequential and concurrent non-tunnel modes.
 */
void SinkTest::runPostVerification()
{
    const std::string &testName = mTestConfiguration.sink_testName;

    if (testName == "Sink_QueueDepth")
    {
        verifyQueueDepth();
    }
    else if (testName == "Sink_FramesReceived")
    {
        verifyFramesReceived();
    }
    else if (testName == "Sink_FramesDroppedLate")
    {
        verifyFramesDroppedLate();
    }
    else if (testName == "Sink_FramesDroppedFRC")
    {
        verifyFramesDroppedFRC();
    }
    else if (testName == "Sink_FramesRepeatedFRC")
    {
        verifyFramesRepeatedFRC();
    }
    else if (testName == "Sink_FramesMiss")
    {
        verifyFramesMissed();
    }
}


/**
 * @brief Test the sink's ability to report correct queue depth
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 001@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_NonSecure_QueueDepth)
{
    const char *testName = "Sink_QueueDepth";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test the number of frames received by video sink
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 002@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_NonSecure_FramesReceived)
{
    const char *testName = "Sink_FramesReceived";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames dropped due to lateness
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 003@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_NonSecure_FramesDroppedLate)
{
    const char *testName = "Sink_FramesDroppedLate";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames repeated due to frame rate changes
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 004@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_NonSecure_FramesRepeatedFRC)
{
    const char *testName = "Sink_FramesRepeatedFRC";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for repeated frames due to missing frame detection
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 005@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_NonSecure_FramesMiss)
{
    const char *testName = "Sink_FramesMiss";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Tests repeated frame rendering with dropped frames
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 006@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_NonSecure_FramesDroppedFRC)
{
    const char *testName = "Sink_FramesDroppedFRC";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 007@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_NonSecure_VideoFlush)
{
    const char *testName = "Sink_VideoFlush";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after eos pts to the sink.
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 008@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_NonSecure_EOS)
{
    const char *testName = "Sink_EOS";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Video sink playback with underflow detection
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 009@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_NonSecure_UnderflowResumed)
{
    const char *testName = "Sink_UnderflowResumed";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify discardFramesUntil behavior in video sink
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 010@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_NonSecure_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test the sink's ability to report correct queue depth
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 011@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_NonSecure_QueueDepth)
{
    const char *testName = "Sink_QueueDepth";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test the number of frames received by video sink
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 012@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_NonSecure_FramesReceived)
{
    const char *testName = "Sink_FramesReceived";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames dropped due to lateness
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 013@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_NonSecure_FramesDroppedLate)
{
    const char *testName = "Sink_FramesDroppedLate";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames repeated due to frame rate changes
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 014@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_NonSecure_FramesDroppedFRC)
{
    const char *testName = "Sink_FramesDroppedFRC";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}


/**
 * @brief Test for repeated frames due to missing frame detection
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 015@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_NonSecure_FramesRepeatedFRC)
{
    const char *testName = "Sink_FramesRepeatedFRC";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for repeated frames due to missing frame detection
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 016@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_NonSecure_FramesMiss)
{
    const char *testName = "Sink_FramesMiss";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 017@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_NonSecure_VideoFlush)
{
    const char *testName = "Sink_VideoFlush";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after sending eos pts to the sink.
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 018@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_NonSecure_EOS)
{
    const char *testName = "Sink_EOS";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Video sink playback with underflow detection
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 019@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_NonSecure_UnderflowResumed)
{
    const char *testName = "Sink_UnderflowResumed";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify discardFramesUntil behavior in video sink
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 020@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_NonSecure_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test the sink's ability to report correct queue depth
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 021@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_Secure_QueueDepth)
{
    const char *testName = "Sink_QueueDepth";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test the number of frames received by video sink
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 022@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_Secure_FramesReceived)
{
    const char *testName = "Sink_FramesReceived";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames dropped due to lateness
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 023@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_Secure_FramesDroppedLate)
{
    const char *testName = "Sink_FramesDroppedLate";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames repeated due to frame rate changes
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 024@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_Secure_FramesRepeatedFRC)
{
    const char *testName = "Sink_FramesRepeatedFRC";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for repeated frames due to missing frame detection
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 025@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_Secure_FramesMiss)
{
    const char *testName = "Sink_FramesMiss";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Tests repeated frame rendering with dropped frames
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 026@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_Secure_FramesDroppedFRC)
{
    const char *testName = "Sink_FramesDroppedFRC";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 027@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_Secure_VideoFlush)
{
    const char *testName = "Sink_VideoFlush";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after eos pts to the sink.
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 028@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_Secure_EOS)
{
    const char *testName = "Sink_EOS";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Video sink playback with underflow detection
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 029@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_Secure_UnderflowResumed)
{
    const char *testName = "Sink_UnderflowResumed";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify discardFramesUntil behavior in video sink
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 030@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_Secure_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test the sink's ability to report correct queue depth
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 031@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_Secure_QueueDepth)
{
    const char *testName = "Sink_QueueDepth";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test the number of frames received by video sink
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 032@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_Secure_FramesReceived)
{
    const char *testName = "Sink_FramesReceived";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames dropped due to lateness
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 033@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_Secure_FramesDroppedLate)
{
    const char *testName = "Sink_FramesDroppedLate";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames repeated due to frame rate changes
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 034@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_Secure_FramesDroppedFRC)
{
    const char *testName = "Sink_FramesDroppedFRC";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}


/**
 * @brief Test for repeated frames due to missing frame detection
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 035@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_Secure_FramesRepeatedFRC)
{
    const char *testName = "Sink_FramesRepeatedFRC";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for repeated frames due to missing frame detection
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 036@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_Secure_FramesMiss)
{
    const char *testName = "Sink_FramesMiss";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 037@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_Secure_VideoFlush)
{
    const char *testName = "Sink_VideoFlush";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after sending eos pts to the sink.
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 038@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_Secure_EOS)
{
    const char *testName = "Sink_EOS";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Video sink playback with underflow detection
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 039@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_Secure_UnderflowResumed)
{
    const char *testName = "Sink_UnderflowResumed";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify discardFramesUntil behavior in video sink
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 040@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_NonTunnel_Secure_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test the sink's ability to report correct queue depth
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 041@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_NonSecure_QueueDepth)
{
    const char *testName = "Sink_QueueDepth";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test the number of frames received by video sink
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 042@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_NonSecure_FramesReceived)
{
    const char *testName = "Sink_FramesReceived";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames dropped due to lateness
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 043@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_NonSecure_FramesDroppedLate)
{
    const char *testName = "Sink_FramesDroppedLate";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames repeated due to frame rate changes
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 044@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_NonSecure_FramesRepeatedFRC)
{
    const char *testName = "Sink_FramesRepeatedFRC";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for repeated frames due to missing frame detection
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 045@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_NonSecure_FramesMiss)
{
    const char *testName = "Sink_FramesMiss";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Tests repeated frame rendering with dropped frames
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 046@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_NonSecure_FramesDroppedFRC)
{
    const char *testName = "Sink_FramesDroppedFRC";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 047@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_NonSecure_VideoFlush)
{
    const char *testName = "Sink_VideoFlush";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after eos pts to the sink.
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 048@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_NonSecure_EOS)
{
    const char *testName = "Sink_EOS";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Video sink playback with underflow detection
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 049@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_NonSecure_UnderflowResumed)
{
    const char *testName = "Sink_UnderflowResumed";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify discardFramesUntil behavior in video sink
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 050@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_NonSecure_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test the sink's ability to report correct queue depth
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 051@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_NonSecure_QueueDepth)
{
    const char *testName = "Sink_QueueDepth";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test the number of frames received by video sink
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 052@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_NonSecure_FramesReceived)
{
    const char *testName = "Sink_FramesReceived";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames dropped due to lateness
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 053@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_NonSecure_FramesDroppedLate)
{
    const char *testName = "Sink_FramesDroppedLate";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames repeated due to frame rate changes
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 054@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_NonSecure_FramesDroppedFRC)
{
    const char *testName = "Sink_FramesDroppedFRC";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}


/**
 * @brief Test for repeated frames due to missing frame detection
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 055@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_NonSecure_FramesRepeatedFRC)
{
    const char *testName = "Sink_FramesRepeatedFRC";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for repeated frames due to missing frame detection
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 056@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_NonSecure_FramesMiss)
{
    const char *testName = "Sink_FramesMiss";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 057@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_NonSecure_VideoFlush)
{
    const char *testName = "Sink_VideoFlush";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after sending eos pts to the sink.
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 058@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_NonSecure_EOS)
{
    const char *testName = "Sink_EOS";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Video sink playback with underflow detection
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 059@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_NonSecure_UnderflowResumed)
{
    const char *testName = "Sink_UnderflowResumed";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify discardFramesUntil behavior in video sink
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 060@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_NonSecure_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test the sink's ability to report correct queue depth
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 061@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_Secure_QueueDepth)
{
    const char *testName = "Sink_Sequential_Tunnel_Secure_QueueDepth";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test the number of frames received by video sink
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 062@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_Secure_FramesReceived)
{
    const char *testName = "Sink_Sequential_Tunnel_Secure_FramesReceived";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames dropped due to lateness
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 063@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_Secure_FramesDroppedLate)
{
    const char *testName = "Sink_Sequential_Tunnel_Secure_FramesDroppedLate";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames repeated due to frame rate changes
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 064@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_Secure_FramesRepeatedFRC)
{
    const char *testName = "Sink_Sequential_Tunnel_Secure_FramesRepeatedFRC";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for repeated frames due to missing frame detection
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 065@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_Secure_FramesMiss)
{
    const char *testName = "Sink_Sequential_Tunnel_Secure_FramesMiss";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Tests repeated frame rendering with dropped frames
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 066@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_Secure_FramesDroppedFRC)
{
    const char *testName = "Sink_Sequential_Tunnel_Secure_FramesDroppedFRC";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 067@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_Secure_VideoFlush)
{
    const char *testName = "Sink_Sequential_Tunnel_Secure_VideoFlush";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after eos pts to the sink.
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 068@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_Secure_EOS)
{
    const char *testName = "Sink_Sequential_Tunnel_Secure_EOS";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Video sink playback with underflow detection
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 069@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_Secure_UnderflowResumed)
{
    const char *testName = "Sink_Sequential_Tunnel_Secure_UnderflowResumed";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify discardFramesUntil behavior in video sink
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 070@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_Tunnel_Secure_DiscardFramesUntil)
{
    const char *testName = "Sink_Sequential_Tunnel_Secure_DiscardFramesUntil";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test the sink's ability to report correct queue depth
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 071@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_Secure_QueueDepth)
{
    const char *testName = "Sink_Concurrent_Tunnel_Secure_QueueDepth";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test the number of frames received by video sink
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 072@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_Secure_FramesReceived)
{
    const char *testName = "Sink_Concurrent_Tunnel_Secure_FramesReceived";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames dropped due to lateness
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 073@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_Secure_FramesDroppedLate)
{
    const char *testName = "Sink_Concurrent_Tunnel_Secure_FramesDroppedLate";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for frames repeated due to frame rate changes
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 074@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_Secure_FramesDroppedFRC)
{
    const char *testName = "Sink_Concurrent_Tunnel_Secure_FramesDroppedFRC";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}


/**
 * @brief Test for repeated frames due to missing frame detection
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 075@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_Secure_FramesRepeatedFRC)
{
    const char *testName = "Sink_Concurrent_Tunnel_Secure_FramesRepeatedFRC";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test for repeated frames due to missing frame detection
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 076@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_Secure_FramesMiss)
{
    const char *testName = "Sink_Concurrent_Tunnel_Secure_FramesMiss";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 077@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_Secure_VideoFlush)
{
    const char *testName = "Sink_Concurrent_Tunnel_Secure_VideoFlush";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after sending eos pts to the sink.
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 078@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_Secure_EOS)
{
    const char *testName = "Sink_Concurrent_Tunnel_Secure_EOS";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Video sink playback with underflow detection
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 079@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_Secure_UnderflowResumed)
{
    const char *testName = "Sink_Concurrent_Tunnel_Secure_UnderflowResumed";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/**
 * @brief Test to verify discardFramesUntil behavior in video sink
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 080@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Concurrent_Tunnel_Secure_DiscardFramesUntil)
{
    const char *testName = "Sink_Concurrent_Tunnel_Secure_DiscardFramesUntil";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}
/** @} */ // End of VIDEO_SINK_HALTEST L3 File
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK Module
/** @} */ // End of HPK

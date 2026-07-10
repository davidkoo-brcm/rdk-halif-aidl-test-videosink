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
#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"
#include "vts_videosink_parse_testconfig.h"
#include "vts_videosink.h"
#include "vts_videodecoder.h"
#include "vts_videosink_test.h"

#include <condition_variable>
#include <mutex>

using namespace std::chrono;
using namespace std;

/* Test class registration with UT_Core */
UT_ADD_TEST_TO_GROUP(VTS_L3_VideoSink, UT_TESTS_L3)

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

class VTS_L3_VideoSink : public UTCore
{
public:
    VTS_L3_VideoSink() : UTCore(), mAVbufferHelper(nullptr) {}

    ~VTS_L3_VideoSink() override = default;

    vector<tuple<IVideoSink::Id, sp<IVideoSink>, sp<VideoSinkControllerListener>, sp<VideoSinkEventListener>>> mVideoSinkList;
    vector<tuple<IVideoDecoder::Id, sp<IVideoDecoder>, sp<VideoDecoderControllerListener>, sp<VideoDecoderEventListener>, videodecoder::Capabilities*>> mVideoDecoderList;
    sp<IAVBuffer> mAVBufferManager;
    IAVBufferHelper* mAVbufferHelper;
    bool mIsTunnelSupported = false;

    bool isSecureSupportedByDecoder(const videodecoder::Capabilities* capabilities) const
    {
        return (capabilities != nullptr) && capabilities->supportsSecure;
    }

    const decoder_t* getDecoderHfpCapabilities(const IVideoDecoder::Id& decoderId) const
    {
        auto it = std::find_if(gVideodecoders_hfp.begin(),
                               gVideodecoders_hfp.end(),
                               [&decoderId](const decoder_t& item) {
                                   return item.id == decoderId.value;
                               });

        if (it == gVideodecoders_hfp.end())
        {
            return nullptr;
        }

        return &(*it);
    }

    bool isDecoderCompatibleWithStreamFromHfp(const IVideoDecoder::Id& decoderId,
                                              const videoDecoderTestConfiguration_t& streamConfig,
                                              std::string& reason) const
    {
        const decoder_t* hfpDecoder = getDecoderHfpCapabilities(decoderId);
        if (hfpDecoder == nullptr)
        {
            reason = "decoder HFP capabilities unavailable";
            return false;
        }

        if (hfpDecoder->supportedCodecs.empty())
        {
            reason = "decoder HFP has no supportedCodecs";
            return false;
        }

        const double requestedFrameRate = (streamConfig.frameRateDenominator > 0)
                                              ? static_cast<double>(streamConfig.frameRateNumerator) /
                                                    static_cast<double>(streamConfig.frameRateDenominator)
                                              : 0.0;

        bool codecSeen = false;
        bool pixelFormatSeen = false;
        bool resolutionSeen = false;
        bool frameRateSeen = false;

        for (const auto& codecCaps : hfpDecoder->supportedCodecs)
        {
            if (codecCaps.codec != streamConfig.codec)
            {
                continue;
            }

            codecSeen = true;

            if (codecCaps.supportedOutputPixelFormats.empty())
            {
                reason = "decoder HFP missing supportedOutputPixelFormats";
                return false;
            }

            if (std::find(codecCaps.supportedOutputPixelFormats.begin(),
                          codecCaps.supportedOutputPixelFormats.end(),
                          streamConfig.pixelFormat) == codecCaps.supportedOutputPixelFormats.end())
            {
                continue;
            }
            pixelFormatSeen = true;

            if ((codecCaps.maxFrameWidth > 0 && streamConfig.codedWidth > codecCaps.maxFrameWidth) ||
                (codecCaps.maxFrameHeight > 0 && streamConfig.codedHeight > codecCaps.maxFrameHeight))
            {
                continue;
            }
            resolutionSeen = true;

            if (codecCaps.maxFrameRate > 0 && requestedFrameRate > static_cast<double>(codecCaps.maxFrameRate))
            {
                continue;
            }
            frameRateSeen = true;

            if (streamConfig.dynamicRange != videodecoder::DynamicRange::UNKNOWN &&
                !hfpDecoder->supportedDynamicRanges.empty())
            {
                if (std::find(hfpDecoder->supportedDynamicRanges.begin(),
                              hfpDecoder->supportedDynamicRanges.end(),
                              streamConfig.dynamicRange) == hfpDecoder->supportedDynamicRanges.end())
                {
                    reason = "dynamic range unsupported";
                    return false;
                }
            }

            if (streamConfig.colorimetry != videodecoder::Colorimetry::UNKNOWN &&
                !hfpDecoder->supportedColorimetries.empty())
            {
                if (std::find(hfpDecoder->supportedColorimetries.begin(),
                              hfpDecoder->supportedColorimetries.end(),
                              streamConfig.colorimetry) == hfpDecoder->supportedColorimetries.end())
                {
                    reason = "colorimetry unsupported";
                    return false;
                }
            }

            reason.clear();
            return true;
        }

        if (!codecSeen)
        {
            reason = "codec unsupported";
        }
        else if (!pixelFormatSeen)
        {
            reason = "pixel format unsupported";
        }
        else if (!resolutionSeen)
        {
            reason = "resolution exceeds decoder capabilities";
        }
        else if (!frameRateSeen)
        {
            reason = "frame rate exceeds decoder capabilities";
        }
        else
        {
            reason = "stream parameters unsupported";
        }

        return false;
    }

    bool isDecoderCompatibleWithStream(const IVideoDecoder::Id& decoderId,
                                       const videodecoder::Capabilities* capabilities,
                                       const videoDecoderTestConfiguration_t& streamConfig,
                                       std::string& reason) const
    {
        if (capabilities == nullptr)
        {
            std::string hfpReason;
            if (isDecoderCompatibleWithStreamFromHfp(decoderId, streamConfig, hfpReason))
            {
                UT_LOG_WARNING("[DecoderID: %d] Stream '%s' using HFP fallback because decoder getCapabilities is unavailable.",
                               decoderId.value,
                               streamConfig.input.c_str());
                reason.clear();
                return true;
            }

            reason = "decoder capabilities unavailable; HFP fallback failed: " + hfpReason;
            return false;
        }

        const double requestedFrameRate = (streamConfig.frameRateDenominator > 0)
                                              ? static_cast<double>(streamConfig.frameRateNumerator) /
                                                    static_cast<double>(streamConfig.frameRateDenominator)
                                              : 0.0;

        bool codecSeen = false;
        bool pixelFormatCapabilityMissing = false;
        bool pixelFormatSeen = false;
        bool resolutionSeen = false;
        bool frameRateSeen = false;

        for (const auto& codecCaps : capabilities->supportedCodecs)
        {
            if (codecCaps.codec != streamConfig.codec)
            {
                continue;
            }

            codecSeen = true;

            // L3 stream selection is gated by decoder getCapabilities output pixel formats.
            if (codecCaps.supportedOutputPixelFormats.empty())
            {
                pixelFormatCapabilityMissing = true;
                continue;
            }

            if (std::find(codecCaps.supportedOutputPixelFormats.begin(),
                          codecCaps.supportedOutputPixelFormats.end(),
                          streamConfig.pixelFormat) == codecCaps.supportedOutputPixelFormats.end())
            {
                continue;
            }
            pixelFormatSeen = true;

            if ((codecCaps.maxFrameWidth > 0 && streamConfig.codedWidth > codecCaps.maxFrameWidth) ||
                (codecCaps.maxFrameHeight > 0 && streamConfig.codedHeight > codecCaps.maxFrameHeight))
            {
                continue;
            }
            resolutionSeen = true;

            if (codecCaps.maxFrameRate > 0 && requestedFrameRate > static_cast<double>(codecCaps.maxFrameRate))
            {
                continue;
            }
            frameRateSeen = true;

            if (streamConfig.dynamicRange != videodecoder::DynamicRange::UNKNOWN &&
                !capabilities->supportedDynamicRanges.empty() &&
                std::find(capabilities->supportedDynamicRanges.begin(),
                          capabilities->supportedDynamicRanges.end(),
                          streamConfig.dynamicRange) == capabilities->supportedDynamicRanges.end())
            {
                reason = "dynamic range unsupported";
                return false;
            }

            if (streamConfig.colorimetry != videodecoder::Colorimetry::UNKNOWN &&
                !capabilities->supportedColorimetries.empty() &&
                std::find(capabilities->supportedColorimetries.begin(),
                          capabilities->supportedColorimetries.end(),
                          streamConfig.colorimetry) == capabilities->supportedColorimetries.end())
            {
                reason = "colorimetry unsupported";
                return false;
            }

            (void)decoderId;
            reason.clear();
            return true;
        }

        if (!codecSeen)
        {
            reason = "codec unsupported";
        }
        else if (pixelFormatCapabilityMissing && !pixelFormatSeen)
        {
            std::string hfpReason;
            if (isDecoderCompatibleWithStreamFromHfp(decoderId, streamConfig, hfpReason))
            {
                UT_LOG_WARNING("[DecoderID: %d] Stream '%s' decoder getCapabilities missing supportedOutputPixelFormats; using HFP fallback.",
                               decoderId.value,
                               streamConfig.input.c_str());
                reason.clear();
                return true;
            }

            reason = "decoder getCapabilities missing supportedOutputPixelFormats; HFP fallback failed: " + hfpReason;
        }
        else if (!pixelFormatSeen)
        {
            reason = "pixel format unsupported";
        }
        else if (!resolutionSeen)
        {
            reason = "resolution exceeds decoder capabilities";
        }
        else if (!frameRateSeen)
        {
            reason = "frame rate exceeds decoder capabilities";
        }
        else
        {
            reason = "stream parameters unsupported";
        }

        return false;
    }

    bool isSecureModeSupportedByAnyUsableDecoder() const
    {
        for (const auto& [decoderId, decoderHandle, controlListener, eventListener, capabilities] : mVideoDecoderList)
        {
            (void)decoderHandle;
            (void)controlListener;
            (void)eventListener;

            if (!isVideoDecoderUsable(decoderId))
            {
                continue;
            }

            if (isSecureSupportedByDecoder(capabilities))
            {
                return true;
            }
        }

        return false;
    }

    bool isVideoDecoderUsable(const IVideoDecoder::Id& decoderId, bool secureDecode,
                              const videodecoder::Capabilities* capabilities,
                              const videoDecoderTestConfiguration_t& testConfig) const
    {
        std::string unsupportedReason;
        bool isDecoderAvailable = std::find(gAvailableVideodecoders_hfp.begin(),
                                            gAvailableVideodecoders_hfp.end(),
                                            decoderId.value) != gAvailableVideodecoders_hfp.end();
        if (!isDecoderAvailable)
        {
            UT_LOG_WARNING("[DecoderID: %d] is not available, skipping decoder.", decoderId.value);
            return false;
        }

        if (secureDecode && !isSecureSupportedByDecoder(capabilities))
        {
            UT_LOG_WARNING("[DecoderID: %d] Secure decode requested but unsupported, skipping decoder.", decoderId.value);
            return false;
        }

        if (!isDecoderCompatibleWithStream(decoderId, capabilities, testConfig, unsupportedReason))
        {
            UT_LOG_WARNING("[DecoderID: %d] Stream '%s' unsupported (%s), skipping decoder and trying next.",
                           decoderId.value,
                           testConfig.input.c_str(),
                           unsupportedReason.c_str());
            return false;
        }

        return true;
    }

    bool isVideoSinkUsable(const IVideoSink::Id& sinkId) const
    {
        return std::find(gAvailableVideosinks_hfp.begin(), gAvailableVideosinks_hfp.end(), sinkId.value) != gAvailableVideosinks_hfp.end();
    }

    void SetUp() override
    {
        mAVBufferManager = vts_avbuffer_getService();

        if (mAVBufferManager == nullptr) {
                UT_FAIL_FATAL("AVBuffer Service not available");
        }

        if (gVideoDecoderManager == nullptr) {
                UT_FAIL_FATAL("VideoDecoder Service not available");
        }

        if (gVideoSinkManager == nullptr) {
                UT_FAIL_FATAL("VideoSink Service not available");
        }

        vector<IVideoSink::Id> sinkIds;
        vector<IVideoDecoder::Id> videoDecoderIds;

        mAVbufferHelper = getAVBufferHelperInstance();

        if (mAVbufferHelper == nullptr)
        {
            UT_FAIL_FATAL("AVBuffer Helper not available");
        }

        /*Get the Video Sink IDs*/
        VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

        if(getVideoDecoderService() != UT_STATUS_OK)
        {
            UT_LOG_ERROR("getVideoDecoderService failed %d",__LINE__);
        }
        else
        {
            if (gVideodecoders_hfp.empty())
            {
                if (vts_videodecoder_parseVideoDecoderHFP() != UT_STATUS_OK)
                {
                    UT_LOG_WARNING("Failed to parse VideoDecoder HFP capabilities for fallback path.");
                }
            }

            /*Get the Video Decoder IDs*/
            VIDEODECODER_MANAGER_GETVIDEODECODERIDS(videoDecoderIds);

            vector<videodecoder::OperationalMode> supportedModes;
            Status opModeStatus = gVideoDecoderManager->getSupportedOperationalModes(&supportedModes);
            if (opModeStatus.isOk() && opModeStatus.exceptionCode() == Status::EX_NONE)
            {
                mIsTunnelSupported = std::find(supportedModes.begin(), supportedModes.end(), videodecoder::OperationalMode::TUNNELLED) != supportedModes.end();
            }
            else
            {
                mIsTunnelSupported = false;
                UT_LOG_WARNING("Failed to query decoder operational modes. Tunnel tests will be skipped.");
            }

            android::ProcessState::self()->startThreadPool();

            for (const auto& _videoDecoderId : videoDecoderIds)
            {
                sp<IVideoDecoder> videoDecoder;

                std::unique_ptr<videodecoder::Capabilities> capabilitiesOwner(new videodecoder::Capabilities());
                videodecoder::Capabilities* videoDecoderCapabilities = capabilitiesOwner.get();

                sp<VideoDecoderEventListener> videoDecoderEventListener = new VideoDecoderEventListener();

                sp<VideoDecoderControllerListener> videoDecoderControlListener = new VideoDecoderControllerListener();

                /*Get Video Decoder details*/
                VIDEODECODER_MANAGER_GETVIDEODECODER(_videoDecoderId, videoDecoder);

                /*Register the events */
                VIDEODECODER_REGISTER_EVENT_SUCCESS(videoDecoder, videoDecoderEventListener);

                /*Get Capabilities of Video Decoder */
                VIDEODECODER_GETCAPABILITIES(videoDecoder, videoDecoderCapabilities);

                mVideoDecoderList.push_back({_videoDecoderId, std::move(videoDecoder), std::move(videoDecoderControlListener), std::move(videoDecoderEventListener), capabilitiesOwner.release()});
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

            mVideoSinkList.push_back({_sinkId, std::move(videoSink), std::move(videoSinkControlListener), std::move(videoSinkEventListener)});
        }

    }

    bool runSequentialSink(const videoDecoderTestConfiguration_t& testConfig,
                           const videoTestConfiguration_t& sinkConfig,
                           bool secureDecode, bool isTunnel)
    {
        bool executedAtLeastOne = false;
        // Find a usable decoder
        IVideoDecoder::Id decoderId;
        sp<IVideoDecoder> decoderHandle;
        sp<VideoDecoderControllerListener> decoderControlListener;
        sp<VideoDecoderEventListener> decoderEventListener;
        bool foundDecoder = false;

        for (const auto& [_decoderId, _decoderHandle, _controlListener, _eventListener, _capabilities] : mVideoDecoderList)
        {
            std::string unsupportedReason;
            if (!isVideoDecoderUsable(_decoderId, secureDecode, _capabilities.get(), testConfig))
            {
                continue;
            }

            decoderId = _decoderId;
            decoderHandle = _decoderHandle;
            decoderControlListener = _controlListener;
            decoderEventListener = _eventListener;
            foundDecoder = true;
            break;
        }

        if (!foundDecoder)
        {
            UT_LOG_WARNING("No usable decoder found for test configuration '%s', skipping.", testConfig.input.c_str());
            return false;
        }

        try
        {
            for (const auto& [_sinkId, sinkHandle, controlListener, eventListener] : mVideoSinkList)
            {
                if (!isVideoSinkUsable(_sinkId))
                {
                    UT_LOG_INFO("[SinkID: %d] is not usable, skipping.", _sinkId.value);
                    continue;
                }

                IAVClock::Id clockId;
                clockId.value = gVideosinks_hfp.availableClocks_hfp[0];
                bool isSinkAttached = (sinkHandle != nullptr);
                bool isClockInUse = false;

                HeapMetrics beforeHeapMetrics, afterHeapMetrics;
                AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, beforeHeapMetrics);

                videoDecoderTest decodeTest(decoderId, decoderHandle, decoderControlListener,
                                           decoderEventListener, mAVBufferManager, secureDecode,
                                           isTunnel, testConfig, mAVbufferHelper, isSinkAttached);

                std::thread decodeThread(&videoDecoderTest::run, &decodeTest, true);

                VideoSinkTest *sinkTest = new VideoSinkTest(_sinkId, sinkHandle, controlListener, eventListener,
                                                            decoderControlListener, decoderId, clockId, testConfig,
                                                            sinkConfig, isClockInUse);

                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                sinkTest->run(isTunnel);
                executedAtLeastOne = true;

                if (decodeThread.joinable())
                {
                    decodeThread.join();
                }

                decodeTest.closeDecoderSession();

                AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, afterHeapMetrics);
                UT_ASSERT_TRUE(beforeHeapMetrics.secure == afterHeapMetrics.secure);
                UT_ASSERT_TRUE(beforeHeapMetrics.bytesUsed == afterHeapMetrics.bytesUsed);
                UT_ASSERT_TRUE(beforeHeapMetrics.bytesTotal == afterHeapMetrics.bytesTotal);
            }
        } catch (const std::exception& ex) {
            UT_LOG_ERROR("Exception in runSequentialSink: %s", ex.what());
            UT_ASSERT_TRUE(false);
            return false;
        } catch (...) {
            UT_LOG_ERROR("Unknown exception in runSequentialSink");
            UT_ASSERT_TRUE(false);
            return false;
        }

        return executedAtLeastOne;
    }

    bool runConcurrentSink(const videoDecoderTestConfiguration_t& decoderConfig,
                           const videoTestConfiguration_t& sinkConfig,
                           bool secure, bool tunnel)
    {

        bool executedAtLeastOne = false;
        try
        {
            std::vector<std::thread> threads;
            std::vector<std::thread> decoderThreads;
            std::vector<std::unique_ptr<VideoSinkTest>> sinkTests;
            std::vector<std::thread> sinkThreads;
            std::vector<std::unique_ptr<videoDecoderTest>> decoderTests;
            HeapMetrics beforeHeapMetrics, afterHeapMetrics;
            vector<std::tuple<IVideoDecoder::Id, sp<IVideoDecoder>, sp<VideoDecoderControllerListener>, sp<VideoDecoderEventListener>, sp<VideoDecoderCapabilities>>> availableDecoderList;
            vector<std::tuple<IVideoSink::Id, sp<IVideoSink>, sp<VideoSinkControllerListener>, sp<VideoSinkEventListener>>> availableSinkList;
            bool isSinkAttached = true;
            bool isClockInUse = false;

            /* Get heap metrics */
            AVBUFFER_GETMETRICS(mAVBufferManager, secure, beforeHeapMetrics);

            // Collect usable decoders
            for (const auto& entry : mVideoDecoderList)
            {
                const auto& [_decoderId, _dh, _cl, _el, _caps] = entry;
                if (isVideoDecoderUsable(_decoderId, secure, _caps, decoderConfig))
                    availableDecoderList.push_back({_decoderId, _dh, _cl, _el, _caps});
            }

            // Collect usable sinks
            for (const auto& entry : mVideoSinkList)
            {
                const auto& [_sinkId, _, __, ___] = entry;
                if (isSinkUsable(_sinkId, secure))
                    availableSinkList.push_back(entry);
            }

            int32_t availableDecoderCount = availableDecoderList.size();
            int32_t availableSinkCount = availableSinkList.size();
            int32_t availableClockCount = gVideosinks_hfp.availableClocks_hfp.size();
            int32_t numInstances = std::min({availableDecoderCount, availableSinkCount, availableClockCount});
            if (availableDecoderCount == 0)
            {
                UT_ASSERT_TRUE(false);
                UT_LOG_WARNING("No usable decoder found for the given test configuration, skipping concurrent test.");
                return false;
            }
            if (availableSinkCount == 0)
            {
                UT_ASSERT_TRUE(false);
                UT_LOG_WARNING("No usable sink found for the given test configuration, skipping concurrent test.");
                return false;
            }
            if (availableClockCount == 0)
            {
                UT_ASSERT_TRUE(false);
                UT_LOG_WARNING("No available clock found for the given test configuration, skipping concurrent test.");
                return false;
            }

            for(int32_t i = 0; i < numInstances; ++i)
            {
                auto& [_sinkId, videoSink, videoSinkControlListener, videoSinkEventListener] = availableSinkList[i];
                auto& [_decoderId, decoderHandle, decoderControlListener, decoderEventListener] = availableDecoderList[i];

                IAVClock::Id clockId;
                clockId.value = gVideosinks_hfp.availableClocks_hfp[i];

                auto sinkTest = std::make_unique<VideoSinkTest>(_sinkId, videoSink,
                                    videoSinkControlListener, videoSinkEventListener,
                                    decoderControlListener, _decoderId, clockId,
                                    decoderConfig, sinkConfig, isClockInUse);

                sinkThreads.emplace_back(&VideoSinkTest::run, sinkTest.get(), tunnel);
                sinkTests.push_back(std::move(sinkTest));
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            for (int32_t i = 0; i < numInstances; ++i)
            {
                auto& [_decoderId, decoderHandle, decoderControlListener, decoderEventListener] = availableDecoderList[i];

                auto decoderTest = make_unique<videoDecoderTest>(_decoderId, decoderHandle,
                        sp<VideoDecoderControllerListener>::cast(decoderControlListener),
                        sp<VideoDecoderEventListener>::cast(decoderEventListener),
                        sp<IAVBuffer>::cast(mAVBufferManager),
                        secure, tunnel, decoderConfig, mAVbufferHelper, isSinkAttached);

                decoderThreads.emplace_back(&videoDecoderTest::run, decoderTest.get(), true);

                decoderTests.push_back(move(decoderTest));

                availableDecoderList.push_back({
                                                _decoderId,
                                                decoderHandle,
                                                sp<VideoDecoderControllerListener>::cast(decoderControlListener),
                                                sp<VideoDecoderEventListener>::cast(decoderEventListener)
                                            });
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

            for (auto& decoderTest : decoderTests)
            {
                decoderTest->closeDecoderSession();
            }

            decoderTests.clear();
            sinkTests.clear();

            /* Get heap metrics */
            AVBUFFER_GETMETRICS(mAVBufferManager, secure, afterHeapMetrics);

            /* check if the decoder freed all the buffer pools */
            UT_ASSERT_TRUE(beforeHeapMetrics.secure == afterHeapMetrics.secure);
            UT_ASSERT_TRUE(beforeHeapMetrics.bytesUsed == afterHeapMetrics.bytesUsed);
            UT_ASSERT_TRUE(beforeHeapMetrics.bytesTotal == afterHeapMetrics.bytesTotal);
        } catch (const std::exception& ex) {
            UT_LOG_ERROR("Exception in runConcurrentSink: %s", ex.what());
            UT_ASSERT_TRUE(false);
            return false;
        } catch (...) {
            UT_LOG_ERROR("Unknown exception in runConcurrentSink");
            UT_ASSERT_TRUE(false);
            return false;
        }

        return true;
    }

    void runSingleProcessTest(const char *testName, bool secureMode, bool isTunnel, bool concurrentMode)
    {
        vector<videoDecoderTestConfiguration_t> decoderConfigs;
        vector<videoTestConfiguration_t> sinkConfigs;

        if (secureMode && !isSecureModeSupportedByAnyUsableDecoder())
        {
            GTEST_SKIP() << "Secure mode is not supported by any decoder";
            return;
        }

        if (isTunnel && !mIsTunnelSupported)
        {
            GTEST_SKIP() << "Tunnel mode is not supported by decoder capabilities";
            return;
        }

        vts_videodecoder_parse_testconfig(gVideoSinkTestConfiguration, VIDEODECODERMGR, gTestGroup, testName, decoderConfigs);

        vts_videosink_parse_testconfig(gVideoSinkTestConfiguration, VIDEODECODERMGR, gTestGroup, testName, sinkConfigs);

        if (decoderConfigs.empty())
        {
            std::string errorMsg = std::string("No valid test configurations found - missing test streams or invalid configuration for test: ") + testName;
            UT_FAIL_FATAL(errorMsg.c_str());
            return;
        }

        bool executedAnyConfig = false;

        for (size_t i = 0; i < decoderConfigs.size(); ++i)
        {
            auto& decoderConfig = decoderConfigs[i];
            auto& sinkConfig = sinkConfigs[i];

            if (concurrentMode)
            {
                executedAnyConfig = runConcurrentSink(decoderConfig, sinkConfig, secureMode, isTunnel) || executedAnyConfig;
            }
            else
            {
                executedAnyConfig = runSequentialSink(decoderConfig, sinkConfig, secureMode, isTunnel) || executedAnyConfig;
            }
        }

        if (!executedAnyConfig)
        {
            GTEST_SKIP() << "No usable decoder/sink combination for this test configuration (capability filtered)";
        }
    }

    void TearDown() override
    {
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
 * @brief Test back-pressure behavior and onFrameBufferAvailable callback semantics
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 001A@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_NonSecure_FrameBufferAvailable)
{
    const char *testName = "Sink_FrameBufferAvailable";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test back-pressure callback when queue fills without AVClock, then AVClock is attached.
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 001B@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, Sink_Sequential_NonTunnel_NonSecure_FrameBufferAvailable_NoClockAttach)
{
    const char *testName = "Sink_FrameBufferAvailable_NoClockAttach";
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

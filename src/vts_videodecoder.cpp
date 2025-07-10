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
 * @defgroup VTS_L3_VIDEODECODER Video Sink HAL Tests L3 File
 * @{
 * @parblock
 *
 * ### VIDEO DECODE BUFFER CALLS FOR L3:
 *
 * This module includes video decoder calls to decode buffers *
 * **Pre-Conditions:** @n
 * **Dependencies:** None@n
 *
 * @endparblock
 *
 */

/**
 * @file vts_l3_videodecoder.cpp
 *
 */
#include "vts_common.h"
#include "vts_videodecoder.h"
#include "vts_avbuffer.h"

using namespace std;
using namespace android;
using namespace com::rdk::hal;
using namespace com::rdk::hal::videodecoder;
using namespace com::rdk::hal::avbuffer;
using com::rdk::hal::State;
using com::rdk::hal::videodecoder::ErrorCode;
using android::binder::Status;
using com::rdk::hal::videodecoder::IVideoDecoder;
using com::rdk::hal::videodecoder::IVideoDecoderControllerListener;
using com::rdk::hal::videodecoder::IVideoDecoderEventListener;
using com::rdk::hal::avbuffer::IAVBuffer;
using android::sp;

// Definition of global service manager pointer
sp<IVideoDecoderManager> gVideoDecoderManager;
extern IAVBufferHelper* gAVbufferHelper;

// Function to get video decoder service
UT_status_t getVideoDecoderService() {
    gVideoDecoderManager = android::interface_cast<IVideoDecoderManager>(
        android::defaultServiceManager()->getService(
            android::String16(IVideoDecoderManager::serviceName().c_str()))
    );

    if (gVideoDecoderManager == nullptr) {
        printf("Service not available\n");
        return UT_STATUS_FAILURE;
    }
    return UT_STATUS_OK;
}

// VideoDecoderControllerListener implementation
VideoDecoderControllerListener::VideoDecoderControllerListener() = default;
VideoDecoderControllerListener::~VideoDecoderControllerListener() = default;

Status VideoDecoderControllerListener::onFrameOutput(int64_t nsPresentationTime, int64_t frameBufferHandle, const std::optional<FrameMetadata>& metadata)
{

    UT_LOG_DEBUG("onFrameOutput is received nsPresentationTime %lld FrameBufferHandle = 0x%llx ", nsPresentationTime, frameBufferHandle);
    FrameMetadata metaCopy;  // Default-constructed

    if (metadata.has_value()) {
        const FrameMetadata& frameMeta = metadata.value();

        UT_LOG_DEBUG("FrameMetadata:");
        UT_LOG_DEBUG("  Pixel Aspect Ratio: %d:%d", frameMeta.parX, frameMeta.parY);
        UT_LOG_DEBUG("  Source Aspect Ratio: %d:%d", frameMeta.sarX, frameMeta.sarY);
        UT_LOG_DEBUG("  Coded Dimensions: width=%d, height=%d", frameMeta.codedWidth, frameMeta.codedHeight);
        UT_LOG_DEBUG("  Active Display: x=%d, y=%d, width=%d, height=%d",
                    frameMeta.activeX, frameMeta.activeY, frameMeta.activeWidth, frameMeta.activeHeight);
        UT_LOG_DEBUG("  Color Depth: %d bits", frameMeta.colorDepth);
        UT_LOG_DEBUG("  Pixel Format: %d", static_cast<int>(frameMeta.pixelFormat));
        UT_LOG_DEBUG("  Dynamic Range: %d", static_cast<int>(frameMeta.dynamicRange));
        UT_LOG_DEBUG("  Scan Type: %d", static_cast<int>(frameMeta.scanType));
        UT_LOG_DEBUG("  AFD: %d", frameMeta.afd);
        UT_LOG_DEBUG("  Frame Rate: %d/%d", frameMeta.frameRateNumerator, frameMeta.frameRateDenominator);
        UT_LOG_DEBUG("  End of Stream: %d", frameMeta.endOfStream);
        UT_LOG_DEBUG("  Discontinuity: %d", frameMeta.discontinuity);
        UT_LOG_DEBUG("  Low Latency: %d", frameMeta.lowLatency);
        UT_LOG_DEBUG("  Source: %d", static_cast<int>(frameMeta.source));

        if (!frameMeta.sha1.empty()) {
            std::string sha1_hex;
            for (const auto& byte : frameMeta.sha1) {
                char buf[3];
                snprintf(buf, sizeof(buf), "%02x", static_cast<unsigned char>(byte));
                sha1_hex += buf;
            }
            UT_LOG_DEBUG("  SHA1: %s", sha1_hex.c_str());
        } else {
            UT_LOG_DEBUG("  SHA1: (empty)");
        }

        // Copy only the safe parts of metadata
        metaCopy.parX = frameMeta.parX;
        metaCopy.parY = frameMeta.parY;
        metaCopy.sarX = frameMeta.sarX;
        metaCopy.sarY = frameMeta.sarY;
        metaCopy.codedWidth = frameMeta.codedWidth;
        metaCopy.codedHeight = frameMeta.codedHeight;
        metaCopy.activeX = frameMeta.activeX;
        metaCopy.activeY = frameMeta.activeY;
        metaCopy.activeWidth = frameMeta.activeWidth;
        metaCopy.activeHeight = frameMeta.activeHeight;
        metaCopy.colorDepth = frameMeta.colorDepth;
        metaCopy.pixelFormat = frameMeta.pixelFormat;
        metaCopy.dynamicRange = frameMeta.dynamicRange;
        metaCopy.scanType = frameMeta.scanType;
        metaCopy.afd = frameMeta.afd;
        metaCopy.frameRateNumerator = frameMeta.frameRateNumerator;
        metaCopy.frameRateDenominator = frameMeta.frameRateDenominator;
        metaCopy.endOfStream = frameMeta.endOfStream;
        metaCopy.discontinuity = frameMeta.discontinuity;
        metaCopy.lowLatency = frameMeta.lowLatency;
        metaCopy.source = frameMeta.source;
        metaCopy.sha1 = frameMeta.sha1;

    } else {
        UT_LOG_DEBUG("No FrameMetadata provided, using default metadata.");
    }

    FrameData frame(nsPresentationTime, frameBufferHandle, std::move(metaCopy));
    gFrameQueue.push(std::move(frame));

    UT_LOG_DEBUG("Pushed frame to gFrameQueue: PTS = %lld, Handle = 0x%llx", nsPresentationTime, frameBufferHandle);
    return Status::ok();
}

Status VideoDecoderControllerListener::onUserDataOutput(int64_t nsPresentationTime, const std::vector<uint8_t>& userData)
{
    UT_LOG_DEBUG("onUserDataOutput: PTS = %lx, UserDataSize = %zu", nsPresentationTime, userData.size());
    if (!userData.empty()) {
        UT_LOG_DEBUG("First byte of user data: 0x%02X", userData[0]);
    }
    return Status::ok();
}

// VideoDecoderEventListener implementation
VideoDecoderEventListener::VideoDecoderEventListener() : mCurrentState(State::CLOSED) {}
VideoDecoderEventListener::~VideoDecoderEventListener() = default;

Status VideoDecoderEventListener::onDecodeError(ErrorCode errorCode, int32_t vendorErrorCode)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mErrors.push_back(static_cast<int32_t>(errorCode));
    return Status::ok();
}

Status VideoDecoderEventListener::onStateChanged(State oldState, State newState)
{
    UT_LOG_DEBUG("State Change received: %s -> %s", toString(oldState).c_str(), toString(newState).c_str());
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mStateChanges.emplace_back(oldState, newState);
        mCurrentState = newState;
    }
    mCV.notify_all();
    return Status::ok();
}

bool VideoDecoderEventListener::waitForState(State desiredState, std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mMutex);
    return mCV.wait_for(lock, timeout, [this, desiredState]() {
        return mCurrentState == desiredState;
    });
}

std::vector<std::pair<State, State>> VideoDecoderEventListener::getStateChanges() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mStateChanges;
}

void VideoDecoderEventListener::resetStateChanges()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mStateChanges.clear();
}

bool VideoDecoderEventListener::checkStateTransistion(State expOldState, State expNewState)
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (const auto& [oldState, newState] : mStateChanges) {
        if (oldState == expOldState && newState == expNewState) {
            return true;
        }
    }
    return false;
}

std::vector<int32_t> VideoDecoderEventListener::getErrors() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mErrors;
}

// Optional: you need a toString function for logging states (if not already present)
static std::string toString(State state)
{
    switch(state) {
        case State::UNKNOWN:   return "UNKNOWN";
        case State::CLOSED:    return "CLOSED";
        case State::OPENING:   return "OPENING";
        case State::READY:     return "READY";
        case State::STARTING:  return "STARTING";
        case State::STARTED:   return "STARTED";
        case State::FLUSHING:  return "FLUSHING";
        case State::STOPPING:  return "STOPPING";
        case State::CLOSING:   return "CLOSING";
        default:               return "INVALID_STATE";
    }
}


DynamicRange videoDecoderStringToDynamicRange(const std::string& rangeStr) {
    static const std::unordered_map<std::string, DynamicRange> dynamicRangeMap = {
        {"UNKNOWN", DynamicRange::UNKNOWN},
        {"SDR", DynamicRange::SDR},
        {"HLG", DynamicRange::HLG},
        {"HDR10", DynamicRange::HDR10},
        {"HDR10PLUS", DynamicRange::HDR10PLUS},
        {"DOLBY_VISION", DynamicRange::DOLBY_VISION}
    };
    auto it = dynamicRangeMap.find(rangeStr);
    return (it != dynamicRangeMap.end()) ? it->second : DynamicRange::UNKNOWN;
}

Codec videoDecoderStringToCodec(const std::string& codecStr) {
    static const std::unordered_map<std::string, Codec> codecMap = {
        {"MPEG", Codec::MPEG},
        {"H264", Codec::H264},
        {"H265", Codec::H265},
        {"VP9",  Codec::VP9},
        {"AV1",  Codec::AV1}
    };
    auto it = codecMap.find(codecStr);
    if (it != codecMap.end()) {
        return it->second;
    } else {
        throw std::invalid_argument("Unknown codec: " + codecStr);
    }
}

PixelFormat videoDecoderStringToPixelFormat(const std::string& formatStr) {
    static const std::unordered_map<std::string, PixelFormat> pixelFormatMap = {
        {"YCBCR422", PixelFormat::YCBCR422},
        {"YCBCR444", PixelFormat::YCBCR444},
        {"NV12", PixelFormat::YCBCR420},
        {"RGB", PixelFormat::RGB},
        {"NATIVE", PixelFormat::NATIVE}
    };
    auto it = pixelFormatMap.find(formatStr);
    if (it != pixelFormatMap.end()) {
        return it->second;
    }
    return static_cast<PixelFormat>(0); // Unknown format
}

size_t vts_bytesPerPixel(PixelFormat format) {
    switch (format) {
        case PixelFormat::YCBCR420: return 1.5; // Y + 0.25*Cb + 0.25*Cr
        case PixelFormat::YCBCR422: return 2;   // Y + 0.5*Cb + 0.5*Cr
        case PixelFormat::YCBCR444: return 3;   // Y + Cb + Cr
        case PixelFormat::RGB: return 3;        // R + G + B
        case PixelFormat::NATIVE: return 3;     // Assume 3 bytes
        default: return 1;
    }
}

UT_status_t vts_videodecoder_parse_testconfig(void* pTestInstance, const char *module, const char *testGroup, const char *testName, vector<testConfiguration_t>& testConfiguration)
{
    ut_kvp_status_t status;
    ut_kvp_instance_t* kvpTestInstance = (ut_kvp_instance_t*)pTestInstance;
    std::string streamInputPath;
    std::string streamRefPath;
    int32_t streamCount = 0;
    filesystem::path currentPath = filesystem::current_path();
    filesystem::path fullPath;
    string testList = string(module) + "." + string(testGroup) + "." + string(testName);
    string controlList = testList + "." + "Controls";
    string streamList = testList + "." + "Inputs" + "." + "Streams";
    string streamsDetails = string("VideoStreams");

    // Get input and reference paths
    char tempBuffer[UT_KVP_MAX_ELEMENT_SIZE] = {0};
    status = ut_kvp_getStringField(kvpTestInstance, "IVideoDecoderManager.input_streams_path", tempBuffer, UT_KVP_MAX_ELEMENT_SIZE);
    if (status != UT_KVP_STATUS_SUCCESS)
    {
        UT_LOG_ERROR("Failed to get input_streams_path, status: %d", status);
        return UT_STATUS_FAILURE;
    }
    streamInputPath = tempBuffer;
    UT_LOG_DEBUG("streamInputPath: %s", streamInputPath.c_str());

    status = ut_kvp_getStringField(kvpTestInstance, "IVideoDecoderManager.reference_streams_path", tempBuffer, UT_KVP_MAX_ELEMENT_SIZE);
    if (status != UT_KVP_STATUS_SUCCESS)
    {
        UT_LOG_ERROR("Failed to get reference_streams_path, status: %d", status);
        return UT_STATUS_FAILURE;
    }
    streamRefPath = tempBuffer;
    UT_LOG_DEBUG("streamRefPath: %s", streamRefPath.c_str());

    streamCount = ut_kvp_getListCount(kvpTestInstance, streamList.c_str());
    if (streamCount <= 0)
    {
        UT_LOG_ERROR("No streams found in %s", streamList.c_str());
        return UT_STATUS_FAILURE;
    }
    UT_LOG_DEBUG("Found %d streams", streamCount);

    for (int32_t i = 0; i < streamCount; i++)
    {
        std::string resultKVP;
        testConfiguration_t testDetails;
        vector<int32_t> errorFrameNum;

        string streamName = streamList + "." + to_string(i) + ".Name";
        status = ut_kvp_getStringField(kvpTestInstance, streamName.c_str(), tempBuffer, UT_KVP_MAX_ELEMENT_SIZE);
        if (status != UT_KVP_STATUS_SUCCESS)
        {
            UT_LOG_ERROR("Failed to get stream name for index %d, status: %d", i, status);
            continue;
        }
        resultKVP = tempBuffer;
        UT_LOG_DEBUG("Stream name: %s", resultKVP.c_str());
        testDetails.streamName = resultKVP.c_str();

        string prefix = streamsDetails + "." + resultKVP + ".";
        string input = prefix + "input";
        string reference = prefix + "reference";
        string codec = prefix + "codec";
        string codedWidth = prefix + "codedWidth";
        string codedHeight = prefix + "codedHeight";
        string frameRateNumerator = prefix + "frameRateNumerator";
        string frameRateDenominator = prefix + "frameRateDenominator";
        string pixelFormat = prefix + "pixelFormat";
        string dynamicRange = prefix + "dynamic_range";
        string colorDepth = prefix + "colorDepth";
        string bitRate = prefix + "bit_rate";
        string noOfFrames = prefix + "no_of_frames";
        string frameSize = prefix + "frame_size";
        string errorFrames = prefix + "error_frames";
        string framePTS = prefix  + ".PTS";
        string rmsDiff = streamList + "." + to_string(i) + ".rms_diff_threshold";
        string decodeTime = streamList + "." + to_string(i) + ".decode_time_threshold";
        string decoderDelay = controlList + ".delay_between_decode_calls";
        string stopPts = prefix + "stop_start_pts";
        string discontinuityPts = prefix + "signal_discontinuity_pts";
        string flushPts = prefix + "flush_pts";
        string flushResetPts = prefix + "flush_reset_pts";
        string eosPts = prefix + "eos_pts";
        string sink_discardFramesUntilPts = streamList + "." + to_string(i) + ".sink_discardFramesUntilPts";
        string sink_underflowPts         = streamList + "." + to_string(i) + ".sink_underflow_pts";
        string sink_resumeTriggerPts      = streamList + "." + to_string(i) + ".sink_resumeTriggerPts";
        string sink_repeatPerFrame        = streamList + "." + to_string(i) + ".sink_repeatPerFrame";
        string sink_flush_pts             = streamList + "." + to_string(i) + ".sink_flush_pts";
        string sink_flush_reset_pts       = streamList + "." + to_string(i) + ".sink_flush_reset_pts";
        string sink_eos_pts               = streamList + "." + to_string(i) + ".sink_eos_pts";
        string sink_interFrameDelayMs     = streamList + "." + to_string(i) + ".sink_interFrameDelayMs";
        string sink_discontinuityPTS      = streamList + "." + to_string(i) + ".sink_discontinuityPTS";

        // Input stream path
        status = ut_kvp_getStringField(kvpTestInstance, input.c_str(), tempBuffer, UT_KVP_MAX_ELEMENT_SIZE);
        if (status == UT_KVP_STATUS_SUCCESS)
        {
            resultKVP = tempBuffer;
            fullPath = filesystem::path(streamInputPath) / resultKVP;
            UT_LOG_DEBUG("Full input file path: %s", fullPath.string().c_str());

            if (!filesystem::exists(fullPath))
            {
                UT_LOG_ERROR("Input file does not exist: %s", fullPath.string().c_str());
                continue;
            }

            testDetails.input = fullPath.string();
            UT_LOG_DEBUG("Input path set: %s", testDetails.input.c_str());
        }
        else
        {
            UT_LOG_ERROR("Failed to get input path for stream %s, status: %d", resultKVP.c_str(), status);
            continue;
        }

        // Codec
        status = ut_kvp_getStringField(kvpTestInstance, codec.c_str(), tempBuffer, UT_KVP_MAX_ELEMENT_SIZE);
        if (status == UT_KVP_STATUS_SUCCESS)
        {
            testDetails.codec = videoDecoderStringToCodec(string(tempBuffer));
        }

        // Resolution
        testDetails.codedWidth = ut_kvp_getUInt32Field(kvpTestInstance, codedWidth.c_str());
        testDetails.codedHeight = ut_kvp_getUInt32Field(kvpTestInstance, codedHeight.c_str());

        // Frame rate
        testDetails.frameRateNumerator = ut_kvp_getUInt32Field(kvpTestInstance, frameRateNumerator.c_str());
        testDetails.frameRateDenominator = ut_kvp_getUInt32Field(kvpTestInstance, frameRateDenominator.c_str());

        // Pixel format
        status = ut_kvp_getStringField(kvpTestInstance, pixelFormat.c_str(), tempBuffer, UT_KVP_MAX_ELEMENT_SIZE);
        if (status == UT_KVP_STATUS_SUCCESS)
        {
            testDetails.pixelFormat = videoDecoderStringToPixelFormat(string(tempBuffer));
        }

        // Dynamic range
        status = ut_kvp_getStringField(kvpTestInstance, dynamicRange.c_str(), tempBuffer, UT_KVP_MAX_ELEMENT_SIZE);
        if (status == UT_KVP_STATUS_SUCCESS)
        {
            testDetails.dynamicRange = videoDecoderStringToDynamicRange(string(tempBuffer));
        }

        // Color depth
        testDetails.colorDepth = ut_kvp_getUInt32Field(kvpTestInstance, colorDepth.c_str());

        // Bit rate
        testDetails.bitRate = ut_kvp_getUInt32Field(kvpTestInstance, bitRate.c_str());

        // Number of frames
        testDetails.no_of_frames = ut_kvp_getUInt32Field(kvpTestInstance, noOfFrames.c_str());

        // Error frames
        if (ut_kvp_fieldPresent(kvpTestInstance, errorFrames.c_str()))
        {
            int32_t count = ut_kvp_getListCount(kvpTestInstance, errorFrames.c_str());
            for (int32_t j = 0; j < count; j++)
            {
                string error = errorFrames + "." + to_string(j);
                int32_t frameNum = ut_kvp_getUInt32Field(kvpTestInstance, error.c_str());
                if (frameNum >= 1 && frameNum <= testDetails.no_of_frames)
                {
                    errorFrameNum.push_back(frameNum);
                }
                else
                {
                    UT_LOG_WARNING("Invalid error frame index: %d, max: %d", frameNum, testDetails.no_of_frames);
                }
            }
        }

        // Frame details
        int32_t frameCount = ut_kvp_getListCount(kvpTestInstance, frameSize.c_str());

        // this check is not required as no_of_frames and size,pts are different
        /*if (frameCount != testDetails.no_of_frames || frameCount != ut_kvp_getListCount(kvpTestInstance, framePTS.c_str()))
        {
            UT_LOG_ERROR("Frame size/PTS count mismatch: frames=%d, sizes=%d, PTS=%d",
                         testDetails.no_of_frames, frameCount, ut_kvp_getListCount(kvpTestInstance, framePTS.c_str()));
            continue;
        }*/
        for (int32_t j = 0; j < frameCount; j++)
        {
            string eachFrame = frameSize + "." + to_string(j);
            string eachPTS = framePTS + "." + to_string(j);
            int32_t size = ut_kvp_getUInt32Field(kvpTestInstance, eachFrame.c_str());
            int64_t PTS = ut_kvp_getUInt64Field(kvpTestInstance, eachPTS.c_str());
            bool errorFrame = (find(errorFrameNum.begin(), errorFrameNum.end(), j + 1) != errorFrameNum.end());
            testDetails.frameDetails.push_back({size, PTS, errorFrame});
        }

        // RMS difference threshold
        testDetails.rms_diff_threshold = ut_kvp_getDoubleField(kvpTestInstance, rmsDiff.c_str());

        // Decode time threshold
        testDetails.decode_time_threshold = ut_kvp_getUInt64Field(kvpTestInstance, decodeTime.c_str());

        // Decoder delay
        testDetails.decoderDelay = ut_kvp_getUInt32Field(kvpTestInstance, decoderDelay.c_str());

        // Control points
        testDetails.stopPts = ut_kvp_getUInt64Field(kvpTestInstance, stopPts.c_str());
        testDetails.discontinuityPts = ut_kvp_getUInt64Field(kvpTestInstance, discontinuityPts.c_str());
        testDetails.flushPts = ut_kvp_getUInt64Field(kvpTestInstance, flushPts.c_str());
        testDetails.flushResetPts = ut_kvp_getUInt64Field(kvpTestInstance, flushResetPts.c_str());
        testDetails.eosPts = ut_kvp_getUInt64Field(kvpTestInstance, eosPts.c_str());
        testDetails.sink_discardFramesUntilPts = ut_kvp_getUInt64Field(kvpTestInstance, sink_discardFramesUntilPts.c_str());
        testDetails.sink_underflowPts          = ut_kvp_getUInt64Field(kvpTestInstance, sink_underflowPts.c_str());
        testDetails.sink_resumeTriggerPts      = ut_kvp_getUInt64Field(kvpTestInstance, sink_resumeTriggerPts.c_str());
        testDetails.sink_repeatPerFrame        = ut_kvp_getUInt64Field(kvpTestInstance, sink_repeatPerFrame.c_str());
        testDetails.sink_flush_pts             = ut_kvp_getUInt64Field(kvpTestInstance, sink_flush_pts.c_str());
        testDetails.sink_flush_reset_pts       = ut_kvp_getUInt64Field(kvpTestInstance, sink_flush_reset_pts.c_str());
        testDetails.sink_eos_pts               = ut_kvp_getUInt64Field(kvpTestInstance, sink_eos_pts.c_str());
        testDetails.sink_interFrameDelayMs     = ut_kvp_getUInt64Field(kvpTestInstance, sink_interFrameDelayMs.c_str());
        testDetails.sink_discontinuityPTS      = ut_kvp_getUInt64Field(kvpTestInstance, sink_discontinuityPTS.c_str());
        testDetails.sink_testName              = testName;

        testConfiguration.push_back(testDetails);
    }

    return UT_STATUS_OK;
}

DecoderTest::DecoderTest(IVideoDecoder::Id decoderId,
                         sp<IVideoDecoder> decoderHandle,
                         sp<VideoDecoderControllerListener> controlListener,
                         sp<VideoDecoderEventListener> eventListener,
                         sp<IAVBuffer> AVBufferManager,
                         bool secureDecode,
                         bool tunnelMode,
                         testConfiguration_t& config)
    : mDecoderId(decoderId),
      mDecoderHandle(decoderHandle),
      mControlListener(controlListener),
      mEventListener(eventListener),
      mAVBufferManager(AVBufferManager),
      mSecureDecode(secureDecode),
      mTunnelMode(tunnelMode),
      mTestConfiguration(config),
      mDecodingStream(false)
{}

void DecoderTest::decodeAndConsumeVideoStreams()
{
    sp<IVideoDecoderController> videoDecoderController;
    int64_t videoBufferHandle;
    Pool videoPool;
    sp<IAVBufferSpaceListener> bufferListener = new AVBufferSpaceListener();

    AVBUFFER_CREATE_VIDEOPOOL_SUCCESS(mAVBufferManager, mSecureDecode, mDecoderId, bufferListener, videoPool);

    ifstream inputFile(mTestConfiguration.input, ios::binary);
    if (!inputFile)
    {
        UT_LOG_ERROR("Failed to open file: %s", mTestConfiguration.input.c_str());
        return;
    }

    VIDEODECODER_OPEN_SUCCESS(mDecoderHandle, mTestConfiguration.codec, mSecureDecode, mControlListener, videoDecoderController, true);
    VIDEODECODER_START_SUCCESS(mDecoderHandle, videoDecoderController, true);

    for (const auto& [frameSize, framePTS, errorFrame] : mTestConfiguration.frameDetails)
    {
        vector<uint8_t> inputBuffer;
        size_t output_buffer_size;
        double rms_diff = 0.0;
        int32_t trimEndNs = 0;
        int32_t trimStartNs = 0;
        int32_t startFramesDecoded = 0, startFrameErrors = 0, startFramesDropped = 0;
        int32_t endFramesDecoded = 0, endFrameErrors = 0, endFramesDropped = 0;
        DecodedVideoFrame_t decodedFrame;
        chrono::steady_clock::time_point startTime;

        AVBUFFER_ALLOC_SUCCESS(mAVBufferManager, videoPool, frameSize, videoBufferHandle);

        inputBuffer.resize(frameSize);
        inputFile.read(reinterpret_cast<char*>(inputBuffer.data()), frameSize);

        startTime = chrono::steady_clock::now();
        VIDEODECODER_DECODEBUFFER_SUCCESS(videoDecoderController, framePTS, videoBufferHandle);

        // mControlListener->getNextFrame(decodedFrame);

        if (!mTunnelMode && !decodedFrame.data.empty())
        {
            output_buffer_size = decodedFrame.metadata.width * decodedFrame.metadata.height * vts_bytesPerPixel(decodedFrame.metadata.pixelFormat);
            // rms_diff = vts_computeVideoRMSDiff(...);
            UT_ASSERT_TRUE(rms_diff <= mTestConfiguration.rms_diff_threshold);
        }

        auto duration = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - startTime);
        UT_LOG_DEBUG("Decode Time [PTS: %lld]: %lld", framePTS, duration.count());

        startFramesDecoded++;
        if (errorFrame) startFrameErrors++;

        UT_LOG_DEBUG("Number of frames Decoded %d Error Frames %d startFramesDropped %d", startFramesDecoded, startFrameErrors, startFramesDropped);

        AVBUFFER_FREE_SUCCESS(mAVBufferManager, videoBufferHandle);
    }

    VIDEODECODER_STOP_SUCCESS(mDecoderHandle, videoDecoderController, true);
    VIDEODECODER_CLOSE_SUCCESS(mDecoderHandle, videoDecoderController, true);
    AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, videoPool);
}

void DecoderTest::decoderThreadFunc()
{
    sp<IVideoDecoderController> videoDecoderController;
    int64_t videoBufferHandle = 0;
    Pool videoPool;
    sp<IAVBufferSpaceListener> bufferListener = new AVBufferSpaceListener();
    mDecodingStream = true;

    UT_LOG_DEBUG("[PID: %d, TID: %d] decoderThreadFunc Start", getpid(), syscall(SYS_gettid));

    // Validate gAVbufferHelper
    if (!gAVbufferHelper) {
        UT_LOG_ERROR("gAVbufferHelper is null");
        mDecodingStream = false;
        return;
    }
    UT_LOG_DEBUG("gAVbufferHelper initialized: %p, secureDecode: %d", gAVbufferHelper, mSecureDecode);

    // Create Video Pool
    {
        std::lock_guard<std::mutex> lock(mDecodeMutex);
        UT_LOG_DEBUG("Calling createVideoPool(secureHeap:%d, videoDecoderId:%d, listener:%p)",
                    mSecureDecode, mDecoderId, bufferListener.get());
        AVBUFFER_CREATE_VIDEOPOOL_SUCCESS(mAVBufferManager, mSecureDecode, mDecoderId, bufferListener, videoPool);
        UT_LOG_DEBUG("Created video pool with handle:%lld", videoPool.handle);
    }

    // Validate and read input file
    if (!std::filesystem::exists(mTestConfiguration.input)) {
        UT_LOG_ERROR("Input file does not exist: %s", mTestConfiguration.input.c_str());
        mDecodingStream = false;
        UT_LOG_DEBUG("Destroying video pool handle:%lld", videoPool.handle);
        AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, videoPool);

        return;
    }
    std::ifstream inputFile(mTestConfiguration.input, std::ios::binary);
    if (!inputFile.is_open()) {
        UT_LOG_ERROR("Failed to open file: %s", mTestConfiguration.input.c_str());
        mDecodingStream = false;
        UT_LOG_DEBUG("Destroying video pool handle:%lld", videoPool.handle);
        AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, videoPool);

        return;
    }

    inputFile.seekg(0, std::ios::end);
    auto fileSizePos = inputFile.tellg();
    if (fileSizePos < 0) {
        UT_LOG_ERROR("Failed to get file size for %s: tellg returned %lld",
                    mTestConfiguration.input.c_str(), static_cast<int64_t>(fileSizePos));
        mDecodingStream = false;
        UT_LOG_DEBUG("Destroying video pool handle:%lld", videoPool.handle);
        AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, videoPool);

        return;
    }
    uint64_t fileSize = static_cast<uint64_t>(fileSizePos);
    if (fileSize == 0) {
        UT_LOG_ERROR("Input file is empty: %s", mTestConfiguration.input.c_str());
        mDecodingStream = false;
        UT_LOG_DEBUG("Destroying video pool handle:%lld", videoPool.handle);
        AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, videoPool);

        return;
    }
    if (fileSize > std::numeric_limits<uint32_t>::max()) {
        UT_LOG_ERROR("File size too large: %llu bytes, max allowed: %u bytes",
                    fileSize, std::numeric_limits<uint32_t>::max());
        mDecodingStream = false;
        UT_LOG_DEBUG("Destroying video pool handle:%lld", videoPool.handle);
        AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, videoPool);

        return;
    }
    UT_LOG_DEBUG("Input file size: %llu bytes", fileSize);

    std::vector<uint8_t> inputBuffer(fileSize);
    try {
        inputFile.seekg(0, std::ios::beg);
        inputFile.read(reinterpret_cast<char*>(inputBuffer.data()), fileSize);
        if (!inputFile) {
            UT_LOG_ERROR("Failed to read file data: only %zu bytes read", inputFile.gcount());
            mDecodingStream = false;
            UT_LOG_DEBUG("Destroying video pool handle:%lld", videoPool.handle);
            AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, videoPool);

            return;
        }
        uint64_t actualSize = static_cast<uint64_t>(inputFile.gcount());
        if (actualSize != fileSize) {
            UT_LOG_WARNING("Read %llu bytes, expected %llu bytes", actualSize, fileSize);
            fileSize = actualSize;
        }
    } catch (const std::exception& e) {
        UT_LOG_ERROR("Exception reading file: %s", e.what());
        mDecodingStream = false;
        UT_LOG_DEBUG("Destroying video pool handle:%lld", videoPool.handle);
        AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, videoPool);

        return;
    }

    // Determine frame data based on codec
    std::vector<std::tuple<uint64_t, uint32_t, int64_t, bool>> frameData; // [offset, size, pts, isErrorFrame]
    if (mTestConfiguration.codec == com::rdk::hal::videodecoder::Codec::H264 ||
        mTestConfiguration.codec == com::rdk::hal::videodecoder::Codec::H265) {
        if (mTestConfiguration.frameDetails.empty()) {
            UT_LOG_ERROR("frameDetails is empty for H.264/H.265");
            mDecodingStream = false;
            UT_LOG_DEBUG("Destroying video pool handle:%lld", videoPool.handle);
            AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, videoPool);

            return;
        }
        uint64_t offset = 0;
        for (const auto& [size, pts, isErrorFrame] : mTestConfiguration.frameDetails) {
            if (offset + size > fileSize) {
                UT_LOG_ERROR("Frame size at offset %llu exceeds file size %llu", offset + size, fileSize);
                mDecodingStream = false;
                UT_LOG_DEBUG("Destroying video pool handle:%lld", videoPool.handle);
                AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, videoPool);

                return;
            }
            frameData.emplace_back(offset, size, pts, isErrorFrame);
            UT_LOG_DEBUG("Codec:%d Frame from frameDetails: offset=%llu, size=%u, pts=%lld, errorFrame=%d",
                        mTestConfiguration.codec,offset, size, pts, isErrorFrame);
            offset += size;
        }
    } else {
        // Parse frames for VP9, MPEG, AV1
        auto parsedFrames = parseFrames(inputBuffer, fileSize, mTestConfiguration.codec);
        if (parsedFrames.empty()) {
            UT_LOG_ERROR("No frames parsed for codec: %d", static_cast<int>(mTestConfiguration.codec));
            mDecodingStream = false;
            UT_LOG_DEBUG("Destroying video pool handle:%lld", videoPool.handle);
            AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, videoPool);

            return;
        }
        for (size_t i = 0; i < parsedFrames.size(); ++i) {
            auto [offset, size, pts] = parsedFrames[i];
            bool isErrorFrame = (i < mTestConfiguration.frameDetails.size()) ?
                                std::get<2>(mTestConfiguration.frameDetails[i]) : false;
            frameData.emplace_back(offset, size, pts, isErrorFrame);
            UT_LOG_DEBUG("Parsed frame: Codec:%d offset=%llu, size=%u, pts=%lld, errorFrame=%d",
                        mTestConfiguration.codec,offset, size, pts, isErrorFrame);
        }
    }

    // Open Decoder
    {
        std::lock_guard<std::mutex> lock(mDecodeMutex);
        UT_LOG_DEBUG("Calling open(codec:%d, secure:%d, listener:%p)",
                    mTestConfiguration.codec, mSecureDecode, mControlListener.get());
        VIDEODECODER_OPEN_SUCCESS(mDecoderHandle, mTestConfiguration.codec, mSecureDecode,
                                mControlListener, videoDecoderController, true);
        UT_LOG_DEBUG("Decoder opened, controller:%p", videoDecoderController.get());

        if (mTunnelMode) {
            std::vector<std::pair<Property, int32_t>> startedIntProps = {
                {Property::OPERATIONAL_MODE, static_cast<int32_t>(OperationalMode::TUNNELLED)}
            };
            UT_LOG_DEBUG("Set properties in TUNNELLED Mode");
            VIDEODECODER_SETPROPERTY_LIST_SUCCESS(videoDecoderController, startedIntProps, intValue);
        }
        std::optional<PropertyValue> aidl_return;
        VIDEODECODER_GETPROPERTY(mDecoderHandle, Property::OPERATIONAL_MODE, aidl_return);
    }

    // Start Decoder
    {
        std::lock_guard<std::mutex> lock(mDecodeMutex);
        VIDEODECODER_START_SUCCESS(mDecoderHandle, videoDecoderController, true);
    }

    // Get initial metrics
    int32_t framesDecoded, decodeErrors, framesDropped;
    getDecoderMetrics(framesDecoded, decodeErrors, framesDropped);
    UT_LOG_DEBUG("Initial Decoder Metrics: Codec: %d ,Frames Decoded=%d, Decode Errors=%d, Frames Dropped=%d",
                mTestConfiguration.codec,framesDecoded, decodeErrors, framesDropped);

    // Process Frames
    for (size_t i = 0; i < frameData.size(); ++i) {
        auto [frameOffset, frameSize, framePTS, isErrorFrame] = frameData[i];

        UT_LOG_DEBUG("Processing frame %zu: offset=%llu, size=%u, PTS=%lldns, errorFrame=%d",
                    i, frameOffset, frameSize, framePTS, isErrorFrame);

        // Allocate Buffer
        {
            std::lock_guard<std::mutex> lock(mDecodeMutex);
            videoBufferHandle = 0;
            AVBUFFER_ALLOC_SUCCESS(mAVBufferManager, videoPool, frameSize, videoBufferHandle);
        }

        // Write Frame Data
        void* mapBuffer = nullptr;
        uint32_t mapSize = 0;
        if (mSecureDecode) {
            std::lock_guard<std::mutex> lock(mAVbufferHelperLock);
            UT_LOG_DEBUG("Calling writeSecureHandle(handle:0x%llx, size=%u, gAVbufferHelper:%p)",
                        videoBufferHandle, frameSize, gAVbufferHelper);
            if (!gAVbufferHelper || !gAVbufferHelper->writeSecureHandle(videoBufferHandle, inputBuffer.data() + frameOffset, frameSize)) {
                UT_LOG_ERROR("Failed to copy frame data to secure buffer");
                AVBUFFER_FREE_SUCCESS(mAVBufferManager, videoBufferHandle);
                continue;
            }
        } else {
            std::lock_guard<std::mutex> lock(mAVbufferHelperLock);
            UT_LOG_DEBUG("Calling mapHandle(handle:0x%llx, gAVbufferHelper:%p)", videoBufferHandle, gAVbufferHelper);
            if (!gAVbufferHelper) {
                UT_LOG_ERROR("gAVbufferHelper is null before mapHandle");
                AVBUFFER_FREE_SUCCESS(mAVBufferManager, videoBufferHandle);
                continue;
            }
            mapBuffer = gAVbufferHelper->mapHandle(videoBufferHandle, &mapSize);
            if (mapBuffer && mapSize >= frameSize) {
                UT_LOG_DEBUG("Copying frame data to mapped buffer, size=%u", frameSize);
                memcpy(mapBuffer, inputBuffer.data() + frameOffset, frameSize);
            } else {
                UT_LOG_ERROR("Failed to map buffer or insufficient size (mapBuffer:%p, mapSize:%u, required:%u)",
                            mapBuffer, mapSize, frameSize);
                AVBUFFER_FREE_SUCCESS(mAVBufferManager, videoBufferHandle);
                continue;
            }
        }

        UT_LOG_DEBUG("mTestConfiguration.stopPts = %lld framePTS = %lld ",mTestConfiguration.stopPts,framePTS);
        UT_LOG_DEBUG("mTestConfiguration.discontinuityPts = %lld framePTS = %lld ",mTestConfiguration.discontinuityPts,framePTS);
        UT_LOG_DEBUG("mTestConfiguration.flushPts = %lld framePTS = %lld ",mTestConfiguration.flushPts,framePTS);
        UT_LOG_DEBUG("mTestConfiguration.flushResetPts = %lld framePTS = %lld ",mTestConfiguration.flushResetPts,framePTS);
        UT_LOG_DEBUG("mTestConfiguration.eosPts = %lld framePTS = %lld ",mTestConfiguration.eosPts,framePTS);

        // Decoder Operations
        {
            std::lock_guard<std::mutex> lock(mDecodeMutex);
            if (mTestConfiguration.stopPts == framePTS) {
                UT_LOG_DEBUG("Stopping decoder at PTS:%lld", framePTS);
                VIDEODECODER_STOP_SUCCESS(mDecoderHandle, videoDecoderController, true);
                VIDEODECODER_START_SUCCESS(mDecoderHandle, videoDecoderController, true);
            }
            if (mTestConfiguration.discontinuityPts == framePTS) {
                UT_LOG_DEBUG("Signaling discontinuity at PTS:%lld", framePTS);
                VIDEODECODER_SIGNAL_DISCONTINUITY_SUCCESS(mDecoderHandle, videoDecoderController, true);
            }
            if (mTestConfiguration.flushPts == framePTS) {
                UT_LOG_DEBUG("Flushing decoder at PTS:%lld", framePTS);
                VIDEODECODER_FLUSH_SUCCESS(mDecoderHandle, videoDecoderController, false, true);
            }
            if (mTestConfiguration.flushResetPts == framePTS) {
                UT_LOG_DEBUG("Flush reset at PTS:%lld", framePTS);
                VIDEODECODER_FLUSH_SUCCESS(mDecoderHandle, videoDecoderController, true, true);
                VIDEODECODER_START_SUCCESS(mDecoderHandle, videoDecoderController, true);
            }
            if (mTestConfiguration.eosPts == framePTS) {
                UT_LOG_DEBUG("Signaling EOS at PTS:%lld", framePTS);
                VIDEODECODER_SIGNAL_EOS_SUCCESS(mDecoderHandle, videoDecoderController, true);
                VIDEODECODER_DECODEBUFFER_FAILURE(videoDecoderController, framePTS, videoBufferHandle, Status::EX_NONE);
                VIDEODECODER_STOP_SUCCESS(mDecoderHandle, videoDecoderController, true);
                VIDEODECODER_START_SUCCESS(mDecoderHandle, videoDecoderController, true);
            }

            InputVideoFrame_t inputVideo;
            inputVideo.sendTime = std::chrono::steady_clock::now();
            inputVideo.framePTS = framePTS;
            inputVideo.rms_threshold = mTestConfiguration.rms_diff_threshold;
            inputVideo.isErrorFrame = isErrorFrame;
            UT_LOG_DEBUG("Adding frame to mInputFrame, PTS:%lld, errorFrame=%d", framePTS, isErrorFrame);
            mInputFrame.push_back(inputVideo);
        }

        auto decodeStart = std::chrono::steady_clock::now();

        // Decode Frame
        {
            std::lock_guard<std::mutex> lock(mDecodeMutex);
            if (!videoDecoderController) {
                UT_LOG_ERROR("videoDecoderController is null");
                AVBUFFER_FREE_SUCCESS(mAVBufferManager, videoBufferHandle);
                if (!mSecureDecode && mapBuffer) {
                    UT_LOG_DEBUG("Unmapping buffer handle:0x%llx", videoBufferHandle);
                    gAVbufferHelper->unmapHandle(videoBufferHandle);
                }
                continue;
            }
            if (isErrorFrame) {
                VIDEODECODER_DECODEBUFFER_FAILURE(videoDecoderController, framePTS, videoBufferHandle, Status::EX_NONE);
            } else {
                VIDEODECODER_DECODEBUFFER_SUCCESS(videoDecoderController, framePTS, videoBufferHandle);
            }
            getDecoderMetrics(framesDecoded, decodeErrors, framesDropped);
            UT_LOG_DEBUG("Decoder Metrics: Codec: %d, Frames Decoded=%d, Decode Errors=%d, Frames Dropped=%d",
                        mTestConfiguration.codec,framesDecoded, decodeErrors, framesDropped);
        }

        // Check decode time
        auto decodeEnd = std::chrono::steady_clock::now();
        auto decodeTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(decodeEnd - decodeStart).count();
        if (decodeTimeNs > mTestConfiguration.decode_time_threshold) {
            UT_LOG_DEBUG("Frame %zu decode time %lld ns exceeds threshold %lld ns",
                        i, decodeTimeNs, mTestConfiguration.decode_time_threshold);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(mTestConfiguration.decoderDelay));

        // Free Buffer
        {
            std::lock_guard<std::mutex> lock(mDecodeMutex);
            AVBUFFER_FREE_SUCCESS(mAVBufferManager, videoBufferHandle);
        }

        if (!mSecureDecode && mapBuffer) {
            std::lock_guard<std::mutex> lock(mAVbufferHelperLock);
            UT_LOG_DEBUG("Unmapping buffer handle:0x%llx", videoBufferHandle);
            if (gAVbufferHelper) {
                gAVbufferHelper->unmapHandle(videoBufferHandle);
            }
        }
    }

    // Get final metrics
    getDecoderMetrics(framesDecoded, decodeErrors, framesDropped);
    UT_LOG_DEBUG("Final Decoder Metrics: Codec: %d, Frames Decoded=%d, Decode Errors=%d, Frames Dropped=%d",
                mTestConfiguration.codec,framesDecoded, decodeErrors, framesDropped);

    // Stop Decoder
    {
        std::lock_guard<std::mutex> lock(mDecodeMutex);
        UT_LOG_DEBUG("Stopping decoder");
        VIDEODECODER_STOP_SUCCESS(mDecoderHandle, videoDecoderController, true);
    }

    getDecoderMetrics(framesDecoded, decodeErrors, framesDropped);
    UT_LOG_DEBUG("After Stop Decoder Metrics: Code: %d, Frames Decoded=%d, Decode Errors=%d, Frames Dropped=%d",
                mTestConfiguration.codec,framesDecoded, decodeErrors, framesDropped);

    // Close Decoder
    VIDEODECODER_CLOSE_SUCCESS(mDecoderHandle, videoDecoderController, true);

    // Destroy Video Pool
    {
        std::lock_guard<std::mutex> lock(mDecodeMutex);
        UT_LOG_DEBUG("Destroying video pool handle:%lld", videoPool.handle);
        AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, videoPool);
    }

    UT_LOG_DEBUG("[PID: %d, TID: %d] decoderThreadFunc End", getpid(), syscall(SYS_gettid));
    mDecodingStream = false;
}


std::vector<std::tuple<uint64_t, uint32_t, int64_t>> DecoderTest::parseFrames(const std::vector<uint8_t>& inputBuffer, uint64_t fileSize, com::rdk::hal::videodecoder::Codec codec)
{
    std::vector<std::tuple<uint64_t, uint32_t, int64_t>> frames; // {offset, size, pts}

    if (mTestConfiguration.frameDetails.empty()) {
        UT_LOG_ERROR("frameDetails is empty");
        return frames;
    }

    switch (codec) {
        case com::rdk::hal::videodecoder::Codec::H264:
        case com::rdk::hal::videodecoder::Codec::H265: {
            uint64_t pos = 0;
            size_t frameIndex = 0;
            while (pos < fileSize - 4) {
                bool isStartCode = false;
                uint32_t startCodeLen = 0;
                if (pos < fileSize - 3 && inputBuffer[pos] == 0 && inputBuffer[pos + 1] == 0 && inputBuffer[pos + 2] == 1) {
                    isStartCode = true;
                    startCodeLen = 3;
                } else if (pos < fileSize - 4 && inputBuffer[pos] == 0 && inputBuffer[pos + 1] == 0 &&
                        inputBuffer[pos + 2] == 0 && inputBuffer[pos + 3] == 1) {
                    isStartCode = true;
                    startCodeLen = 4;
                }

                if (isStartCode) {
                    uint64_t start = pos;
                    pos += startCodeLen;
                    uint64_t nextPos = pos;
                    while (nextPos < fileSize - 3) {
                        if ((inputBuffer[nextPos] == 0 && inputBuffer[nextPos + 1] == 0 && inputBuffer[nextPos + 2] == 1) ||
                            (nextPos < fileSize - 4 && inputBuffer[nextPos] == 0 && inputBuffer[nextPos + 1] == 0 &&
                            inputBuffer[nextPos + 2] == 0 && inputBuffer[nextPos + 3] == 1)) {
                            break;
                        }
                        nextPos++;
                    }
                    uint32_t frameSize = nextPos - start;
                    if (frameSize > startCodeLen) {
                        int64_t pts = (frameIndex < mTestConfiguration.frameDetails.size()) ?
                                    std::get<1>(mTestConfiguration.frameDetails[frameIndex]) :
                                    frameIndex * 33333333; // Default 30fps
                        frames.emplace_back(start, frameSize, pts);
                        UT_LOG_DEBUG("Frame detected: offset=%llu, size=%u, pts=%lld", start, frameSize, pts);
                    }
                    pos = nextPos;
                    frameIndex++;
                } else {
                    pos++;
                }
            }
            break;
        }
        case com::rdk::hal::videodecoder::Codec::MPEG: {
            uint64_t pos = 0;
            size_t frameIndex = 0;
            while (pos < fileSize - 4) {
                if (inputBuffer[pos] == 0 && inputBuffer[pos + 1] == 0 && inputBuffer[pos + 2] == 1 &&
                    inputBuffer[pos + 3] >= 0xE0 && inputBuffer[pos + 3] <= 0xEF) { // Video stream ID
                    uint64_t start = pos;
                    pos += 4;
                    if (pos + 2 >= fileSize) break;
                    uint32_t pesLength = (inputBuffer[pos] << 8) | inputBuffer[pos + 1];
                    pos += 2;
                    uint32_t frameSize = pesLength + 6; // Include start code and length
                    if (start + frameSize > fileSize) break;
                    int64_t pts = (frameIndex < mTestConfiguration.frameDetails.size()) ?
                                std::get<1>(mTestConfiguration.frameDetails[frameIndex]) :
                                frameIndex * 33333333; // Default 30fps
                    frames.emplace_back(start, frameSize, pts);
                    UT_LOG_DEBUG("MPEG PES packet detected: offset=%llu, size=%u, pts=%lld", start, frameSize, pts);
                    pos = start + frameSize;
                    frameIndex++;
                } else {
                    pos++;
                }
            }
            break;
        }
        case com::rdk::hal::videodecoder::Codec::VP9: {
            uint64_t pos = 32; // Skip IVF header (fixed 32 bytes)
            size_t frameIndex = 0;
            while (pos + 12 <= fileSize) {
                uint32_t frameSize = inputBuffer[pos] |
                                    (inputBuffer[pos + 1] << 8) |
                                    (inputBuffer[pos + 2] << 16) |
                                    (inputBuffer[pos + 3] << 24);
                uint64_t timestamp = static_cast<uint64_t>(inputBuffer[pos + 4]) |
                                    (static_cast<uint64_t>(inputBuffer[pos + 5]) << 8) |
                                    (static_cast<uint64_t>(inputBuffer[pos + 6]) << 16) |
                                    (static_cast<uint64_t>(inputBuffer[pos + 7]) << 24) |
                                    (static_cast<uint64_t>(inputBuffer[pos + 8]) << 32) |
                                    (static_cast<uint64_t>(inputBuffer[pos + 9]) << 40) |
                                    (static_cast<uint64_t>(inputBuffer[pos + 10]) << 48) |
                                    (static_cast<uint64_t>(inputBuffer[pos + 11]) << 56);

                uint64_t frameStart = pos + 12;
                if (frameStart + frameSize > fileSize) break;

                int64_t pts = (frameIndex < mTestConfiguration.frameDetails.size()) ?
                            std::get<1>(mTestConfiguration.frameDetails[frameIndex]) :
                            static_cast<int64_t>(timestamp); // Use IVF timestamp
                frames.emplace_back(frameStart, frameSize, pts);
                UT_LOG_DEBUG("VP9 IVF frame: offset=%llu, size=%u, pts=%lld", frameStart, frameSize, pts);

                pos = frameStart + frameSize;
                frameIndex++;
            }
            break;
        }
        case com::rdk::hal::videodecoder::Codec::AV1: {
            uint64_t pos = 0;
            size_t frameIndex = 0;
            while (pos < fileSize) {
                if (pos + 1 >= fileSize) break;
                uint8_t obuHeader = inputBuffer[pos];
                uint32_t obuType = (obuHeader >> 3) & 0x1F;
                bool hasSize = (obuHeader & 0x02) == 0;
                uint64_t start = pos;
                pos++;
                uint32_t obuSize = 0;
                if (hasSize) {
                    for (int i = 0; i < 8 && pos < fileSize; i++) {
                        uint8_t byte = inputBuffer[pos++];
                        obuSize |= (byte & 0x7F) << (i * 7);
                        if (!(byte & 0x80)) break;
                    }
                }
                if (obuType == 1 || obuType == 3 || obuType == 6) { // Frame or Temporal Unit OBUs
                    uint32_t frameSize = obuSize + (pos - start);
                    if (start + frameSize > fileSize) break;
                    int64_t pts = (frameIndex < mTestConfiguration.frameDetails.size()) ?
                                std::get<1>(mTestConfiguration.frameDetails[frameIndex]) :
                                frameIndex * 33333333; // Default 30fps
                    frames.emplace_back(start, frameSize, pts);
                    UT_LOG_DEBUG("AV1 OBU detected: offset=%llu, size=%u, pts=%lld", start, frameSize, pts);
                    pos += obuSize;
                    frameIndex++;
                } else {
                    pos += obuSize;
                }
            }
            break;
        }
        default:
            UT_LOG_ERROR("Unsupported codec: %d", static_cast<int>(codec));
            return frames;
    }

    if (!frames.empty() && frames.size() != mTestConfiguration.frameDetails.size()) {
        UT_LOG_WARNING("Frame count mismatch: parsed=%zu, expected=%zu", frames.size(), mTestConfiguration.frameDetails.size());
    }
    return frames;
}



bool DecoderTest::getDecoderMetrics(int32_t &framesDecoded, int32_t &decodeErrors, int32_t &framesDropped)
{
    optional<PropertyValue> propertyValue;

    framesDecoded = decodeErrors = framesDropped = -1;

    VIDEODECODER_GET_PROPERTY_SUCCESS(mDecoderHandle, Property::METRIC_FRAMES_DECODED, propertyValue);
    if (!propertyValue.has_value() || !propertyValue->value.has_value())
    {
        return false;
    }
    framesDecoded = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();

    VIDEODECODER_GET_PROPERTY_SUCCESS(mDecoderHandle, Property::METRIC_DECODE_ERRORS, propertyValue);
    if (!propertyValue.has_value() || !propertyValue->value.has_value())
    {
        return false;
    }
    decodeErrors = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();

    VIDEODECODER_GET_PROPERTY_SUCCESS(mDecoderHandle, Property::METRIC_FRAMES_DROPPED, propertyValue);
    if (!propertyValue.has_value() || !propertyValue->value.has_value())
    {
        return false;
    }
    framesDropped = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();

    return true;
}

void DecoderTest::run(bool concurrentDecodeConsume)
{
    mConcurrentDecodeConsume = concurrentDecodeConsume;
    mDecodingStream = true;
    if (concurrentDecodeConsume)
    {
        mDecoderThread = thread(&DecoderTest::decoderThreadFunc, this);
        this_thread::sleep_for(chrono::milliseconds(100));
        //mConsumerThread = thread(&DecoderTest::consumerThreadFunc, this);

        mDecoderThread.join();
        //mConsumerThread.join();
    }
    else
    {
        decodeAndConsumeVideoStreams();
    }
}
/** @} */ // End of VTS_L3_VIDEODECODER
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK
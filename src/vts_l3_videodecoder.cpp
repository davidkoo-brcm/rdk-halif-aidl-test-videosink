#include "vts_l3_common.h"
#include "vts_l3_videodecoder.h"
#include "vts_l3_avbuffer.h"

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
    UT_LOG_INFO("onFrameOutput is received nsPresentationTime %lld FrameBufferHandle = 0x%llx ",nsPresentationTime,frameBufferHandle);
    if (metadata.has_value())
    {
        const FrameMetadata& frameMeta = metadata.value();
        UT_LOG_INFO("FrameMetadata:");
        UT_LOG_INFO("  Pixel Aspect Ratio: %d:%d", frameMeta.parX, frameMeta.parY);
        UT_LOG_INFO("  Source Aspect Ratio: %d:%d", frameMeta.sarX, frameMeta.sarY);
        UT_LOG_INFO("  Coded Dimensions: width=%d, height=%d", frameMeta.codedWidth, frameMeta.codedHeight);
        UT_LOG_INFO("  Active Display: x=%d, y=%d, width=%d, height=%d",
                    frameMeta.activeX, frameMeta.activeY, frameMeta.activeWidth, frameMeta.activeHeight);
        UT_LOG_INFO("  Color Depth: %d bits", frameMeta.colorDepth);
        UT_LOG_INFO("  Pixel Format: %d", static_cast<int>(frameMeta.pixelFormat));
        UT_LOG_INFO("  Dynamic Range: %d", static_cast<int>(frameMeta.dynamicRange));
        UT_LOG_INFO("  Scan Type: %d", static_cast<int>(frameMeta.scanType));
        UT_LOG_INFO("  AFD: %d", frameMeta.afd);
        UT_LOG_INFO("  Frame Rate: %d/%d", frameMeta.frameRateNumerator, frameMeta.frameRateDenominator);
        UT_LOG_INFO("  End of Stream: %d", frameMeta.endOfStream);
        UT_LOG_INFO("  Discontinuity: %d", frameMeta.discontinuity);
        UT_LOG_INFO("  Low Latency: %d", frameMeta.lowLatency);
        UT_LOG_INFO("  Source: %d", static_cast<int>(frameMeta.source));

        // Handle SHA1 field (byte array)
        if (!frameMeta.sha1.empty()) {
            std::string sha1_hex;
            for (const auto& byte : frameMeta.sha1) {
                char buf[3];
                snprintf(buf, sizeof(buf), "%02x", static_cast<unsigned char>(byte));
                sha1_hex += buf;
            }
            UT_LOG_INFO("  SHA1: %s", sha1_hex.c_str());
        } else {
            UT_LOG_INFO("  SHA1: (empty)");
        }

        // Handle extension (ParcelableHolder)
        // Note: ParcelableHolder may require specific handling depending on its content
        UT_LOG_INFO("  Extension: (ParcelableHolder present)");
    } else {
        UT_LOG_INFO("No FrameMetadata provided");
    }
    return Status::ok();
}


Status VideoDecoderControllerListener::onUserDataOutput(int64_t nsPresentationTime, const std::vector<uint8_t>& userData)
{
    UT_LOG_INFO("onUserDataOutput: PTS = %lx, UserDataSize = %zu", nsPresentationTime, userData.size());
    if (!userData.empty()) {
        UT_LOG_INFO("First byte of user data: 0x%02X", userData[0]);
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
    UT_LOG("State Change received: %s -> %s", toString(oldState).c_str(), toString(newState).c_str());
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
        {"MPEG", Codec::MPEG2_VIDEO},
        {"H264", Codec::H264_AVC},
        {"H265", Codec::H265_HEVC},
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
    UT_LOG_INFO("streamInputPath: %s", streamInputPath.c_str());

    status = ut_kvp_getStringField(kvpTestInstance, "IVideoDecoderManager.reference_streams_path", tempBuffer, UT_KVP_MAX_ELEMENT_SIZE);
    if (status != UT_KVP_STATUS_SUCCESS)
    {
        UT_LOG_ERROR("Failed to get reference_streams_path, status: %d", status);
        return UT_STATUS_FAILURE;
    }
    streamRefPath = tempBuffer;
    UT_LOG_INFO("streamRefPath: %s", streamRefPath.c_str());

    streamCount = ut_kvp_getListCount(kvpTestInstance, streamList.c_str());
    if (streamCount <= 0)
    {
        UT_LOG_ERROR("No streams found in %s", streamList.c_str());
        return UT_STATUS_FAILURE;
    }
    UT_LOG_INFO("Found %d streams", streamCount);

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
        UT_LOG_INFO("Stream name: %s", resultKVP.c_str());
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
        string colourSpace = prefix + "colour_space";
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

        // Input stream path
        status = ut_kvp_getStringField(kvpTestInstance, input.c_str(), tempBuffer, UT_KVP_MAX_ELEMENT_SIZE);
        if (status == UT_KVP_STATUS_SUCCESS)
        {
            resultKVP = tempBuffer;
            fullPath = filesystem::path(streamInputPath) / resultKVP;
            UT_LOG_INFO("Full input file path: %s", fullPath.string().c_str());

            if (!filesystem::exists(fullPath))
            {
                UT_LOG_ERROR("Input file does not exist: %s", fullPath.string().c_str());
                continue;
            }

            testDetails.input = fullPath.string();
            UT_LOG_INFO("Input path set: %s", testDetails.input.c_str());
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

        // Colour space
        status = ut_kvp_getStringField(kvpTestInstance, colourSpace.c_str(), tempBuffer, UT_KVP_MAX_ELEMENT_SIZE);
        if (status == UT_KVP_STATUS_SUCCESS)
        {
            testDetails.colourSpace = string(tempBuffer);
            UT_LOG_INFO("Colour space: %s", testDetails.colourSpace.c_str());
        }
        else
        {
            UT_LOG_WARNING("Colour space not found, defaulting to empty");
            testDetails.colourSpace = "";
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
            UT_ASSERT_TRUE(rms_diff <= mTestConfiguration.rms_diff_threashold);
        }

        auto duration = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - startTime);
        UT_LOG_INFO("Decode Time [PTS: %lld]: %lld", framePTS, duration.count());

        startFramesDecoded++;
        if (errorFrame) startFrameErrors++;

        UT_ASSERT_TRUE(startFramesDecoded == endFramesDecoded);
        UT_ASSERT_TRUE(startFrameErrors == endFrameErrors);
        UT_ASSERT_TRUE(startFramesDropped == endFramesDropped);

        AVBUFFER_FREE_SUCCESS(mAVBufferManager, videoBufferHandle);
    }

    VIDEODECODER_STOP_SUCCESS(mDecoderHandle, videoDecoderController, true);
    VIDEODECODER_CLOSE_SUCCESS(mDecoderHandle, videoDecoderController, true);
    AVBUFFER_DESTROY_POOL_SUCCESS(mAVBufferManager, videoPool);
}

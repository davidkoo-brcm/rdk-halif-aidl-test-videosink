/**
 *  If not stated otherwise in this file or this component's LICENSE
 *  file the following copyright and licenses apply:
 *
 *  Copyright 2026 RDK Management
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
 * @file vts_videosink_test.cpp
 *
 */

#include "vts_videosink_test.h"

void VideoSinkTest::run ( bool isTunnel )
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

bool VideoSinkTest::getSinkPropertyInt(videosink::Property property, int32_t& outValue)
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
        UT_LOG_ERROR("Result getProperty(IN:property[%s], propertyValue.has_value[%s]), status:[isOK:%s, exception:%s]",
            videosink::toString(property).c_str(),
            UT_Control_GetMapString(bool_mapTable, propertyValue.has_value()),
            UT_Control_GetMapString(bool_mapTable, status.isOk()),
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));

        return false;
    }

    outValue = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();
    return true;
}

void VideoSinkTest::setupClock()
{
    // Open the clock
    AVCLOCK_MANAGER_GETAVCLOCK_SUCCESS(mClockId, mClock);

    // Open the clock controller
    AVCLOCK_OPEN_SUCCESS(mClock, mClockCtrlListener, mClockController,true);

    // Set the clock mode to Audio Master
    AVCLOCK_SET_CLOCK_MODE_SUCCESS(mClock, mClockController, ClockMode::AUDIO_MASTER, true);
}

void VideoSinkTest::setupVideoSink()
{
    /*Open the Video Sink*/
    VIDEOSINK_OPEN_SUCCESS(mSinkHandle, mVideoSinkControlListener, mVideoSinkController, true);

    /*Associate sink with decoder before starting queue operations.*/
    VIDEOSINK_CONTROLLER_SETVIDEODECODER_SUCCESS(mVideoSinkController, mDecoderId);

    VIDEOSINK_ATTACH_CLOCK_SUCCESS(mVideoSinkController, mClockId);

    /*Start the Video Sink*/
    VIDEOSINK_START_SUCCESS(mSinkHandle, mVideoSinkController, true);

    if(!mIsClockInUse)
    {
        setupClock();
    }
}

void VideoSinkTest::videoSinkNonTunnelMode()
{
    IAVClock::Id clockId;
    clockId.value = IAVClock::Id::UNDEFINED;
    bool clockAttached = false;
    bool clockSessionAcquired = false;

    try
    {
        UT_LOG_INFO("[SinkID: %d] Start of Testing Stream: %s", mSinkId.value, mTestConfiguration.input.c_str());

        /*Reset the Flush, EOS, Underflow and Resume Counts*/
        mVideoSinkControlListener->resetUnderflow();
        mVideoSinkEventListner->resetUnderflow();
        mVideoSinkControlListener->resetResumed();
        mVideoSinkEventListner->resetResumed();
        mVideoSinkControlListener->resetEos();
        mVideoSinkEventListner->resetEos();
        mVideoSinkControlListener->resetFlushComplete();
        mVideoSinkEventListner->resetFlushComplete();

        /*Wait up to 500 ms for a frame, and tolerate a few consecutive misses*/
        const int64_t frameTimeoutMs = 500; // Wait up to 500 ms for a frame
        int timeoutCount = 0;
        const int maxTimeoutCount = 3;
        const bool verifyFrameBufferAvailable = (mSinkTestConfiguration.testName == "Sink_FrameBufferAvailable");
        const bool verifyFrameBufferAvailableNoClockAttach =
            (mSinkTestConfiguration.testName == "Sink_FrameBufferAvailable_NoClockAttach");
        const bool verifyAnyFrameBufferAvailable =
            verifyFrameBufferAvailable || verifyFrameBufferAvailableNoClockAttach;
        const bool verifyPrimedAgainstVideoPts = !verifyAnyFrameBufferAvailable;
        bool frameBufferEpisodeVerified = false;
        bool avClockPrimedVerified = false;

        if (!verifyFrameBufferAvailableNoClockAttach)
        {

        }

        if (verifyAnyFrameBufferAvailable)
        {
            mVideoSinkControlListener->resetFrameBufferAvailable();
        }

        /*Set the "discard until PTS" value for the relevant test cases*/
        if (mSinkTestConfiguration.vSink_discardFramesUntilPts >= 0)
        {
            VIDEOSINK_SIGNAL_DISCARDFRAMESTILL_SUCCESS(controller, static_cast<uint64_t>(mSinkTestConfiguration.vSink_discardFramesUntilPts));
        }

        while (true)
        {
            DecodedVideoFrame_t decodedFrame;

            if (!mDecoderControllerListener)
            {
                UT_LOG_WARNING("[SinkID: %d] Decoder listener not available, exiting sink loop", mSinkId.value);
                break;
            }

            bool success = mDecoderControllerListener->getNextFrame(decodedFrame, std::chrono::milliseconds(frameTimeoutMs));

            if (!success)
            {
                ++timeoutCount;
                UT_LOG_WARNING("[SinkID: %d] Timeout %d/%d waiting for frame from decoder", mSinkId.value, timeoutCount, maxTimeoutCount);

                if (timeoutCount >= maxTimeoutCount)
                {
                    UT_LOG_WARNING("[SinkID: %d] Exiting loop after repeated decoder timeouts", mSinkId.value);
                    break;
                }
                continue;
            }

            timeoutCount = 0;

            int64_t pts = decodedFrame.framePTS;
            int64_t bufferHandle = decodedFrame.frameAVBufferHandle;

            // Use fully qualified name to avoid ambiguity
            videodecoder::FrameMetadata metadata{};

            if (decodedFrame.metadata)
            {
                // Copy field-by-field because move assignment is deleted
                const auto& src = *(decodedFrame.metadata);
                metadata.parX = src.parX;
                metadata.parY = src.parY;
                metadata.sarX = src.sarX;
                metadata.sarY = src.sarY;
                metadata.codedWidth = src.codedWidth;
                metadata.codedHeight = src.codedHeight;
                metadata.activeX = src.activeX;
                metadata.activeY = src.activeY;
                metadata.activeWidth = src.activeWidth;
                metadata.activeHeight = src.activeHeight;
                metadata.colorDepth = src.colorDepth;
                metadata.pixelFormat = src.pixelFormat;
                metadata.dynamicRange = src.dynamicRange;
                metadata.scanType = src.scanType;
                metadata.afd = src.afd;
                metadata.frameRateNumerator = src.frameRateNumerator;
                metadata.frameRateDenominator = src.frameRateDenominator;
                metadata.endOfStream = src.endOfStream;
                metadata.discontinuity = src.discontinuity;
                metadata.lowLatency = src.lowLatency;
                metadata.source = src.source;
                metadata.sha1 = src.sha1;
            }

            // Override EOS condition if PTS matches configured EOS PTS
            if (mSinkTestConfiguration.vSink_eos_pts >= 0 && pts == static_cast<int64_t>(mSinkTestConfiguration.vSink_eos_pts))
            {
                metadata.endOfStream = true;
            }

            UT_LOG_INFO("[SinkID: %d] Queueing frame: PTS=%" PRId64 ", Handle=%" PRIx64 ", EOS=%d, Discontinuity=%d",
                        mSinkId.value, pts, bufferHandle, metadata.endOfStream, metadata.discontinuity);

            // Simulate underflow
            if (mSinkTestConfiguration.vSink_underflowPts >= 0 &&
                pts == static_cast<int64_t>(mSinkTestConfiguration.vSink_underflowPts))
            {
                uint32_t frameDurationMs = 0;
                if (metadata.frameRateNumerator > 0 && metadata.frameRateDenominator > 0)
                {
                    frameDurationMs = static_cast<uint32_t>(
                        1000.0 * metadata.frameRateDenominator / metadata.frameRateNumerator);
                }

                UT_LOG_DEBUG("[SinkID: %d] Injecting delay at PTS %lld to simulate underflow",
                             mSinkId.value, static_cast<long long>(pts));
                std::this_thread::sleep_for(std::chrono::milliseconds(frameDurationMs));
            }

            if (verifyAnyFrameBufferAvailable && !frameBufferEpisodeVerified)
            {
                int refusedCount = 0;
                int acceptedCount = 0;
                const int burstDepth = 64;

                UT_LOG_INFO("[SinkID: %d] Triggering queue back-pressure burst for onFrameBufferAvailable verification", mSinkId.value);
                for (int i = 0; i < burstDepth; ++i)
                {
                    bool aidlReturn = false;
                    ::android::binder::Status status = controller->queueVideoFrame(pts + i, bufferHandle, metadata, &aidlReturn);

                    if (aidlReturn)
                    {
                        UT_ASSERT_TRUE(status.isOk());
                        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE);
                        acceptedCount++;
                    }
                    else
                    {
                        // Some implementations report queue full with EX_NONE + false, others with an exception code.
                        UT_ASSERT_TRUE(status.exceptionCode() == Status::EX_NONE ||
                                       status.exceptionCode() == Status::EX_UNSUPPORTED_OPERATION);
                        refusedCount++;
                    }
                }

                UT_LOG_INFO("[SinkID: %d] Back-pressure burst result accepted=%d refused=%d",
                            mSinkId.value, acceptedCount, refusedCount);
                UT_ASSERT_TRUE_FATAL(refusedCount > 0);

                if (verifyFrameBufferAvailableNoClockAttach)
                {
                    UT_LOG_INFO("[SinkID: %d] Queue is full without AVClock; attaching and starting AVClock now", mSinkId.value);
                    clockId = acquireStartedAVClock();
                    UT_ASSERT_TRUE_FATAL(clockId.value != IAVClock::Id::UNDEFINED);
                    clockSessionAcquired = true;
                    VIDEOSINK_ATTACH_CLOCK_SUCCESS(controller, clockId);
                    clockAttached = true;
                }

                // Keep retrying while back-pressure is active; callback should still be one-per-transition.
                for (int i = 0; i < 5; ++i)
                {
                    bool aidlReturn = true;
                    ::android::binder::Status status = controller->queueVideoFrame(pts + burstDepth + i, bufferHandle, metadata, &aidlReturn);
                    if (!aidlReturn)
                    {
                        UT_ASSERT_TRUE(status.exceptionCode() == Status::EX_NONE ||
                                       status.exceptionCode() == Status::EX_UNSUPPORTED_OPERATION);
                    }
                }

                bool callbackArrived = mVideoSinkControlListener->waitForFrameBufferAvailableEvent(3);
                UT_ASSERT_TRUE_FATAL(callbackArrived);
                UT_ASSERT_EQUAL(mVideoSinkControlListener->getFrameBufferAvailableCount(), 1);

                frameBufferEpisodeVerified = true;
                break;
            }

            VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_SUCCESS(controller, pts, bufferHandle, metadata);

            if (clockAttached && clockSessionAcquired && verifyPrimedAgainstVideoPts && !avClockPrimedVerified)
            {
                const bool primedVerified =
                    verifyStartedAVClockPrimedForVideoPts(pts, std::chrono::milliseconds(3000));
                UT_ASSERT_TRUE(primedVerified);
                if (primedVerified)
                {
                    avClockPrimedVerified = true;
                }
            }

            receivedPtsSet.insert(pts);
            ++actualFramesReceived;

            if (metadata.endOfStream)
            {
                UT_LOG_DEBUG("[SinkID: %d] EOS frame received, stopping video sink input loop",
                             mSinkId.value);
                break;
            }

            if (mSinkTestConfiguration.vSink_interFrameDelayMs > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(mSinkTestConfiguration.vSink_interFrameDelayMs));
            }

            if (mSinkTestConfiguration.vSink_flush_pts >= 0 && pts == static_cast<int64_t>(mSinkTestConfiguration.vSink_flush_pts))
            {
                VIDEOSINK_FLUSH_SUCCESS(controller, false);
            }

        }

        if (verifyAnyFrameBufferAvailable)
        {
            UT_ASSERT_TRUE_FATAL(frameBufferEpisodeVerified);
        }

        if (clockAttached && clockSessionAcquired && verifyPrimedAgainstVideoPts && actualFramesReceived > 0)
        {
            UT_ASSERT_TRUE(avClockPrimedVerified);
        }

        /*Validation of EOS*/
        if (mSinkTestConfiguration.vSink_eos_pts >= 0)
        {
            videodecoder::FrameMetadata dummyMeta{};
            dummyMeta.endOfStream = true;
            handleEosFrame(dummyMeta, mSinkTestConfiguration.vSink_eos_pts);
        }

        /*Validation of Simulated Underflow*/
        if (mSinkTestConfiguration.vSink_underflowPts >= 0)
        {
            verifyUnderflow();
        }

        /*Validation of Simulated Resume*/
        if (mSinkTestConfiguration.vSink_resumeTriggerPts >= 0)
        {
            verifyResume();
        }

        if (mSinkTestConfiguration.vSink_flush_pts >= 0)
        {
            verifyFlushComplete();
        }

        /*Validation of whether UnderFlow occured in normal Queuing*/
        verifyNoUnexpectedUnderflow();

        /* Validating the metrics data after the queuing process is complete. */
        runPostVerification();

        cleanupClock();

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        VIDEOSINK_STOP_SUCCESS(mSinkHandle, controller, true);
        VIDEOSINK_CLOSE_SUCCESS(mSinkHandle, controller, true);
    }
    catch (const std::exception& ex) {
        cleanupClock();
        UT_LOG_ERROR("[SinkID: %d] Exception in videoSinkNonTunnelMode: %s", mSinkId.value, ex.what());
        UT_ASSERT_TRUE(false);
    } catch (...) {
        cleanupClock();
        UT_LOG_ERROR("[SinkID: %d] Unknown exception in videoSinkNonTunnelMode", mSinkId.value);
        UT_ASSERT_TRUE(false);
    }
}

void VideoSinkTest::videoSinkTunnelMode()
{
//TODO: Need to implement the tunnel test.
}

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
void VideoSinkTest::verifyNoUnexpectedUnderflow()
{
    if (mSinkTestConfiguration.vSink_underflowPts < 0) {
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
void VideoSinkTest::verifyUnderflow()
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
void VideoSinkTest::verifyResume()
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
void VideoSinkTest::handleEosFrame(const videodecoder::FrameMetadata& metadata, int64_t pts)
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
void VideoSinkTest::verifyFlushComplete()
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
void VideoSinkTest::verifyQueueDepth()
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
void VideoSinkTest::verifyFramesReceived()
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
void VideoSinkTest::verifyFramesDroppedLate()
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
void VideoSinkTest::verifyFramesDroppedFRC()
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
void VideoSinkTest::verifyFramesRepeatedFRC()
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
void VideoSinkTest::verifyFramesMissed()
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
 * defined in `mTestConfiguration.vSink_testName`. Handles various cases including
 * queue depth, frames received, dropped frames, repeated frames, and missed frames,
 * for both sequential and concurrent non-tunnel modes.
 */
void VideoSinkTest::runPostVerification()
{
    const std::string &testName = mSinkTestConfiguration.testName;

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

/** @} */ // End of VIDEO_SINK_HALTEST_CLASS
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK
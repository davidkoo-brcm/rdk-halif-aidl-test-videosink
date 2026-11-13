# VideoSink VTS Change Review

## Latest test results

Platform: Broadcom Xione. Profile: `profiles/hfp-unified_xione.yaml`. Run date: 2026-11-13.

| Level | Total | PASS | FAIL | SKIP |
| --- | ---: | ---: | ---: | ---: |
| L1 | 35 | 35 | 0 | 0 |
| L2 | 5 | 5 | 0 | 0 |
| L3 | 48 | 12 | 0 | 36 |
| Total | 88 | 52 | 0 | 36 |

Sequential and concurrent Tunnel NonSecure basic, EOS, Underflow, Flush, DiscardFramesUntil and BackPressure tests all pass. The 36 Secure and NonTunnel tests skip according to capabilities.

## Prepare output routes before decoder startup

The sequential path opens the decoder session and prepares the sink before launching the decoder thread. The concurrent path opens all decoder sessions, then prepares all sinks before launching threads. Startup depends on completed preparation rather than fixed delays. prepare() reuses an existing controller and resets listener counters only once.

[src/vts_videosink_l3_tests.cpp:308](../../src/vts_videosink_l3_tests.cpp#L308)

```cpp
                if (!decodeTest.openDecoderSession())
                {
                    UT_ASSERT_TRUE(false) << "Failed to open decoder before sink preparation";
                    decodeTest.closeDecoderSession();
                    return executedAtLeastOne;
                }
                if (!sinkTest.prepare(isTunnel))
                {
                    UT_ASSERT_TRUE(false) << "Failed to prepare sink before decoder start";
                    sinkTest.close();
                    decodeTest.closeDecoderSession();
                    return executedAtLeastOne;
                }

                std::thread decodeThread(&videoDecoderTest::run, &decodeTest, true);
```

[src/vts_videosink_l3_tests.cpp:463](../../src/vts_videosink_l3_tests.cpp#L463)

```cpp
            for (int32_t i = 0; i < numInstances; ++i)
                sinkThreads.emplace_back(&VideoSinkTest::run, sinkTests[i].get(), tunnel);
            for (int32_t i = 0; i < numInstances; ++i)
                decoderThreads.emplace_back(&videoDecoderTest::run, decoderTests[i].get(), true);
```

## Prepare the clock before sink start

An owned clock is opened and configured before sink start so it exists when the HAL binds STC. External AVClock paths reuse their prepared clock.

[src/vts_videosink_test.cpp:154](../../src/vts_videosink_test.cpp#L154)

```cpp
    // The clock must exist when sink start binds the decoder's STC channel.
    if(!mIsClockInUse)
    {
        setupClock();
    }

    /*Start the Video Sink*/
    VIDEOSINK_START_SUCCESS(mSinkHandle, mVideoSinkController, true);
```

## Normalize tunnel BackPressure before preparation

Tunnel mode feeds the sink directly and has no client frame queue to backpressure. prepare(bool) disables sink_backpressure before clock preparation so the clock starts. The existing prepare() entry point is retained.

[src/vts_videosink_test.cpp:216](../../src/vts_videosink_test.cpp#L216)

```cpp
bool VideoSinkTest::prepare(bool isTunnel)
{
    // Tunnel mode has no frame queue to backpressure. Normalize before clock
    // preparation so the clock starts even when prepare() precedes run().
    if (isTunnel) mTestConfiguration.sink_backpressure = false;
    return prepare();
}
```

## Complete sink EOS after decoder EOS and output drain

The shared decoder producer ends input at the configured eosPts and retains the decoder session until the sink finishes. The tunnel sink drains output and confirms decoder EOS before sending sink EOS, flushing, and sending final EOS. It does not require a specific input PTS to appear in output. The first expected EOS PTS is the last output value saved in mFirstSignalEOSPTS; final EOS is compared with mSignalEOSPTS. NonTunnel control-point handling is retained. Both listeners must still receive two EOS callbacks and one EOS-control-point flush callback, plus any separately configured flush. Callback completion waits up to two seconds without relaxing exact counts.

[src/vts_videosink_test.cpp:752](../../src/vts_videosink_test.cpp#L752)

```cpp
        // The producer stops input at eosPts. After draining output, use
        // decoder EOS to complete this control point even if its picture was
        // dropped. The sink reports the last output PTS, not the input PTS.
        if (mTestConfiguration.eosPts >= 0)
        {
            const bool decoderEos = mDecoderControllerListener->waitForEndOfStream(
                1, std::chrono::milliseconds(5000));
            UT_ASSERT_TRUE(decoderEos);
            UT_ASSERT_TRUE(mSignalEOSPTS >= 0) << "No output frame before decoder EOS";
            if (!decoderEos || mSignalEOSPTS < 0) { tearDownVideoSink(); return; }

            mFirstSignalEOSPTS = mSignalEOSPTS;
            if (mFirstSignalEOSPTS != mTestConfiguration.eosPts)
            {
                UT_LOG_WARNING("[SinkID: %d] EOS input PTS=%lld, last output PTS=%lld",
                               mSinkId.value,
                               static_cast<long long>(mTestConfiguration.eosPts),
                               static_cast<long long>(mFirstSignalEOSPTS));
            }
            VIDEOSINK_SIGNAL_EOS_SUCCESS(mVideoSinkController);
            VIDEOSINK_FLUSH_SUCCESS(mVideoSinkController, true);
        }

        /* Signal EOS; after the control-point flush its PTS is unchanged. */
        VIDEOSINK_SIGNAL_EOS_SUCCESS(mVideoSinkController);
```

[src/vts_videosink_test.cpp:259](../../src/vts_videosink_test.cpp#L259)

```cpp
    const size_t expectedEosCount = mTestConfiguration.eosPts >= 0 ? 2 : 1;
    const int expectedFlushCount = (mTestConfiguration.flushPts >= 0 ? 1 : 0) +
                                   (mTestConfiguration.eosPts >= 0 ? 1 : 0);
```

## Session cleanup

Preparation failures clean up sink and decoder sessions before threads start. The destructor closes a remaining sink. Teardown returns when no controller exists and clears the pointer after close to prevent duplicate shutdown. Concurrent cleanup releases sink helpers before decoder sessions and buffer pools, then checks heap metrics.

[src/vts_videosink_test.cpp:67](../../src/vts_videosink_test.cpp#L67)

```cpp
VideoSinkTest::~VideoSinkTest()
{
    close();
}

void VideoSinkTest::close()
{
    if (mVideoSinkController != nullptr) tearDownVideoSink();
}
```

## Underflow input control

Tunnel Underflow pauses the input producer through its pause hook. Input resumes after both sink listeners observe underflow, polling every 10 ms for up to four seconds. Failure to observe starvation remains an assertion.

[src/vts_videosink_test.cpp:244](../../src/vts_videosink_test.cpp#L244)

```cpp
bool VideoSinkTest::waitForInputUnderflow()
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (mVideoSinkControlListener->getUnderflowCount() > 0 &&
            mVideoSinkEventListener->getUnderflowCount() > 0)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}
```

## Capability and discard validation

The shared checkDecoderSupportsConfig() checks codec, dynamic range and frame rate. A configuration without a supported decoder/sink combination skips; missing declared sink or clock resources retain assertions. discardFramesUntil() is requested after sink start and the first displayed PTS must meet its lower bound. A STARTED HAL sink without a decoder stores the PTS; an attached decoder uses the NEXUS clipping path. The HAL document describes decoder-free requests.

# VideoSink VTS Change Review

## Latest test results

Platform: Broadcom Xione. Profile: `profiles/hfp-unified_xione.yaml`.

| Check | Latest result |
| --- | --- |
| AVClock regression with DSOLA off | PASS — confirmed by the user |
| `seq_tun_nonsec_AutoAudioBeforeVideo` (2026-11-12) | PASS — 81.119 s |

An AVClock PASS verifies priming and clock progression, not simultaneous audible output or pitch-corrected trick playback.

## Prepare the sink once before media starts

Expose idempotent preparation for AVClock. An existing controller is reused. Otherwise reset first-frame, underflow/resume, EOS and flush counters once and configure the sink. Both tunnel and non-tunnel execution reuse preparation, preserving callbacks that arrive after AVClock pre-connects the sink.

[`src/vts_videosink_test.cpp:202`](../../src/vts_videosink_test.cpp#L202)

```cpp
bool VideoSinkTest::prepare()
{
    if (mVideoSinkController != nullptr)
        return true;
    /*Reset the Flush, EOS, Underflow and Resume Counts*/
    mVideoSinkControlListener->resetFirstFrameRendered();
    mVideoSinkEventListener->resetFirstFrameRendered();
    mVideoSinkControlListener->resetUnderflow();
    mVideoSinkEventListener->resetUnderflow();
    mVideoSinkControlListener->resetResumed();
    mVideoSinkEventListener->resetResumed();
    mVideoSinkControlListener->resetEos();
    mVideoSinkEventListener->resetEos();
    mVideoSinkControlListener->resetFlushComplete();
    mVideoSinkEventListener->resetFlushComplete();

    setupVideoSink();
    return mVideoSinkController != nullptr;
}
```

## Current change scope

This review covers the current local VideoSink VTS changes, its shared
VideoDecoder helper, and the related VideoSink HAL implementation.

| Area | Files |
| --- | --- |
| Capability selection and teardown | [vts_videosink_l3_tests.cpp](../../src/vts_videosink_l3_tests.cpp) |
| Sink execution and assertions | [vts_videosink_test.cpp](../../src/vts_videosink_test.cpp), [vts_videosink_test.h](../../src/vts_videosink_test.h) |
| Shared decoder producer | [vts_videodecoder.h](../../../videodecoder/src/vts_videodecoder.h), [vts_videodecoder_test.cpp](../../../videodecoder/src/vts_videodecoder_test.cpp) |
| VideoSink HAL | `hal/video-sink-soc-broadcom/src/VideoSinkControllerImpl.cpp`, `hal/video-sink-soc-broadcom/include/VideoSinkControllerImpl.h` |

The [VideoDecoder review](../../../videodecoder/docs/pages/VideoDecoder_VTS_Change_Review.md)
describes the shared capability policy and header dependencies.

## Capability selection and resource cleanup

VideoSink delegates to `::checkDecoderSupportsConfig()` for shared
HAL/HFP capability checks, including dynamic range and fractional frame
rate. A test with no usable decoder/sink combination skips. Missing
declared sink or clock resources retain assertion checks.

Teardown unregisters decoder event listeners, deletes owned capability
objects, and clears their list.

### Shared capability checker

Location: [videosink/src/vts_videosink_l3_tests.cpp:117](../../src/vts_videosink_l3_tests.cpp#L117) · `checkDecoderSupportsConfig()`

```cpp
bool checkDecoderSupportsConfig(const IVideoDecoder::Id& decoderId,
                                const videodecoder::Capabilities* capabilities,
                                const videoDecoderTestConfiguration_t& decoderConfig,
                                bool secureDecode)
{
    return ::checkDecoderSupportsConfig(decoderId, capabilities, decoderConfig, secureDecode);
}
```

The explicit global qualifier selects the shared helper, so dynamic range and fractional frame-rate policy is consistent across component tests.

### Unsupported decoder selection

Location: [videosink/src/vts_videosink_l3_tests.cpp:377](../../src/vts_videosink_l3_tests.cpp#L377) · `runConcurrentSink()`

```cpp
int32_t availableDecoderCount = availableDecoderList.size();
int32_t availableSinkCount = availableSinkList.size();
int32_t availableClockCount = gVideosinks_hfp.availableClocks_hfp.size();
int32_t numInstances = std::min({availableDecoderCount, availableSinkCount, availableClockCount});
if (availableDecoderCount == 0)
{
    UT_LOG_WARNING("No usable decoder found for the given test configuration, skipping concurrent test.");
    return false;
}
```

No usable decoder returns false for this configuration. The enclosing runner skips when no configuration executes; missing declared sink/clock resources remain separately asserted.

### Decoder listener and capability cleanup

Location: [videosink/src/vts_videosink_l3_tests.cpp:540](../../src/vts_videosink_l3_tests.cpp#L540) · `TearDown()`

```cpp
for (const auto& [decoderId, decoder, controlListener, eventListener, capabilities] : mVideoDecoderList)
{
    VIDEODECODER_UNREGISTER_EVENT_SUCCESS(decoder, eventListener);
    delete capabilities;
}
mVideoDecoderList.clear();
```

Each registered decoder event listener is removed, its owned capability object is deleted, and the container is cleared.


## Tunnel EOS and callback handling

At the configured EOS control PTS, VTS waits for decoder EOS, signals
sink EOS, and flushes the sink. After draining the frame queue, it signals
final EOS. With a sink attached, the decoder helper preserves the completed
decoder session until sink processing finishes. Decoder-only tests retain
their stop/start state-transition coverage.

HAL suppresses duplicate sink EOS with an atomic exchange. Sink flush
resets the EOS flag and allows another EOS notification.

VTS waits up to two seconds for expected EOS/flush and requested resumed
callbacks, then checks exact callback counts. Each listener must report
exactly one first frame. Discard tests preserve the configured threshold
and check first rendered PTS >= threshold; ordinary playback keeps its
first-PTS equality check.

### EOS control point and final EOS

Location: [videosink/src/vts_videosink_test.cpp:729](../../src/vts_videosink_test.cpp#L729) · `VideoSinkTest::videoSinkTunnelMode()`

```cpp
    mSignalEOSPTS = pts;
    if (mTestConfiguration.eosPts >= 0 && pts == mTestConfiguration.eosPts)
    {
        const bool decoderEos = mDecoderControllerListener->waitForEndOfStream(
            1, std::chrono::milliseconds(5000));
        UT_ASSERT_TRUE(decoderEos);
        if (!decoderEos) { tearDownVideoSink(); return; }
        VIDEOSINK_SIGNAL_EOS_SUCCESS(mVideoSinkController);
        VIDEOSINK_FLUSH_SUCCESS(mVideoSinkController, true);
    }
}

/* Signal EOS, exptected EOS PTS callback is at mSignalEOSPTS */
VIDEOSINK_SIGNAL_EOS_SUCCESS(mVideoSinkController);
```

At the control point, wait up to five seconds for decoder EOS before notifying and flushing the sink. Final EOS follows queue drain, outside the loop.

### Attached decoder session lifetime

Location: [videodecoder/src/vts_videodecoder_test.cpp:451](../../../videodecoder/src/vts_videodecoder_test.cpp#L451) · `videoDecoderTest::startDecoding()`

```cpp
    // Preserve the completed decoder session until the attached sink
    // has reported presentation EOS. Decoder-only tests retain their
    // existing stop/start state-transition coverage.
    if (!mIsSinkAttached)
    {
        VIDEODECODER_STOP_SUCCESS(mDecoderHandle, mVideoDecoderController, true);
        VIDEODECODER_START_SUCCESS(mDecoderHandle, mVideoDecoderController, true);
    }
    // Exit the loop and not process any more frames after EOS is signaled.
    break;
}
```

Stop/start is performed only for decoder-only tests. An attached sink can consume presentation EOS while the completed decoder session and its last PTS remain available.

### Idempotent HAL EOS

Location: [hal/video-sink-soc-broadcom/src/VideoSinkControllerImpl.cpp:741](../../../../../../git/hal/video-sink-soc-broadcom/src/VideoSinkControllerImpl.cpp#L741) · `VideoSinkControllerImpl::signalEndOfStream()`

```cpp
::android::binder::Status VideoSinkControllerImpl::signalEndOfStream() {
    if (auto s = requireState(static_cast<int32_t>(State::STARTED),
                              "signalEndOfStream() requires STARTED state"); !s.isOk()) {
        return s;
    }
    // Tunnelled mode: the decoder drives the display, so the final PTS is the
    // last frame it output (-1 == IAVClock.UNDEFINED_TIME if none).
    if (mEosSignaled.exchange(true)) return ::android::binder::Status::ok();
    int64_t lastPts = -1;
```

State validation precedes the atomic exchange. The first call continues to emit EOS; subsequent calls return successfully without another event. flush() clears the flag.

### Callback completion deadline

Location: [videosink/src/vts_videosink_test.cpp:237](../../src/vts_videosink_test.cpp#L237) · `VideoSinkTest::validateCallbacks()`

```cpp
const size_t expectedEosCount = mTestConfiguration.eosPts >= 0 ? 2 : 1;
const int expectedFlushCount = (mTestConfiguration.flushPts >= 0 ? 1 : 0) +
                               (mTestConfiguration.eosPts >= 0 ? 1 : 0);
const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
while (std::chrono::steady_clock::now() < deadline)
{
    const bool resumeReady = mTestConfiguration.sink_underflow_pts < 0 ||
        (mVideoSinkControlListener->getResumedCount() > 0 &&
         mVideoSinkEventListener->getResumedCount() > 0);
    if (mVideoSinkControlListener->getEosPTS().size() >= expectedEosCount &&
        mVideoSinkEventListener->getEosPTS().size() >= expectedEosCount &&
        mVideoSinkControlListener->getFlushCompleteCount() >= expectedFlushCount &&
        mVideoSinkEventListener->getFlushCompleteCount() >= expectedFlushCount && resumeReady)
        break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
```

Wait up to two seconds for both listeners to reach expected EOS/flush counts and required resume notifications. Final assertions still enforce exact counts.

### Discard request and listener assertions

Location: [videosink/src/vts_videosink_test.cpp:148](../../src/vts_videosink_test.cpp#L148) · `VideoSinkTest::setupVideoSink()`

```cpp
if (mTestConfiguration.sink_discardFramesUntilPts >= 0)
{
    mFirstFramePTS = static_cast<int64_t>(mTestConfiguration.sink_discardFramesUntilPts);
    VIDEOSINK_SIGNAL_DISCARDFRAMESTILL_SUCCESS(mVideoSinkController, static_cast<uint64_t>(mTestConfiguration.sink_discardFramesUntilPts));
}
```

Keep the requested threshold as the expected first-PTS lower bound and send that value to the sink.

### First-render lower-bound check

Location: [videosink/src/vts_videosink_test.cpp:261](../../src/vts_videosink_test.cpp#L261) · `VideoSinkTest::validateCallbacks()`

```cpp
/* Validate first frame rendered PTS */
std::vector<int64_t> firstFramePTS = mVideoSinkEventListener->getFirstPTS();
UT_ASSERT_EQUAL(firstFramePTS.size(), 1);
if (mTestConfiguration.sink_discardFramesUntilPts >= 0)
{
    UT_ASSERT_TRUE(!firstFramePTS.empty() &&
        firstFramePTS.front() >= mTestConfiguration.sink_discardFramesUntilPts);
}
else
{
    UT_ASSERT_EQUAL(firstFramePTS.empty() ? -1 : firstFramePTS.front(), mFirstFramePTS);
}

firstFramePTS = mVideoSinkControlListener->getFirstPTS();
UT_ASSERT_EQUAL(firstFramePTS.size(), 1);
if (mTestConfiguration.sink_discardFramesUntilPts >= 0)
{
    UT_ASSERT_TRUE(!firstFramePTS.empty() &&
        firstFramePTS.front() >= mTestConfiguration.sink_discardFramesUntilPts);
}
```

Both event and controller listeners must have exactly one first-render event. For discard, each reported PTS must be at or beyond the requested boundary.


## Underflow input control

Tunnel underflow tests install an optional input-pause hook on the shared
decoder helper. After submitting the frame at `sink_underflow_pts`,
the producer waits up to four seconds for both sink listeners to report
underflow, then resumes the remaining input. Timeout remains an assertion
failure. Decoder tests without the hook have no additional pause.

### Public pause-hook interface

Location: [videosink/src/vts_videosink_test.h:120](../../src/vts_videosink_test.h#L120) · `VideoSinkTest public methods`

```cpp
void run ( bool isTunnel );
bool getSinkPropertyInt(videosink::Property property, int32_t& outValue);
bool waitForInputUnderflow();
```

Expose the wait operation to the producer hook installed by the L3 fixture.

### Shared producer hook setter

Location: [videodecoder/src/vts_videodecoder.h:1761](../../../videodecoder/src/vts_videodecoder.h#L1761) · `videoDecoderTest::setInputPauseHook()`

```cpp
void setInputPauseHook(std::function<bool()> hook) { mInputPauseHook = std::move(hook); }
```

Store an optional callback returning whether sink underflow was observed. The matching private member is mInputPauseHook at line 1772.

### Sequential and concurrent hook wiring

Location: [videosink/src/vts_videosink_l3_tests.cpp:303](../../src/vts_videosink_l3_tests.cpp#L303) · `runSequentialSink()`

```cpp
if (isTunnel && decoderConfig.sink_underflow_pts >= 0)
{
    decodeTest.setInputPauseHook([&sinkTest] { return sinkTest.waitForInputUnderflow(); });
}
```

Sequential tunnel underflow tests install this callback. The concurrent path installs the corresponding per-instance callback at lines 431–435.

### Producer pause point

Location: [videodecoder/src/vts_videodecoder_test.cpp:463](../../../videodecoder/src/vts_videodecoder_test.cpp#L463) · `videoDecoderTest::startDecoding()`

```cpp
if (mIsSinkAttached && mTunnelMode &&
    mTestConfiguration.sink_underflow_pts == framePTS)
{
    // Pause the producer until both listeners observe real starvation.
    const bool underflowObserved = mInputPauseHook && mInputPauseHook();
    UT_ASSERT_TRUE(underflowObserved) << "No sink underflow while decoder input was paused";
}
```

Invoke the hook at the configured submitted PTS only for an attached tunnel sink. Missing or unsuccessful observation remains an assertion failure.

### Wait for both sink listeners

Location: [videosink/src/vts_videosink_test.cpp:222](../../src/vts_videosink_test.cpp#L222) · `VideoSinkTest::waitForInputUnderflow()`

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

Require both underflow counters before resuming input. Poll every 10 ms with a four-second deadline; timeout returns false.


## HAL display clipping

`discardFramesUntil()` calls
`NEXUS_SimpleVideoDecoder_SetStartPts()`, propagates SDK errors, and rejects
a missing decoder. This platform's SimpleDecoder forwards to the AVD
backend, which stores a pending start PTS when no decoder is open. With an
open decoder, it calls `BXVD_SetClipTime(eClipStartOnly)` and
`BXVD_SetTSMWaitForValidSTC()`. The vendor display manager clips presentation
while earlier pictures remain available as decode references.

Vendor implementation references:

- `nexus/modules/simple_decoder/src/nexus_simple_video_decoder.c`
- `nexus/modules/video_decoder/src/nexus_video_decoder.c`,
  `NEXUS_VideoDecoder_P_SetStartPts_Avd()`
- `magnum/portinginterface/xvd/include/bxvd.h`

Clipping uses wrapping 32-bit 45 kHz PTS. The requested nanoseconds round
upward: 83,333,000 ns becomes 3,750 ticks (83,333,333.33 ns).
After discard, a 20 ms monitor requires decoder `started`,
`firstPtsPassed`, and an eligible display PTS before reporting a first
frame. It reconstructs the wrap, rounds ns upward, and uses the display
PTS for clock priming. A first-frame event is emitted once; a later
discard does not repeat it. Sink start resets the local threshold.

### Nanoseconds to clipping ticks

Location: [hal/video-sink-soc-broadcom/src/VideoSinkControllerImpl.cpp:58](../../../../../../git/hal/video-sink-soc-broadcom/src/VideoSinkControllerImpl.cpp#L58) · `discardPts45k()`

```cpp
// Round up so a hardware tick below the requested ns boundary is excluded.
uint64_t discardPts45k(int64_t ns) {
    const auto value = static_cast<uint64_t>(ns);
    return (value / 1000000000ULL) * 45000ULL +
           ((value % 1000000000ULL) * 45000ULL + 999999999ULL) / 1000000000ULL;
}

```

Use 45 kHz hardware ticks and upward rounding to exclude ticks earlier than the nanosecond boundary. Quotient/remainder arithmetic avoids multiplying the full ns value.

### Hardware clipping and error propagation

Location: [hal/video-sink-soc-broadcom/src/VideoSinkControllerImpl.cpp:855](../../../../../../git/hal/video-sink-soc-broadcom/src/VideoSinkControllerImpl.cpp#L855) · `VideoSinkControllerImpl::discardFramesUntil()`

```cpp
const auto pts = static_cast<uint32_t>(discardPts45k(nsPresentationTime));
const NEXUS_Error rc = NEXUS_SimpleVideoDecoder_SetStartPts(
    decoder->getVideoDecoder(), pts);
if (rc != NEXUS_SUCCESS) {
    ALOGE("[VideoSinkCtrl] SetStartPts failed rc=%d", rc);
    return ::android::binder::Status::fromExceptionCode(
        ::android::binder::Status::EX_SERVICE_SPECIFIC,
        "discardFramesUntil() hardware clipping failed");
}
mDiscardUntilPts = nsPresentationTime;
```

Program the vendor display clipping point. Store the local threshold only after SDK success; SDK failure returns a service exception.

### Display-status first-frame observation

Location: [hal/video-sink-soc-broadcom/src/VideoSinkControllerImpl.cpp:283](../../../../../../git/hal/video-sink-soc-broadcom/src/VideoSinkControllerImpl.cpp#L283) · `VideoSinkControllerImpl::pollDiscardFirstFrame()`

```cpp
void VideoSinkControllerImpl::pollDiscardFirstFrame() {
    int64_t ns = -1;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mDiscardUntilPts < 0 || mFirstFrameSeen.load() || !mDecoderAttached) return;
        auto* decoder = BrcmDecoderRegistry::instance().lookup(mDecoderId.value);
        if (!decoder || !decoder->getVideoDecoder()) return;
        NEXUS_VideoDecoderStatus status{};
        if (NEXUS_SimpleVideoDecoder_GetStatus(decoder->getVideoDecoder(), &status) ||
            !status.started || !status.firstPtsPassed) return;

        const uint64_t target = discardPts45k(mDiscardUntilPts);
        // MPEG presentation time wraps at 32 bits in the NEXUS 45 kHz API.
        // A negative modular difference is still before the clipping point.
        const uint32_t delta = status.pts - static_cast<uint32_t>(target);
        if (delta > 0x7fffffffU) return;
        const uint64_t ticks = target + delta;
        const uint64_t value = (ticks / 45000ULL) * 1000000000ULL +
            ((ticks % 45000ULL) * 1000000000ULL + 44999ULL) / 45000ULL;
        if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) return;
        ns = static_cast<int64_t>(value);
        mFirstFrameSeen.store(true);
    }
    // Report a confirmed displayed PTS, never a submitted/decode FIFO PTS.
    notifyFirstFrameRendered(ns);
}
```

Require active display status, reject a PTS before the boundary using modular comparison, reconstruct ns across wrap, and notify once after releasing the lock. This is a sampled display PTS.

### Decode observer gating

Location: [hal/video-sink-soc-broadcom/src/VideoSinkControllerImpl.cpp:273](../../../../../../git/hal/video-sink-soc-broadcom/src/VideoSinkControllerImpl.cpp#L273) · `VideoSinkControllerImpl::onDecoderFirstFrame()`

```cpp
void VideoSinkControllerImpl::onDecoderFirstFrame(int64_t nsPresentationTime) {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        // Decode-order notifications cannot identify the first displayed
        // picture after hardware clipping. The monitor reads display status.
        if (mDiscardUntilPts >= 0 || mFirstFrameSeen.exchange(true)) return;
    }
    notifyFirstFrameRendered(nsPresentationTime);
}
```

With a discard threshold set, use display-status observation instead of decode-order notification. Without discard, atomic first-frame tracking permits a single notification. The threshold member is declared in VideoSinkControllerImpl.h at line 127, reset in start() at line 603, and polled by the monitor at line 333.

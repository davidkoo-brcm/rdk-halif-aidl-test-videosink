/**
 *  If not stated otherwise in this file or this component's LICENSE
 *  file the following copyright and licenses apply:
 *
 *  Copyright 2025 RDK Management
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

/** @brief
 * VideoSink.h
 *
 * Header file for the VideoSink service implementation
 */

 #ifndef VIDEOSINK_H
 #define VIDEOSINK_H

 #include <binder/BinderService.h>
 #include <com/rdk/hal/AVSource.h>
 #include <com/rdk/hal/PropertyValue.h>
 #include <com/rdk/hal/avclock/IAVClock.h>
 #include <com/rdk/hal/videosink/BnVideoSink.h>
 #include <com/rdk/hal/videosink/BnVideoSinkController.h>
 #include <com/rdk/hal/videosink/BnVideoSinkManager.h>
 #include <com/rdk/hal/videosink/IVideoSink.h>
 #include <com/rdk/hal/videosink/IVideoSinkControllerListener.h>
 #include <com/rdk/hal/videosink/IVideoSinkEventListener.h>
 #include <com/rdk/hal/videosink/IVideoSinkManager.h>
 #include <com/rdk/hal/videosink/State.h>
 #include <condition_variable>
 #include <cstdint>
 #include <mutex>
 #include <optional>
 #include <queue>
 #include <string>
 #include <utility>
 #include <utils/StrongPointer.h>
 #include <vector>

 namespace com {
 namespace rdk {
 namespace hal {
 namespace videosink {

 static constexpr int32_t NUM_VIDEO_SINK = 2;
 static constexpr int32_t MAX_QUEUE_DEPTH = 10;

 // Global state declarations
 extern State g_videosink_state[NUM_VIDEO_SINK];
 extern videodecoder::IVideoDecoder::Id g_videodecoder_id[NUM_VIDEO_SINK];
 extern int32_t g_resource_id[NUM_VIDEO_SINK];
 extern int32_t g_sink_queue_depth[NUM_VIDEO_SINK];
 extern AVSource g_av_source[NUM_VIDEO_SINK];
 extern int32_t g_render_first_frame[NUM_VIDEO_SINK];
 extern std::queue<std::pair<int64_t, int64_t>> g_frame_queue[NUM_VIDEO_SINK]; // nsPresentationTime, frameBufferHandle
 extern std::mutex state_mutex;
 extern std::condition_variable state_cv;

 class VideoSinkManager : public android::BinderService<VideoSinkManager>,
                          public BnVideoSinkManager
{
public:
    VideoSinkManager();
    virtual ~VideoSinkManager();

    static char const* getServiceName() { return IVideoSinkManager::serviceName().c_str(); }

    void reset();

    android::binder::Status getVideoSinkIds(std::vector<IVideoSink::Id>* _aidl_return) override;
    android::binder::Status getVideoSink(const IVideoSink::Id& videoSinkId, android::sp<IVideoSink>* _aidl_return) override;
};

class VideoSink : public android::BinderService<VideoSink>,
                public BnVideoSink
{
private:
    IVideoSink::Id id;
    android::sp<IVideoSinkEventListener> eventListener;
    android::sp<IVideoSinkControllerListener> controllerListener;

public:
    VideoSink(IVideoSink::Id id);
    virtual ~VideoSink();

    android::binder::Status getCapabilities(videosink::Capabilities* _aidl_return) override;
    android::binder::Status getProperty(videosink::Property property, std::optional<PropertyValue>* _aidl_return) override;
    android::binder::Status getState(State* _aidl_return) override;
    android::binder::Status open(const android::sp<videosink::IVideoSinkControllerListener>& videoSinkControllerListener, android::sp<videosink::IVideoSinkController>* _aidl_return) override;
    android::binder::Status close(const android::sp<videosink::IVideoSinkController>& videoSinkController, bool* _aidl_return) override;
    android::binder::Status registerEventListener(const android::sp<videosink::IVideoSinkEventListener>& videoSinkEventListener, bool* _aidl_return) override;
    android::binder::Status unregisterEventListener(const android::sp<videosink::IVideoSinkEventListener>& videoSinkEventListener, bool* _aidl_return) override;
};

class VideoSinkController : public android::BinderService<VideoSinkController>,
                        public BnVideoSinkController
{
private:
    IVideoSink::Id id;

public:
    explicit VideoSinkController(IVideoSink::Id id);
    virtual ~VideoSinkController();

    android::binder::Status setVideoDecoder(const ::com::rdk::hal::videodecoder::IVideoDecoder::Id& videoDecoderId, bool* _aidl_return) override;
    android::binder::Status getVideoDecoder(::com::rdk::hal::videodecoder::IVideoDecoder::Id* _aidl_return) override;
    android::binder::Status attachClock(const ::com::rdk::hal::avclock::IAVClock::Id& clockId) override;
    android::binder::Status detachClock() override;
    android::binder::Status getClock(::com::rdk::hal::avclock::IAVClock::Id* _aidl_return) override;
    android::binder::Status start() override;
    android::binder::Status stop() override;
    android::binder::Status queueVideoFrame(int64_t nsPresentationTime, int64_t frameBufferHandle, const ::com::rdk::hal::videodecoder::FrameMetadata& metadata, bool* _aidl_return) override;
    android::binder::Status signalEndOfStream() override;
    android::binder::Status flush(bool holdLastFrame) override;
    android::binder::Status discardFramesUntil(int64_t nsPresentationTime) override;
    android::binder::Status setProperty(videosink::Property property, const PropertyValue& propertyValue, bool* _aidl_return) override;
};

 } // namespace videosink
 } // namespace hal
 } // namespace rdk
 } // namespace com

 #endif // VIDEOSINK_H
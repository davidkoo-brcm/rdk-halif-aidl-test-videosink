/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

/** @brief
 * VideoSink.cpp
 *
 * Implementation of VideoSink service
 */

 #include "VideoSink.h"
 #include <algorithm>
 #include <chrono>
 #include <thread>
 #include <queue>
 #include <android/log.h>
 #include <com/rdk/hal/videosink/IVideoSink.h> // Ensure IVideoSink::Id is included
 #include <com/rdk/hal/State.h> // For State enum
 #include <com/rdk/hal/PropertyValue.h> // For PropertyValue
 #include <com/rdk/hal/AVSource.h> // For AVSource enum
 #include <com/rdk/hal/videosink/Capabilities.h> // For Capabilities
 #include <com/rdk/hal/videodecoder/FrameMetadata.h> // For FrameMetadata

 #define LOG_TAG "VideoSink"
 #define LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
 #define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

 namespace com {
 namespace rdk {
 namespace hal {
 namespace videosink {

 // Global state
 State g_videosink_state[NUM_VIDEO_SINK] = {State::UNKNOWN, State::UNKNOWN};
 videodecoder::IVideoDecoder::Id g_videodecoder_id[NUM_VIDEO_SINK];
 int32_t g_resource_id[NUM_VIDEO_SINK];
 int32_t g_sink_queue_depth[NUM_VIDEO_SINK];
 AVSource g_av_source[NUM_VIDEO_SINK];
 int32_t g_render_first_frame[NUM_VIDEO_SINK];
 std::queue<std::pair<int64_t, int64_t>> g_frame_queue[NUM_VIDEO_SINK]; // nsPresentationTime, frameBufferHandle

 std::mutex state_mutex;
 std::condition_variable state_cv;

 VideoSinkManager::VideoSinkManager() {
     reset();
     LOG_INFO("VideoSinkManager service initialized");
 }

 VideoSinkManager::~VideoSinkManager() {
     LOG_INFO("VideoSinkManager service destroyed");
 }

 void VideoSinkManager::reset() {
     std::lock_guard<std::mutex> lock(state_mutex);
     for (int32_t i = 0; i < NUM_VIDEO_SINK; i++) {
         g_videosink_state[i] = State::CLOSED;
         g_av_source[i] = AVSource::UNKNOWN;
         g_videodecoder_id[i].value = videodecoder::IVideoDecoder::Id::UNDEFINED;
         g_resource_id[i] = i;
         g_render_first_frame[i] = 0;
         g_sink_queue_depth[i] = 0;
         while (!g_frame_queue[i].empty()) {
             g_frame_queue[i].pop();
         }
     }
 }

 android::binder::Status VideoSinkManager::getVideoSinkIds(std::vector<IVideoSink::Id>* _aidl_return) {
     if (!_aidl_return) {
         LOG_ERROR("getVideoSinkIds: null _aidl_return");
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
     }
     _aidl_return->clear();
     for (int32_t i = 0; i < NUM_VIDEO_SINK; i++) {
         IVideoSink::Id sinkId;
         sinkId.value = g_resource_id[i];
         _aidl_return->push_back(sinkId);
     }
     return android::binder::Status::ok();
 }

 android::binder::Status VideoSinkManager::getVideoSink(const IVideoSink::Id& videoSinkId, android::sp<IVideoSink>* _aidl_return) {
     if (!_aidl_return) {
         LOG_ERROR("getVideoSink: null _aidl_return");
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
     }
     if (videoSinkId.value < 0 || videoSinkId.value >= NUM_VIDEO_SINK) {
         LOG_ERROR("getVideoSink: invalid videoSinkId %d", videoSinkId.value);
         *_aidl_return = nullptr;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     *_aidl_return = new VideoSink(videoSinkId);
     return android::binder::Status::ok();
 }

 VideoSink::VideoSink(IVideoSink::Id id) : id(id) {
     eventListener = nullptr;
     LOG_INFO("VideoSink service initialized for ID %d", id.value);
 }

 VideoSink::~VideoSink() {
     LOG_INFO("VideoSink service destroyed for ID %d", id.value);
 }

 android::binder::Status VideoSink::getCapabilities(videosink::Capabilities* _aidl_return) {
     if (!_aidl_return) {
         LOG_ERROR("getCapabilities: null _aidl_return for ID %d", id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
     }
     // Mock capabilities
     videosink::Capabilities capabilities;
//     capabilities.vsyncDisplayLatency = 16000000; // 16ms
     // Add more fields as needed (e.g., supported resolutions, formats)
     *_aidl_return = capabilities;
     return android::binder::Status::ok();
 }

 android::binder::Status VideoSink::getProperty(videosink::Property property, std::optional<PropertyValue>* _aidl_return) {
     if (!_aidl_return) {
         LOG_ERROR("getProperty: null _aidl_return for ID %d", id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
     }
     if (id.value < 0 || id.value >= NUM_VIDEO_SINK) {
         LOG_ERROR("getProperty: invalid ID %d", id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     PropertyValue value;
     switch (property) {
         case videosink::Property::RESOURCE_ID:
             value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(id.value);
             break;
         case videosink::Property::AV_SOURCE:
             value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(static_cast<int32_t>(g_av_source[id.value]));
             break;
         case videosink::Property::SINK_QUEUE_DEPTH:
             value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(g_sink_queue_depth[id.value]);
             break;
         case videosink::Property::RENDER_FIRST_FRAME:
             value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(g_render_first_frame[id.value]);
             break;
         case videosink::Property::METRIC_FRAMES_RECEIVED:
             value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(g_sink_queue_depth[id.value]); // Mock
             break;
         case videosink::Property::METRIC_FRAMES_PRESENTED:
             value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(g_sink_queue_depth[id.value] / 2); // Mock
             break;
         case videosink::Property::METRIC_FRAMES_DROPPED_LATE:
             value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(0); // Mock
             break;
         case videosink::Property::METRIC_FRAMES_DROPPED_FRC:
             value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(0); // Mock
             break;
         case videosink::Property::METRIC_FRAMES_REPEATED_FRC:
             value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(0); // Mock
             break;
         case videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME:
             value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(0); // Mock
             break;
         case videosink::Property::METRIC_UNDERFLOWED:
             value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(g_frame_queue[id.value].empty() ? 1 : 0); // Mock
             break;
         default:
             LOG_ERROR("getProperty: invalid property %d for ID %d", static_cast<int>(property), id.value);
             return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     *_aidl_return = value;
     return android::binder::Status::ok();
 }

 android::binder::Status VideoSink::getState(State* _aidl_return) {
     if (!_aidl_return) {
         LOG_ERROR("getState: null _aidl_return for ID %d", id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
     }
     if (id.value < 0 || id.value >= NUM_VIDEO_SINK) {
         LOG_ERROR("getState: invalid ID %d", id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     *_aidl_return = g_videosink_state[id.value];
     return android::binder::Status::ok();
 }



// VideoSink::open
android::binder::Status VideoSink::open(const android::sp<videosink::IVideoSinkControllerListener>& videoSinkControllerListener, android::sp<IVideoSinkController>* _aidl_return) {
        if (!videoSinkControllerListener) {
        LOG_ERROR("open: null videoSinkControllerListener for ID %d", id.value);
        *_aidl_return = nullptr;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
        }
        if (id.value < 0 || id.value >= NUM_VIDEO_SINK) {
        LOG_ERROR("open: invalid ID %d", id.value);
        *_aidl_return = nullptr;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
        }
        std::lock_guard<std::mutex> lock(state_mutex);
        if (g_videosink_state[id.value] != State::CLOSED) {
        LOG_ERROR("open: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
        *_aidl_return = nullptr;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
        }
        controllerListener = videoSinkControllerListener;
        g_videosink_state[id.value] = State::OPENING;
        LOG_INFO("open: ID %d transitioning CLOSED -> OPENING", id.value);
        if (eventListener) {
        eventListener->onStateChanged(State::CLOSED, State::OPENING);
        }
        controllerListener->onStateChanged(State::CLOSED, State::OPENING);
        state_cv.notify_all();

        // Simulate state transition
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        g_videosink_state[id.value] = State::READY;
        LOG_INFO("open: ID %d transitioning OPENING -> READY", id.value);
        if (eventListener) {
        eventListener->onStateChanged(State::OPENING, State::READY);
        }
        controllerListener->onStateChanged(State::OPENING, State::READY);
        state_cv.notify_all();

        *_aidl_return = new VideoSinkController(id, eventListener, controllerListener); // Pass listeners
        return android::binder::Status::ok();
}
 android::binder::Status VideoSink::close(const android::sp<videosink::IVideoSinkController>& videoSinkController, bool* _aidl_return) {
     if (!videoSinkController) {
         LOG_ERROR("close: null videoSinkController for ID %d", id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
     }
     if (id.value < 0 || id.value >= NUM_VIDEO_SINK) {
         LOG_ERROR("close: invalid ID %d", id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     std::lock_guard<std::mutex> lock(state_mutex);
     if (g_videosink_state[id.value] != State::READY) {
         LOG_ERROR("close: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
     }
     g_videosink_state[id.value] = State::CLOSING;
     LOG_INFO("close: ID %d transitioning READY -> CLOSING", id.value);
     if (eventListener) {
         eventListener->onStateChanged(State::READY, State::CLOSING);
     }
     if (controllerListener) {
         controllerListener->onStateChanged(State::READY, State::CLOSING);
     }
     state_cv.notify_all();

     // Simulate state transition
     std::this_thread::sleep_for(std::chrono::milliseconds(5));
     g_videosink_state[id.value] = State::CLOSED;
     LOG_INFO("close: ID %d transitioning CLOSING -> CLOSED", id.value);
     if (eventListener) {
         eventListener->onStateChanged(State::CLOSING, State::CLOSED);
     }
     if (controllerListener) {
         controllerListener->onStateChanged(State::CLOSING, State::CLOSED);
     }
     state_cv.notify_all();

     controllerListener = nullptr;
     *_aidl_return = true;
     return android::binder::Status::ok();
 }

 android::binder::Status VideoSink::registerEventListener(const android::sp<videosink::IVideoSinkEventListener>& videoSinkEventListener, bool* _aidl_return) {
     if (!videoSinkEventListener) {
         LOG_ERROR("registerEventListener: null videoSinkEventListener for ID %d", id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
     }
     if (id.value < 0 || id.value >= NUM_VIDEO_SINK) {
         LOG_ERROR("registerEventListener: invalid ID %d", id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     if (eventListener) {
         LOG_ERROR("registerEventListener: listener already registered for ID %d", id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
     }
     eventListener = videoSinkEventListener;
     *_aidl_return = true;
     LOG_INFO("registerEventListener: registered for ID %d", id.value);
     return android::binder::Status::ok();
 }

 android::binder::Status VideoSink::unregisterEventListener(const android::sp<videosink::IVideoSinkEventListener>& videoSinkEventListener, bool* _aidl_return) {
     if (!videoSinkEventListener) {
         LOG_ERROR("unregisterEventListener: null videoSinkEventListener for ID %d", id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
     }
     if (id.value < 0 || id.value >= NUM_VIDEO_SINK) {
         LOG_ERROR("unregisterEventListener: invalid ID %d", id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     if (!eventListener) {
         LOG_ERROR("unregisterEventListener: no listener registered for ID %d", id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
     }
     eventListener = nullptr;
     *_aidl_return = true;
     LOG_INFO("unregisterEventListener: unregistered for ID %d", id.value);
     return android::binder::Status::ok();
 }

// VideoSinkController constructor
VideoSinkController::VideoSinkController(IVideoSink::Id id,
        const android::sp<IVideoSinkEventListener>& eventListener,
        const android::sp<IVideoSinkControllerListener>& controllerListener)
    : id(id), eventListener(eventListener), controllerListener(controllerListener) {
LOG_INFO("VideoSinkController service initialized for ID %d", id.value);
}



 VideoSinkController::~VideoSinkController() {
     LOG_INFO("VideoSinkController service destroyed for ID %d", id.value);
 }

 android::binder::Status VideoSinkController::setProperty(videosink::Property property, const PropertyValue& propertyValue, bool* _aidl_return) {
     if (!propertyValue.value) {
         LOG_ERROR("setProperty: null propertyValue for ID %d", id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     if (id.value < 0 || id.value >= NUM_VIDEO_SINK) {
         LOG_ERROR("setProperty: invalid ID %d", id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     std::lock_guard<std::mutex> lock(state_mutex);
     switch (property) {
         case videosink::Property::AV_SOURCE: {
             if (g_videosink_state[id.value] != State::READY) {
                 LOG_ERROR("setProperty: invalid state %d for AV_SOURCE ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
                 *_aidl_return = false;
                 return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
             }
             int32_t intValue = propertyValue.value->get<PropertyValue::Value::Tag::intValue>();
             LOG_INFO("setProperty: AV_SOURCE = %d for ID %d", intValue, id.value);
             if (intValue < static_cast<int32_t>(AVSource::AUTO) || intValue > static_cast<int32_t>(AVSource::COMPOSITE_5)) {
                 LOG_ERROR("setProperty: invalid AV_SOURCE value %d for ID %d", intValue, id.value);
                 *_aidl_return = false;
                 return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
             }
             g_av_source[id.value] = static_cast<AVSource>(intValue);
             break;
         }
         case videosink::Property::RENDER_FIRST_FRAME: {
             int32_t intValue = propertyValue.value->get<PropertyValue::Value::Tag::intValue>();
             LOG_INFO("setProperty: RENDER_FIRST_FRAME = %d for ID %d", intValue, id.value);
             if (intValue < 0 || intValue > 1) {
                 LOG_ERROR("setProperty: invalid RENDER_FIRST_FRAME value %d for ID %d", intValue, id.value);
                 *_aidl_return = false;
                 return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
             }
             g_render_first_frame[id.value] = intValue;
             break;
         }
         default:
             LOG_ERROR("setProperty: invalid property %d for ID %d", static_cast<int>(property), id.value);
             *_aidl_return = false;
             return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     *_aidl_return = true;
     return android::binder::Status::ok();
 }

 android::binder::Status VideoSinkController::setVideoDecoder(const videodecoder::IVideoDecoder::Id& videoDecoderId, bool* _aidl_return) {
     if (id.value < 0 || id.value >= NUM_VIDEO_SINK) {
         LOG_ERROR("setVideoDecoder: invalid ID %d", id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     std::lock_guard<std::mutex> lock(state_mutex);
     if (g_videosink_state[id.value] != State::READY) {
         LOG_ERROR("setVideoDecoder: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
     }
     if (videoDecoderId.value < 0) {
         LOG_ERROR("setVideoDecoder: invalid videoDecoderId %d for ID %d", videoDecoderId.value, id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     g_videodecoder_id[id.value] = videoDecoderId;
     *_aidl_return = true;
     LOG_INFO("setVideoDecoder: set ID %d to decoder %d", id.value, videoDecoderId.value);
     return android::binder::Status::ok();
 }

 android::binder::Status VideoSinkController::getVideoDecoder(videodecoder::IVideoDecoder::Id* _aidl_return) {
     if (!_aidl_return) {
         LOG_ERROR("getVideoDecoder: null _aidl_return for ID %d", id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
     }
     if (id.value < 0 || id.value >= NUM_VIDEO_SINK) {
         LOG_ERROR("getVideoDecoder: invalid ID %d", id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     std::lock_guard<std::mutex> lock(state_mutex);
     if (g_videosink_state[id.value] != State::READY && g_videosink_state[id.value] != State::STARTED) {
         LOG_ERROR("getVideoDecoder: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
     }
     *_aidl_return = g_videodecoder_id[id.value];
     return android::binder::Status::ok();
 }

 android::binder::Status VideoSinkController::start() {
     if (id.value < 0 || id.value >= NUM_VIDEO_SINK) {
         LOG_ERROR("start: invalid ID %d", id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     std::lock_guard<std::mutex> lock(state_mutex);
     if (g_videosink_state[id.value] != State::READY) {
         LOG_ERROR("start: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
     }
     g_videosink_state[id.value] = State::STARTING;
     LOG_INFO("start: ID %d transitioning READY -> STARTING", id.value);
     if (eventListener) {
         eventListener->onStateChanged(State::READY, State::STARTING);
     }
     if (controllerListener) {
         controllerListener->onStateChanged(State::READY, State::STARTING);
     }
     state_cv.notify_all();

     // Simulate state transition
     std::this_thread::sleep_for(std::chrono::milliseconds(5));
     g_videosink_state[id.value] = State::STARTED;
     LOG_INFO("start: ID %d transitioning STARTING -> STARTED", id.value);
     if (eventListener) {
         eventListener->onStateChanged(State::STARTING, State::STARTED);
     }
     if (controllerListener) {
         controllerListener->onStateChanged(State::STARTING, State::STARTED);
     }
     state_cv.notify_all();
     return android::binder::Status::ok();
 }

 android::binder::Status VideoSinkController::stop() {
     if (id.value < 0 || id.value >= NUM_VIDEO_SINK) {
         LOG_ERROR("stop: invalid ID %d", id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     std::lock_guard<std::mutex> lock(state_mutex);
     if (g_videosink_state[id.value] != State::STARTED) {
         LOG_ERROR("stop: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
     }
     g_videosink_state[id.value] = State::STOPPING;
     LOG_INFO("stop: ID %d transitioning STARTED -> STOPPING", id.value);
     if (eventListener) {
         eventListener->onStateChanged(State::STARTED, State::STOPPING);
     }
     if (controllerListener) {
         controllerListener->onStateChanged(State::STARTED, State::STOPPING);
     }
     state_cv.notify_all();

     // Simulate state transition
     std::this_thread::sleep_for(std::chrono::milliseconds(5));
     g_videosink_state[id.value] = State::READY;
     LOG_INFO("stop: ID %d transitioning STOPPING -> READY", id.value);
     if (eventListener) {
         eventListener->onStateChanged(State::STOPPING, State::READY);
     }
     if (controllerListener) {
         controllerListener->onStateChanged(State::STOPPING, State::READY);
     }
     state_cv.notify_all();
     return android::binder::Status::ok();
 }

 android::binder::Status VideoSinkController::queueVideoFrame(int64_t nsPresentationTime, int64_t frameBufferHandle, const videodecoder::FrameMetadata& metadata, bool* _aidl_return) {
     if (id.value < 0 || id.value >= NUM_VIDEO_SINK) {
         LOG_ERROR("queueVideoFrame: invalid ID %d", id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     std::lock_guard<std::mutex> lock(state_mutex);
     if (g_videosink_state[id.value] != State::STARTED) {
         LOG_ERROR("queueVideoFrame: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
     }
     if (nsPresentationTime <= 0) {
         LOG_ERROR("queueVideoFrame: invalid nsPresentationTime %ld for ID %d", nsPresentationTime, id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     if (frameBufferHandle <= 0) {
         LOG_ERROR("queueVideoFrame: invalid frameBufferHandle %ld for ID %d", frameBufferHandle, id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     if (g_sink_queue_depth[id.value] >= MAX_QUEUE_DEPTH) {
         LOG_ERROR("queueVideoFrame: queue full for ID %d", id.value);
         *_aidl_return = false;
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_UNSUPPORTED_OPERATION);
     }
     g_frame_queue[id.value].push({nsPresentationTime, frameBufferHandle});
     g_sink_queue_depth[id.value]++;
     LOG_INFO("queueVideoFrame: queued frame with nsPresentationTime %ld for ID %d", nsPresentationTime, id.value);

     // Simulate rendering first frame
     if (g_sink_queue_depth[id.value] == 1 && g_render_first_frame[id.value]) {
         if (eventListener) {
             eventListener->onFirstFrameRendered(nsPresentationTime);
         }
         if (controllerListener) {
             controllerListener->onFirstFrameRendered(nsPresentationTime);
         }
     }
     // Simulate underflow detection
     if (g_sink_queue_depth[id.value] == 1 && g_frame_queue[id.value].empty()) {
         if (eventListener) {
             eventListener->onVideoUnderflow();
         }
         if (controllerListener) {
             controllerListener->onVideoUnderflow();
         }
     }
     *_aidl_return = true;
     return android::binder::Status::ok();
 }

 android::binder::Status VideoSinkController::flush(bool holdLastFrame) {
     if (id.value < 0 || id.value >= NUM_VIDEO_SINK) {
         LOG_ERROR("flush: invalid ID %d", id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     std::lock_guard<std::mutex> lock(state_mutex);
     if (g_videosink_state[id.value] != State::STARTED) {
         LOG_ERROR("flush: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
     }
     g_videosink_state[id.value] = State::FLUSHING;
     LOG_INFO("flush: ID %d transitioning STARTED -> FLUSHING", id.value);
     if (eventListener) {
         eventListener->onStateChanged(State::STARTED, State::FLUSHING);
     }
     if (controllerListener) {
         controllerListener->onStateChanged(State::STARTED, State::FLUSHING);
     }
     state_cv.notify_all();

     // Simulate flush
     while (!g_frame_queue[id.value].empty()) {
         g_frame_queue[id.value].pop();
     }
     g_sink_queue_depth[id.value] = 0;
     if (holdLastFrame) {
         LOG_INFO("flush: holding last frame for ID %d", id.value);
         // Simulate holding last frame (e.g., keep last frameBufferHandle)
     }

     // Simulate state transition
     std::this_thread::sleep_for(std::chrono::milliseconds(5));
     g_videosink_state[id.value] = State::STARTED;
     LOG_INFO("flush: ID %d transitioning FLUSHING -> STARTED", id.value);
     if (eventListener) {
         eventListener->onStateChanged(State::FLUSHING, State::STARTED);
         eventListener->onFlushComplete();
     }
     if (controllerListener) {
         controllerListener->onStateChanged(State::FLUSHING, State::STARTED);
         controllerListener->onFlushComplete();
     }
     state_cv.notify_all();
     return android::binder::Status::ok();
 }

 android::binder::Status VideoSinkController::discardFramesUntil(int64_t nsPresentationTime) {
     if (id.value < 0 || id.value >= NUM_VIDEO_SINK) {
         LOG_ERROR("discardFramesUntil: invalid ID %d", id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     std::lock_guard<std::mutex> lock(state_mutex);
     if (g_videosink_state[id.value] != State::STARTED) {
         LOG_ERROR("discardFramesUntil: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
     }
     if (nsPresentationTime < 0) {
         LOG_ERROR("discardFramesUntil: invalid nsPresentationTime %ld for ID %d", nsPresentationTime, id.value);
         return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
     }
     // Discard frames before nsPresentationTime
     while (!g_frame_queue[id.value].empty() && g_frame_queue[id.value].front().first < nsPresentationTime) {
         g_frame_queue[id.value].pop();
         g_sink_queue_depth[id.value]--;
     }
     LOG_INFO("discardFramesUntil: discarded frames until %ld for ID %d", nsPresentationTime, id.value);
     // Simulate underflow if queue becomes empty

     /*if(g_frame_queue[id.value].empty()) {
        if (eventListener) {
             eventListener->onVideoUnderflow();
         }
         if (controllerListener) {
             controllerListener->onVideoUnderflow();
         }
     }*/
     return android::binder::Status::ok();
 }

 } // namespace videosink
 } // namespace hal
 } // namespace rdk
 } // namespace com
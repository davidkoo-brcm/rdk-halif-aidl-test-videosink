/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

namespace com {
    namespace rdk {
        namespace hal {
            namespace videosink {

                State g_videosink_state[NUM_VIDEO_SINK] = {State::UNKNOWN, State::UNKNOWN};
                videodecoder::IVideoDecoder::Id g_videodecoder_id[NUM_VIDEO_SINK];
                android::sp<videosink::IVideoSinkEventListener> _videoSinkEventListener;
                int32_t g_resource_id[NUM_VIDEO_SINK];
                int32_t g_sink_queue_depth[NUM_VIDEO_SINK];

                /* Read Write Properties */
                AVSource g_av_source[NUM_VIDEO_SINK];
                int32_t g_render_first_frame[NUM_VIDEO_SINK];

                std::mutex state_mutex;
                std::condition_variable state_cv;

                VideoSinkManager::VideoSinkManager()
                {
                    for (int32_t i = 0;i<NUM_VIDEO_SINK;i++)
                    {
                        g_av_source[i] = AVSource::UNKNOWN;
                        g_videodecoder_id[i].value = videodecoder::IVideoDecoder::Id::UNDEFINED;
                        g_resource_id[i] = i;
                        g_render_first_frame[i] = 0;
                        g_sink_queue_depth[i] = 0;
                    }
                    printf("VideoSinkManager service initialized\n");
                }

                VideoSinkManager::~VideoSinkManager()
                {
                    printf("VideoSinkManager service destroyed\n");
                }

                android::binder::Status VideoSinkManager::getVideoSinkIds(std::vector<IVideoSink::Id>* _aidl_return)
                {
                    for (int32_t i = 0;i<NUM_VIDEO_SINK;i++)
                    {
                        videosink::IVideoSink::Id sinkId;
                        sinkId.value = g_resource_id[i];
                        _aidl_return->push_back(sinkId);
                    }
                    return android::binder::Status::ok();
                }

                android::binder::Status VideoSinkManager::getVideoSink(const IVideoSink::Id& videoSinkId, android::sp<IVideoSink>* _aidl_return)
                {
                    int32_t found = 0;
                    for(int32_t i = 0; i < NUM_VIDEO_SINK; i++)
                    {
                        if(g_resource_id[i] == videoSinkId.value)
                        {
                            found = 1;
                            break;
                        }
                    }
                    if(!found) {
                        *_aidl_return = nullptr;
                        return android::binder::Status::fromExceptionCode(
                                android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }
                    android::sp<IVideoSink> videoSink = new VideoSink(videoSinkId);
                    g_videosink_state[videoSinkId.value] = State::CLOSED;
                    *_aidl_return = videoSink;
                    return android::binder::Status::ok();
                }

                VideoSink::VideoSink(IVideoSink::Id id)
                {
                    this->id = id;
                    this->event_listener = false;
                    this->eventListener = nullptr;
                    printf("VideoSink service initialized\n");
                }

                VideoSink::~VideoSink() {
                    printf("VideoSink service destroyed\n");
                }

                android::binder::Status VideoSink::getCapabilities(videosink::Capabilities* _aidl_return)
                {
                    videosink::Capabilities capabilities;
                    //TBD
                    *_aidl_return = capabilities;
                    return android::binder::Status::ok();
                }

                android::binder::Status VideoSink::getProperty(videosink::Property property, std::optional<PropertyValue>* _aidl_return)
                {
                    if (!_aidl_return)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
                    }
                    com::rdk::hal::PropertyValue value;
                    switch (property) {
                        case videosink::Property::RESOURCE_ID:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(this->id.value);
                            break;
                        case videosink::Property::AV_SOURCE:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(static_cast<int32_t>(g_av_source[this->id.value]));
                            break;
                        case videosink::Property::SINK_QUEUE_DEPTH:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(g_sink_queue_depth[this->id.value]);
                            break;
                        case videosink::Property::RENDER_FIRST_FRAME:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(g_render_first_frame[this->id.value]);
                            break;
                        case videosink::Property::METRIC_FRAMES_RECEIVED:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(-1);
                            break;
                        case videosink::Property::METRIC_FRAMES_PRESENTED:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(-1);
                            break;
                        case videosink::Property::METRIC_FRAMES_DROPPED_LATE:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(-1);
                            break;
                        case videosink::Property::METRIC_FRAMES_DROPPED_FRC:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(-1);
                            break;
                        case videosink::Property::METRIC_FRAMES_REPEATED_FRC:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(-1);
                            break;
                        case videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(-1);
                            break;
                        case videosink::Property::METRIC_UNDERFLOWED:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(-1);
                            break;
                        default:
                            return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }
                    *_aidl_return = value;
                    return android::binder::Status::ok();
                }
                
                android::binder::Status VideoSink::getState(State* _aidl_return)
                {
                    *_aidl_return = g_videosink_state[this->id.value];
                    return android::binder::Status::ok();
                }
                
                android::binder::Status VideoSink::open(const android::sp<videosink::IVideoSinkControllerListener>& videoSinkControllerListener, android::sp<videosink::IVideoSinkController>* _aidl_return)
                {
                    if(videoSinkControllerListener == nullptr)
                    {
                        *_aidl_return = nullptr;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }
                    if(g_videosink_state[this->id.value] != State::CLOSED)
                    {
                        *_aidl_return = nullptr;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from CLOSED to OPENING...\n";
                            g_videosink_state[this->id.value] = State::OPENING;
                            std::cout << "[SERVICE]::g_videosink_state value :"<< static_cast<int>(g_videosink_state[this->id.value])<<"\n";//added
                        }
                        //average wait time to simulate transitory state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        std::cout << "[SERVICE]::Calling onStateChanged for OPENING...\n";
                        this->eventListener->onStateChanged(State::CLOSED, State::OPENING);
                        state_cv.notify_all();
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from OPENING to READY...\n";
                            g_videosink_state[this->id.value] = State::READY;
                            std::cout << "[SERVICE]::g_videosink_state value :"<< static_cast<int>(g_videosink_state[this->id.value])<<"\n";//added
                        }
                        //average wait time to simulate target state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        std::cout << "[SERVICE]::Calling onStateChanged for READY...\n";
                        this->eventListener->onStateChanged(State::OPENING, State::READY);
                        state_cv.notify_all();
                    }
                    *_aidl_return = new VideoSinkController(this->id);
                    _videoSinkEventListener = this->eventListener;
                    return android::binder::Status::ok();
                }
                
                android::binder::Status VideoSink::close(const android::sp<videosink::IVideoSinkController>& videoSinkController, bool* _aidl_return)
                {
                    if(g_videosink_state[this->id.value] != State::READY)
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from READY to CLOSING...\n";
                            g_videosink_state[this->id.value] = State::CLOSING;
                        }
                        //average wait time to simulate transitory state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        std::cout << "[SERVICE]::Calling onStateChanged for CLOSING...\n";
                        this->eventListener->onStateChanged(State::READY, State::CLOSING);
                        state_cv.notify_all();
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from CLOSING to CLOSED...\n";
                            g_videosink_state[this->id.value] = State::CLOSED;
                        }
                        //average wait time to simulate target state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        std::cout << "[SERVICE]::Calling onStateChanged for CLOSED...\n";
                        this->eventListener->onStateChanged(State::CLOSING, State::CLOSED);
                        state_cv.notify_all();
                    }
                    * _aidl_return = true;
                    return android::binder::Status::ok();
                }
                
                android::binder::Status VideoSink::registerEventListener(const android::sp<videosink::IVideoSinkEventListener>& videoSinkEventListener, bool* _aidl_return)
                {
                    if(this->event_listener == true)
                    {
                        std::cout << "[SERVICE]::eventListener for ID : " <<this->id.value<<" is already registered ...exiting...\n";//added
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
//                    _videoSinkEventListener = videoSinkEventListener;
                    this->eventListener = videoSinkEventListener;
                    *_aidl_return = true;
                    this->event_listener = true;
                    std::cout << "[SERVICE]::eventListener for ID : " <<this->id.value<<" is registered successfully :: eventListener value : " << this->event_listener<<"\n";//added
                    return android::binder::Status::ok();
                }
                
                android::binder::Status VideoSink::unregisterEventListener(const android::sp<videosink::IVideoSinkEventListener>& videoSinkEventListener, bool* _aidl_return)
                {
                    if(this->event_listener ==false)
                    {
                        std::cout << "[SERVICE]::eventListener for ID : " <<this->id.value<<" is already unregistered ...exiting...\n";//added
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    *_aidl_return = true;
                    this->eventListener = nullptr;
                    std::cout << "[SERVICE]::eventListener for ID : " <<this->id.value<<" is unregistered successfully:: eventListener value : " << this->event_listener << "\n";//added
                    this->event_listener = false;
                    return android::binder::Status::ok();
                }

                VideoSinkController::VideoSinkController(IVideoSink::Id id)
                {
                    this->id = id;
                    printf("VideoSinkController service initialized\n");
                }

                VideoSinkController::~VideoSinkController() 
                {
                    printf("VideoSinkController service destroyed\n");
                }

                android::binder::Status VideoSinkController::setProperty(videosink::Property property, const PropertyValue& propertyValue, bool* _aidl_return)
                {
                    if (!propertyValue.value) {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(
                                android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }
                    *_aidl_return = true;
                    switch (property) {

                        case videosink::Property::AV_SOURCE: {
                            if (g_videosink_state[this->id.value] != State::READY) {
                                *_aidl_return = false;
                                return android::binder::Status::fromExceptionCode(
                                        android::binder::Status::EX_ILLEGAL_STATE);
                            }
                            int32_t intValue = propertyValue.value->get<PropertyValue::Value::Tag::intValue>();
                            std::cout << "[SERVICE::setProperty]::intValue : AV_SOURCE :"
                                      << intValue << "\n";
                            if (intValue < static_cast<int32_t>(AVSource::AUTO) ||
                                intValue > static_cast<int32_t>(AVSource::COMPOSITE_5)) {
                                *_aidl_return = false;
                                return android::binder::Status::fromExceptionCode(
                                        android::binder::Status::EX_ILLEGAL_ARGUMENT);
                            }
                            g_av_source[this->id.value] = static_cast<AVSource>(intValue);
                            break;
                        }

                        case videosink::Property::RENDER_FIRST_FRAME: {
                            int32_t intValue = propertyValue.value->get<PropertyValue::Value::Tag::intValue>();
                            std::cout << "[SERVICE::setProperty]::intValue : RENDER_FIRST_FRAME :" << intValue
                                      << "\n";
                            if (intValue < 0 || intValue > 1) {
                                *_aidl_return = false;
                                return android::binder::Status::fromExceptionCode(
                                        android::binder::Status::EX_ILLEGAL_ARGUMENT);
                            }
                            g_render_first_frame[this->id.value] = intValue;
                            break;
                        }
                        default:
                            *_aidl_return = false;
                            return android::binder::Status::fromExceptionCode(
                                    android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    return android::binder::Status::ok();
                }

                android::binder::Status VideoSinkController::setVideoDecoder(const ::com::rdk::hal::videodecoder::IVideoDecoder::Id& videoDecoderId, bool* _aidl_return)
                {
                    if(g_videosink_state[this->id.value] != State::READY)
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }

                    g_videodecoder_id[this->id.value] = videoDecoderId;
                    *_aidl_return = true;
                    return android::binder::Status::ok();
                }

                android::binder::Status VideoSinkController::getVideoDecoder(::com::rdk::hal::videodecoder::IVideoDecoder::Id* _aidl_return)
                {
                    if(g_videosink_state[this->id.value] != State::READY && g_videosink_state[this->id.value] != State::STARTED)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    *_aidl_return = g_videodecoder_id[this->id.value];
                    return android::binder::Status::ok();
                }

                android::binder::Status VideoSinkController::start()
                {
                    if(g_videosink_state[this->id.value] != State::READY)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from READY to STARTING...\n";
                            g_videosink_state[this->id.value] = State::STARTED;
                        }
                        //average wait time to simulate transitory state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        std::cout << "[SERVICE]::Calling onStateChanged for STARTING...\n";
                        if (_videoSinkEventListener) {
                            _videoSinkEventListener->onStateChanged(State::READY, State::STARTING);
                        } else {
                            std::cout << "[SERVICE]::Error: _videoSinkEventListener is null!\n";
                        }
                        state_cv.notify_all();
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from STARTING to STARTED...\n";
                            g_videosink_state[this->id.value] = State::STARTED;
                        }
                        //average wait time to simulate target state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        std::cout << "[SERVICE]::Calling onStateChanged for STARTED...\n";
                        _videoSinkEventListener->onStateChanged(State::STARTING, State::STARTED);
                        state_cv.notify_all();
                    }
                    return android::binder::Status::ok();
                }
    
                android::binder::Status VideoSinkController::stop()
                {
                    if(g_videosink_state[this->id.value] != State::STARTED)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }

                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from STARTED to STOPPING...\n";
                            g_videosink_state[this->id.value] = State::STOPPING;
                        }
                        //average wait time to simulate transitory state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        std::cout << "[SERVICE]::Calling onStateChanged for STOPPING...\n";
                        _videoSinkEventListener->onStateChanged(State::STARTED, State::STOPPING);
                        state_cv.notify_all();
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from STOPPING to READY...\n";
                            g_videosink_state[this->id.value] = State::READY;
                        }
                        //average wait time to simulate target state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        std::cout << "[SERVICE]::Calling onStateChanged for READY...\n";
                        _videoSinkEventListener->onStateChanged(State::STOPPING, State::READY);
                        state_cv.notify_all();
                    }
                    return android::binder::Status::ok();
                }
    
                android::binder::Status VideoSinkController::queueVideoFrame(int64_t nsPresentationTime, int64_t frameBufferHandle, const ::com::rdk::hal::videodecoder::FrameMetadata& metadata, bool* _aidl_return)
                {
                    if(g_videosink_state[this->id.value] != State::STARTED)
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    //TODO
                    *_aidl_return = true;
                    return android::binder::Status::ok();
                }

                android::binder::Status VideoSinkController::flush(bool holdLastFrame)
                {
                    if(g_videosink_state[this->id.value] != State::STARTED)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from STARTED to FLUSHING...\n";
                            g_videosink_state[this->id.value] = State::FLUSHING;
                        }
                        //average wait time to simulate transitory state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        std::cout << "[SERVICE]::Calling onStateChanged for FLUSHING...\n";
                        _videoSinkEventListener->onStateChanged(State::STARTED, State::FLUSHING);
                        state_cv.notify_all();
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from FLUSHING to STARTED...\n";
                            g_videosink_state[this->id.value] = State::STARTED;
                        }
                        //average wait time to simulate target state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        std::cout << "[SERVICE]::Calling onStateChanged for STARTED...\n";
                        _videoSinkEventListener->onStateChanged(State::FLUSHING, State::STARTED);
                        state_cv.notify_all();
                    }
                    if(holdLastFrame)
                    {
                        //TODO
                    }
                }
        
                android::binder::Status VideoSinkController::discardFramesUntil(int64_t nsPresentationTime)
                {
                    if(g_videosink_state[this->id.value] != State::STARTED)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }

                    if(nsPresentationTime < 0)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }
                    //TODO
                    return android::binder::Status::ok();
                }
            }
        }
    }
}
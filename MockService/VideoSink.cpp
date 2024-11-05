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

                VideoSinkManager::VideoSinkManager()
                {
                    printf("VideoSinkManager service initialized");
                }

                VideoSinkManager::~VideoSinkManager()
                {
                    printf("VideoSinkManager service destroyed");
                }

                android::binder::Status VideoSinkManager::getVideoSinkIds(  std::vector<IVideoSink::Id>* _aidl_return  )
                {
                    IVideoSink::Id id1;
                    id1.value = 0; // Set value
                    IVideoSink::Id id2;
                    id2.value = 1; // Set value
                    _aidl_return->push_back(id1);
                    _aidl_return->push_back(id2);
                    return android::binder::Status::ok();
                }

                android::binder::Status VideoSinkManager::getVideoSink(  const IVideoSink::Id& videoSinkId, android::sp<IVideoSink>* _aidl_return  )
                {
                    android::sp<IVideoSink> videoSink = new VideoSink();
                    *_aidl_return = videoSink;
                    return android::binder::Status::ok();
                }

                VideoSink::VideoSink() {
                    printf("VideoSink service initialized");
                }

                VideoSink::~VideoSink() {
                    printf("VideoSink service destroyed");
                }

                android::binder::Status VideoSink::getCapabilities(  videosink::Capabilities* _aidl_return  ) 
                {
                    return android::binder::Status::ok();
                }

                android::binder::Status VideoSink::getProperty(  videosink::Property property, std::optional<PropertyValue>* _aidl_return  ) 
                {
                    if (!_aidl_return)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
                    }
                    com::rdk::hal::PropertyValue value;
                    /* TODO : need to replace with actual values, currently values not avialble in AIDL files*/
                    switch (property) 
                    {
                        case videosink::Property::RESOURCE_ID:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(42);
                            break;
                        case videosink::Property::SINK_QUEUE_DEPTH:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(-31);
                            break;
                        case videosink::Property::AV_SOURCE:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(/* your AV source value here */ 0);
                            break;
                        case videosink::Property::METRIC_FRAMES_RECEIVED:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(0);
                            break;
                        case videosink::Property::METRIC_FRAMES_PRESENTED:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(/* metric value */ 100);
                            break;
                        case videosink::Property::METRIC_FRAMES_DROPPED_LATE:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(/* metric value */ 100);
                            break;
                        case videosink::Property::METRIC_FRAMES_DROPPED_FRC:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(/* metric value */ 100);
                            break;
                        case videosink::Property::METRIC_FRAMES_REPEATED_FRC:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(/* metric value */ 100);
                            break;
                        case videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(/* metric value */ 100);
                            break;                       
                        case videosink::Property::METRIC_UNDERFLOWED:
                            value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(/* metric value */ 100);
                            break;
                        default:
                            return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }
                    *_aidl_return = value;
                    return android::binder::Status::ok();
                }
                
                android::binder::Status VideoSink::setProperty(  videosink::Property property, const PropertyValue& propertyValue, bool* _aidl_return  ) 
                {
                    return android::binder::Status::ok();
                }
                
                android::binder::Status VideoSink::getState(  State* _aidl_return  ) 
                {
                    *_aidl_return = State::CLOSED;
                    return android::binder::Status::ok();
                }
                
                android::binder::Status VideoSink::open(  const android::sp<videosink::IVideoSinkControllerListener>& videoSinkControllerListener, android::sp<videosink::IVideoSinkController>* _aidl_return  ) 
                {
                    *_aidl_return = new VideoSinkController();
                    return android::binder::Status::ok();
                }
                
                android::binder::Status VideoSink::close(  const android::sp<videosink::IVideoSinkController>& videoSinkController, bool* _aidl_return  ) 
                {
                    return android::binder::Status::ok();
                }
                
                android::binder::Status VideoSink::registerEventListener(  const android::sp<videosink::IVideoSinkEventListener>& videoSinkEventListener, bool* _aidl_return  ) 
                {
                    return android::binder::Status::ok();
                }
                
                android::binder::Status VideoSink::unregisterEventListener(  const android::sp<videosink::IVideoSinkEventListener>& videoSinkEventListener, bool* _aidl_return  ) 
                {
                    return android::binder::Status::ok();
                }

                VideoSinkController::VideoSinkController() 
                {
                    printf("VideoSinkController service initialized");
                }

                VideoSinkController::~VideoSinkController() 
                {
                    printf("VideoSinkController service destroyed");
                }

                android::binder::Status VideoSinkController::setVideoDecoder(  const ::com::rdk::hal::videodecoder::IVideoDecoder::Id& videoDecoderId, bool* _aidl_return  )
                {
                    return android::binder::Status::ok();
                }

                android::binder::Status VideoSinkController::getVideoDecoder(  ::com::rdk::hal::videodecoder::IVideoDecoder::Id* _aidl_return  )
                {
                    return android::binder::Status::ok();
                }

                android::binder::Status VideoSinkController::start(    ) 
                {
                    return android::binder::Status::ok();
                }
    
                android::binder::Status VideoSinkController::stop(    ) 
                {
                    return android::binder::Status::ok();
                }
    
                android::binder::Status VideoSinkController::queueVideoFrame(  int64_t nsPresentationTime, int64_t frameBufferHandle, const ::com::rdk::hal::videodecoder::FrameMetadata& metadata, bool* _aidl_return  )
                {
                    return android::binder::Status::ok();
                }

                android::binder::Status VideoSinkController::flush(  bool holdLastFrame  ) 
                {
                    return android::binder::Status::ok();
                }
        
                android::binder::Status VideoSinkController::discardFramesUntil(  int64_t nsPresentationTime  ) 
                {
                    return android::binder::Status::ok();
                }
            }
        }
    }
}
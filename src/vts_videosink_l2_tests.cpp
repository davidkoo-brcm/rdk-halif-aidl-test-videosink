
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

#include <ut_kvp_profile.h>
#include <ut.h>
#include <ut_log.h>
#include <ut_control_plane.h>
#include <deque>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <binder/Status.h>
#include <utils/String16.h>
#include <vector>
#include <optional>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <unordered_set>

#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"
#include "vts_videosink.h"

#include "com/rdk/hal/videosink/IVideoSink.h"
#include "com/rdk/hal/videosink/IVideoSinkManager.h"
#include "com/rdk/hal/videosink/BnVideoSinkControllerListener.h"
#include "com/rdk/hal/videosink/BnVideoSinkEventListener.h"

using android::sp;
using android::String16;
using android::defaultServiceManager;
using android::binder::Status;

using namespace com::rdk::hal;
using namespace com::rdk::hal::videosink;
using namespace com::rdk::hal::videodecoder;

extern android::sp<IVideoSinkManager> gVideoSinkManager;

class VTS_L2_VideoSink : public UTCore
{
public:
    std::vector<std::tuple<sp<IVideoSink>, sp<VideoSinkControllerListener>, sp<VideoSinkEventListener>>> mVideoSinkList;
    VTS_L2_VideoSink() : UTCore() {}

    ~VTS_L2_VideoSink() override = default;

    void SetUp() override
    {
        UTCore::SetUp();
        std::vector<IVideoSink::Id> sinkIds;
        /* Get the Video Sink IDs */
        VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

        android::ProcessState::self()->startThreadPool();

        for(const auto& _sinkId: sinkIds)
        {
            sp<IVideoSink> videoSink;

            sp<VideoSinkEventListener> eventListener = new VideoSinkEventListener();

            sp<VideoSinkControllerListener> controlListener = new VideoSinkControllerListener();

            /* Get Video Sink handle */
            VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

            /* Register events */
            VIDEOSINK_REGISTER_EVENT_SUCCESS(videoSink, eventListener);

            mVideoSinkList.push_back({videoSink, controlListener, eventListener});
        }
    }
    void checkCompleteStateTransition(sp<IVideoSink> sink, sp<VideoSinkEventListener> eventListener, State oldState, State intermediateState, State newState)
    {
        ::com::rdk::hal::State currentState;

        UT_ASSERT_TRUE(eventListener->checkStateTransistion(oldState, intermediateState));

        eventListener->waitForState(newState, std::chrono::milliseconds(MAX_FINAL_STATE_WAIT_MS));

        VIDEOSINK_GET_STATE(sink, currentState);

        UT_ASSERT_EQUAL(currentState, newState);

        UT_ASSERT_TRUE(eventListener->checkStateTransistion(intermediateState, newState));

        eventListener->resetStateChanges();
    }

    void TearDown() override
    {
        for (const auto& [sink, controlListener, eventListener] : mVideoSinkList)
        {
            /* Un-register the events */
            VIDEOSINK_UNREGISTER_EVENT_SUCCESS(sink, eventListener);
        }
        mVideoSinkList.clear();
    }
};

struct PropertyTestCase {
    ::com::rdk::hal::videosink::Property property;
    int32_t testValue;
    const char* propertyName;
};
std::vector<PropertyTestCase> testCases = {
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::AUTO), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::IP), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::TUNER), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::SYSTEM), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::HDMI_1), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::HDMI_2), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::HDMI_3), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::HDMI_4), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::HDMI_5), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::COMPOSITE_1), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::COMPOSITE_2), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::COMPOSITE_3), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::COMPOSITE_4), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::COMPOSITE_5), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::RENDER_FIRST_FRAME, 1, "RENDER_FIRST_FRAME"},
    {::com::rdk::hal::videosink::Property::RENDER_FIRST_FRAME, 0, "RENDER_FIRST_FRAME"},
};

/**
 * @brief Test the setting and getting of properties in VideoSink
 *
 * This test verifies that properties can be set and retrieved correctly in the VideoSink module. It ensures that the set and get operations are successful and that the retrieved property values match the set values.
 *
 * **Test Group ID:** 02@n
 * **Test Case ID:** 001@n
 *
 * **Test Procedure:**
 * Refer to UT specification documentation [viedoSink_High-Level_TestSpecification.md](../../docs/pages/viedoSink_High-Level_TestSpecification.md)
 */
UT_ADD_TEST(VTS_L2_VideoSink, TestSetAndGetProperty) {

    for(const auto& [videoSink, controlListener, eventListener] : mVideoSinkList)
    {

        sp<IVideoSinkController> videoSinkController;

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controlListener, videoSinkController, true);

        for (const auto& testCase : testCases) {
            PropertyValue setValue;
            setValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(testCase.testValue);

            VIDEOSINK_SETPROPERTY_SUCCESS(videoSinkController,testCase.property,setValue);

            std::optional<PropertyValue> getPropertyValue;
            VIDEOSINK_GET_PROPERTY_SUCCESS(videoSink, testCase.property, getPropertyValue);
            UT_ASSERT_EQUAL(getPropertyValue->value->get<com::rdk::hal::PropertyValue::Value::Tag::intValue>(), testCase.testValue);
        }

        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }
}

/**
 * @brief Test the setting and getting of properties in  multiple instance of VideoSink
 *
 * This test verifies that properties can be set and retrieved correctly in the VideoSink module. It ensures that the set and get operations are successful and that the retrieved property values match the set values.
 *
 * **Test Group ID:** 02@n
 * **Test Case ID:** 001@n
 *
 * **Test Procedure:**
 * Refer to UT specification documentation [viedoSink_High-Level_TestSpecification.md](../../docs/pages/viedoSink_High-Level_TestSpecification.md)
 */
UT_ADD_TEST(VTS_L2_VideoSink, TestSetAndGetProperty_multipleInstance) {
    std::vector<sp<IVideoSinkController>> videoSinkControllersList;
    for(const auto& [videoSink, controlListener, eventListener] : mVideoSinkList)
    {

        sp<IVideoSinkController> videoSinkController;

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controlListener, videoSinkController, true);
        videoSinkControllersList.push_back(videoSinkController);
    }
    for(size_t i = 0; i< mVideoSinkList.size();++i)
    {
        const auto& [videoSink, controlListener, eventListener] = mVideoSinkList[i];
        const auto& videoSinkController = videoSinkControllersList[i];

        for (const auto& testCase : testCases) {
            PropertyValue setValue;
            setValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(testCase.testValue);

            VIDEOSINK_SETPROPERTY_SUCCESS(videoSinkController,testCase.property,setValue);

            std::optional<PropertyValue> getPropertyValue;
            VIDEOSINK_GET_PROPERTY_SUCCESS(videoSink, testCase.property, getPropertyValue);
            UT_ASSERT_EQUAL(getPropertyValue->value->get<com::rdk::hal::PropertyValue::Value::Tag::intValue>(), testCase.testValue);
        }

        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }
}

/**
* @brief Test the state transitions of the VideoSink component
*
* This test verifies the correct state transitions of the VideoSink component, ensuring that it moves through the expected states: READY, STARTED, FLUSHING, and CLOSED. It checks the integrity and correctness of each transition and the final state.
*
* **Test Group ID:** 02@n
* **Test Case ID:** 002@n
*
* **Test Procedure:**
* Refer to UT specification documentation [viedoSink_High-Level_TestSpecification.md](../../docs/pages/viedoSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L2_VideoSink, TestStateTransitions) {

    for(const auto& [videoSink, controlListener, eventListener] : mVideoSinkList)
    {
        /* Get Video sink handle */

        sp<IVideoSinkController> videoSinkController;

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controlListener, videoSinkController, false);
        checkCompleteStateTransition(videoSink,eventListener,State::CLOSED,State::OPENING,State::READY);

        // Transition to STARTED state
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);
        checkCompleteStateTransition(videoSink,eventListener,State::READY,State::STARTING,State::STARTED);

		// Transition to FLUSHING state
        VIDEOSINK_FLUSH_SUCCESS(videoSink, videoSinkController, false);
        checkCompleteStateTransition(videoSink,eventListener,State::STARTED,State::FLUSHING,State::STARTED);

		// Transition to READY state
        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);
        checkCompleteStateTransition(videoSink,eventListener,State::STARTED,State::STOPPING,State::READY);

		// Transition to CLOSED state
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
        checkCompleteStateTransition(videoSink,eventListener,State::READY,State::CLOSING,State::CLOSED);
    }
}

/**
* @brief Test the state transitions of the VideoSink component
*
* This test verifies the correct state transitions of the VideoSink component, ensuring that it moves through the expected states: READY, STARTED, FLUSHING, and CLOSED. It checks the integrity and correctness of each transition and the final state.
*
* **Test Group ID:** 02@n
* **Test Case ID:** 002@n
*
* **Test Procedure:**
* Refer to UT specification documentation [viedoSink_High-Level_TestSpecification.md](../../docs/pages/viedoSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L2_VideoSink, TestStateTransitions_multipleInstance) {
    std::vector<sp<IVideoSinkController>> videoSinkControllersList;
    for(const auto& [videoSink, controlListener, eventListener] : mVideoSinkList)
    {

        sp<IVideoSinkController> videoSinkController;

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controlListener, videoSinkController, false);
        checkCompleteStateTransition(videoSink,eventListener,State::CLOSED,State::OPENING,State::READY);
        videoSinkControllersList.push_back(videoSinkController);
    }

    for(size_t i = 0; i< mVideoSinkList.size();++i)
    {
        const auto& [videoSink, controlListener, eventListener] = mVideoSinkList[i];
        const auto& videoSinkController = videoSinkControllersList[i];
        /* Get Video sink handle */

        // Transition to STARTED state
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);
        checkCompleteStateTransition(videoSink,eventListener,State::READY,State::STARTING,State::STARTED);

		// Transition to FLUSHING state
        VIDEOSINK_FLUSH_SUCCESS(videoSink, videoSinkController, false);
        checkCompleteStateTransition(videoSink,eventListener,State::STARTED,State::FLUSHING,State::STARTED);

		// Transition to READY state
        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);
        checkCompleteStateTransition(videoSink,eventListener,State::STARTED,State::STOPPING,State::READY);

		// Transition to CLOSED state
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
        checkCompleteStateTransition(videoSink,eventListener,State::READY,State::CLOSING,State::CLOSED);
    }
}


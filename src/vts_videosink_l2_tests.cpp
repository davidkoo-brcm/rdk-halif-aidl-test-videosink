
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

static std::vector<IVideoSink::Id> mSinkIds;
static std::vector<sp<IVideoSink>> mVideoSinks;
static std::vector<sp<IVideoSinkController>> mControllers;
static android::sp<IVideoSinkManager> gVideoSinkManager = nullptr;

static constexpr int MAX_TRANSITORY_STATE_WAIT_MS = 10;
static constexpr int MAX_FINAL_STATE_WAIT_MS = 500;

/* Status::Exception */
const static ut_control_keyStringMapping_t statusException_mapTable[] =
{
    {"EX_NONE",                  (int32_t) Status::EX_NONE},
    {"EX_SECURITY",              (int32_t) Status::EX_SECURITY},
    {"EX_BAD_PARCELABLE",        (int32_t) Status::EX_BAD_PARCELABLE},
    {"EX_ILLEGAL_ARGUMENT",      (int32_t) Status::EX_ILLEGAL_ARGUMENT},
    {"EX_NULL_POINTER",          (int32_t) Status::EX_NULL_POINTER},
    {"EX_ILLEGAL_STATE",         (int32_t) Status::EX_ILLEGAL_STATE},
    {"EX_NETWORK_MAIN_THREAD",   (int32_t) Status::EX_NETWORK_MAIN_THREAD},
    {"EX_UNSUPPORTED_OPERATION", (int32_t) Status::EX_UNSUPPORTED_OPERATION},
    {"EX_SERVICE_SPECIFIC",      (int32_t) Status::EX_SERVICE_SPECIFIC},
    {"EX_PARCELABLE",            (int32_t) Status::EX_PARCELABLE},
    {"EX_HAS_REPLY_HEADER",      (int32_t) Status::EX_HAS_REPLY_HEADER},
    {"EX_TRANSACTION_FAILED",    (int32_t) Status::EX_TRANSACTION_FAILED},
    {NULL, -1}
};

/* bool */
const static ut_control_keyStringMapping_t bool_mapTable[] =
{
    {"false", (int32_t) false},
    {"true",  (int32_t) true},
    {NULL, -1}
};

int getService()
{
    gVideoSinkManager =
        android::interface_cast<::com::rdk::hal::videosink::IVideoSinkManager>(
            android::defaultServiceManager()->getService(
                android::String16(::com::rdk::hal::videosink::IVideoSinkManager::serviceName().c_str())
            )
        );
    if (gVideoSinkManager == nullptr)
    {
        printf("Service not available\n");
        return -1;
    }
    return 0;
};

class VideoSinkEventListenerImpl : public BnVideoSinkEventListener {
public:

    VideoSinkEventListenerImpl() {};
    virtual ~VideoSinkEventListenerImpl() = default;

    std::mutex mtx;
    std::condition_variable cv;
    std::vector<std::pair<State, State>> stateChanges;

    Status onStateChanged(State oldState, State newState) override {
        std::lock_guard<std::mutex> lock(mtx);
        stateChanges.emplace_back(oldState, newState);
        cv.notify_all();
        return Status::ok();
    }

    Status onFirstFrameRendered(int64_t) override { return Status::ok(); }
    Status onEndOfStream(int64_t) override { return Status::ok(); }
    Status onVideoUnderflow() override { return Status::ok(); }
    Status onVideoResumed() override { return Status::ok(); }
    Status onFlushComplete() override { return Status::ok(); }

    bool waitForState(State oldState, State newState, int timeoutMs = 500) {
        std::unique_lock<std::mutex> lock(mtx);
        return cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] {
            for (const auto& p : stateChanges) {
                if (p.first == oldState && p.second == newState) return true;
            }
            return false;
        });
    }
};

class VideoSinkControllerListenerImpl : public BnVideoSinkControllerListener {
public:
    VideoSinkControllerListenerImpl() {};
    virtual ~VideoSinkControllerListenerImpl() = default;

    Status onStateChanged(State oldState, State newState) override {return Status::ok();}
    Status onFirstFrameRendered(int64_t) override { return Status::ok(); }
    Status onEndOfStream(int64_t) override { return Status::ok(); }
    Status onVideoUnderflow() override { return Status::ok(); }
    Status onVideoResumed() override { return Status::ok(); }
    Status onFlushComplete() override { return Status::ok(); }
};

class VideoSinkManagerBaseTest : public virtual UTCore {
protected:
    void SetUp() override {
        UTCore::SetUp();

        UT_ASSERT_EQUAL(getService(), 0);
    }

    void TearDown() override {
        UTCore::TearDown();

        gVideoSinkManager.clear();
    }
};

std::vector<sp<VideoSinkEventListenerImpl>> mSinkEventListener;

class VideoSinkBaseTest : public VideoSinkManagerBaseTest {
    public:
        void SetUp() override {

            VideoSinkManagerBaseTest::SetUp();
            if (mSinkIds.empty()) {
                std::vector<IVideoSink::Id> ids;
                UT_LOG_INFO("Calling getVideoSinkIds(OUT:ids:[])");
                Status status = gVideoSinkManager->getVideoSinkIds(&ids);
                UT_LOG_INFO("Result getVideoSinkIds(OUT:ids:[size:%d]), status:[isOK:%s, exception:%s]",
                            ids.size(),
                            UT_Control_GetMapString(bool_mapTable, status.isOk()),
                            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
                UT_ASSERT_TRUE(status.isOk());
                mSinkIds = ids;
                UT_ASSERT_FALSE(mSinkIds.empty());
            }

            if (mVideoSinks.empty()) {
                for (auto& sinkId : mSinkIds) {
                    sp<VideoSinkEventListenerImpl> listener = new VideoSinkEventListenerImpl();
                    sp<IVideoSink> sink;
                    UT_LOG_INFO("Calling getVideoSink(IN:sinkId:[%d], OUT:sink[])", sinkId.value);
                    Status status = gVideoSinkManager->getVideoSink(sinkId, &sink);
                    UT_LOG_INFO("Result getVideoSink(IN:sinkId:[%d], OUT:sink:[0x%X]), status:[isOK:%s, exception:%s]",
                                sinkId.value, sink.get(),
                                UT_Control_GetMapString(bool_mapTable, status.isOk()),
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
                    UT_ASSERT_TRUE(status.isOk());
                    UT_ASSERT_NOT_EQUAL(sink, nullptr);
                    bool registered = false;
                    UT_LOG_INFO("Calling registerEventListener(IN:listener:[0x%X], OUT:registered[])", listener.get());
                    Status regStatus = sink->registerEventListener(listener, &registered);
                    UT_LOG_INFO("Result registerEventListener(IN:listener:[0x%X], OUT:registered:[%d]), status:[isOK:%s, exception:%s]",
                                listener.get(), registered,
                                UT_Control_GetMapString(bool_mapTable, regStatus.isOk()),
                                UT_Control_GetMapString(statusException_mapTable, regStatus.exceptionCode()));
                    UT_ASSERT_TRUE(regStatus.isOk());
                    UT_ASSERT_TRUE(registered);
                    mVideoSinks.push_back(sink);
                    mSinkEventListener.push_back(listener);
                }
            }
        }

        void TearDown() override {

            for (size_t i = 0; i < mVideoSinks.size(); ++i) {
                bool unregistered = false;
                UT_LOG_INFO("Calling unregisterEventListener(IN:listener:[0x%X], OUT:unregistered[])", mSinkEventListener[i].get());
                Status status = mVideoSinks[i]->unregisterEventListener(mSinkEventListener[i], &unregistered);
                UT_LOG_INFO("Result unregisterEventListener(IN:listener:[0x%X], OUT:unregistered:[%d]), status:[isOK:%s, exception:%s]",
                            mSinkEventListener[i].get(), unregistered,
                            UT_Control_GetMapString(bool_mapTable, status.isOk()),
                            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
                UT_ASSERT_TRUE(unregistered);
            }
            mVideoSinks.clear();
            mSinkIds.clear();
            mSinkEventListener.clear();
            VideoSinkManagerBaseTest::TearDown();
        }
    };

std::vector<sp<VideoSinkControllerListenerImpl>> mControllerListener;

class VideoSinkControllerBaseTest : public VideoSinkBaseTest {
    protected:
        void SetUp() override {

            VideoSinkBaseTest::SetUp();
            if (mControllers.empty() && !mVideoSinks.empty()) {
                for (auto& sink : mVideoSinks) {
                    sp<IVideoSinkController> controller;
                    sp<VideoSinkControllerListenerImpl> controllerListener = new VideoSinkControllerListenerImpl();

                    UT_LOG_INFO("Calling open(IN:controllerListener:[0x%X], OUT:controller[])", controllerListener.get());
                    Status status = sink->open(controllerListener, &controller);
                    UT_LOG_INFO("Result open(IN:controllerListener:[0x%X], OUT:controller:[0x%X]), status:[isOK:%s, exception:%s]",
                        controllerListener.get(), controller.get(),
                                UT_Control_GetMapString(bool_mapTable, status.isOk()),
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
                    UT_ASSERT_TRUE(status.isOk());
                    UT_ASSERT_NOT_EQUAL(controller, nullptr);
                    mControllers.push_back(controller);
                    mControllerListener.push_back(controllerListener);
                }
            }
        }

        void TearDown() override {

            UT_ASSERT_EQUAL(mControllers.size(), mVideoSinks.size());
            for (size_t i = 0; i < mVideoSinks.size(); ++i) {
                bool closed = false;
                UT_LOG_INFO("Calling close(IN:controller:[0x%X], OUT:closed[])", mControllers[i].get());
                Status status = mVideoSinks[i]->close(mControllers[i], &closed);
                UT_LOG_INFO("Result close(IN:controller:[0x%X], OUT:closed:[%d]), status:[isOK:%s, exception:%s]",
                            mControllers[i].get(), closed,
                            UT_Control_GetMapString(bool_mapTable, status.isOk()),
                            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
                EXPECT_TRUE(status.isOk());
                EXPECT_TRUE(closed);
            }
            mControllers.clear();
            mControllerListener.clear();
            VideoSinkBaseTest::TearDown();
        }
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
UT_ADD_TEST(VideoSinkControllerBaseTest, TestSetAndGetProperty) {
    for (size_t i = 0; i < mVideoSinks.size(); ++i) {
        sp<IVideoSink> sink = mVideoSinks[i];

        struct PropertyTestCase {
            ::com::rdk::hal::videosink::Property property;
            int32_t testValue;
            const char* propertyName;
        };

        std::vector<PropertyTestCase> testCases = {
            {::com::rdk::hal::videosink::Property::AV_SOURCE, 2, "AV_SOURCE"},
            {::com::rdk::hal::videosink::Property::RENDER_FIRST_FRAME, 1, "RENDER_FIRST_FRAME"},
        };

        for (const auto& testCase : testCases) {
            PropertyValue setValue;
            setValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(testCase.testValue);

            bool setResult = false;
            UT_LOG_INFO("Calling setProperty(IN:property:[%s], IN:setValue:[%d])",testCase.propertyName, testCase.testValue);
            Status setStatus = mControllers[i]->setProperty(testCase.property, setValue, &setResult);
            UT_LOG_INFO("Result setProperty(IN:property:[%s], IN:setValue:[%d]), status:[isOK:%s, exception:%s]",
                        testCase.propertyName, testCase.testValue,
                        UT_Control_GetMapString(bool_mapTable, setStatus.isOk()),
                        UT_Control_GetMapString(statusException_mapTable, setStatus.exceptionCode()));
            UT_ASSERT_TRUE(setStatus.isOk());
            UT_ASSERT_TRUE(setResult);

            std::optional<PropertyValue> getPropertyValue;
            UT_LOG_INFO("Calling getProperty(IN:property:[%s], OUT:getPropertyValue[])", testCase.propertyName);
            Status getStatus = sink->getProperty(testCase.property, &getPropertyValue);
            UT_LOG_INFO("Result getProperty(IN:property:[%s], OUT:getPropertyValue[%d]), status:[isOK:%s, exception:%s]",
                        testCase.propertyName,
                        getPropertyValue->value->get<com::rdk::hal::PropertyValue::Value::Tag::intValue>(),
                        UT_Control_GetMapString(bool_mapTable, getStatus.isOk()),
                        UT_Control_GetMapString(statusException_mapTable, getStatus.exceptionCode()));
            UT_ASSERT_TRUE(getStatus.isOk());
            UT_ASSERT_TRUE(getPropertyValue.has_value());
            UT_ASSERT_TRUE(getPropertyValue->value.has_value());
            UT_ASSERT_EQUAL(getPropertyValue->value->get<com::rdk::hal::PropertyValue::Value::Tag::intValue>(), testCase.testValue);
        }
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
UT_ADD_TEST(VideoSinkBaseTest, TestStateTransitions) {
    for (size_t i = 0; i < mVideoSinks.size(); ++i) {
        sp<VideoSinkEventListenerImpl> listener = mSinkEventListener[i];
        sp<VideoSinkControllerListenerImpl> controllerListener = new VideoSinkControllerListenerImpl();
        sp<IVideoSinkController> controller;

		// Transition to READY state
        UT_LOG_INFO("Calling open(IN:controllerListener:[0x%X], OUT:controller[])", controllerListener.get());
        Status openStatus = mVideoSinks[i]->open(controllerListener, &controller);
        UT_LOG_INFO("Result open(IN:controllerListener:[0x%X], OUT:controller:[0x%X]), status:[isOK:%s, exception:%s]",
                    controllerListener.get(), controller.get(),
                    UT_Control_GetMapString(bool_mapTable, openStatus.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, openStatus.exceptionCode()));
        UT_ASSERT_TRUE(openStatus.isOk());
        UT_ASSERT_NOT_EQUAL(controller, nullptr);

		// Verify state transition to OPENING
        UT_ASSERT_TRUE(listener->waitForState(State::CLOSED, State::OPENING, MAX_TRANSITORY_STATE_WAIT_MS));

		// Verify state transition to READY
        UT_ASSERT_TRUE(listener->waitForState(State::OPENING, State::READY, MAX_FINAL_STATE_WAIT_MS));

		// Verify READY state
        State currentState;
        UT_LOG_INFO("Calling getState(OUT:currentState[])");
        Status getStateStatus = mVideoSinks[i]->getState(&currentState);
        UT_LOG_INFO("Result getState(OUT:currentState:[%s]), status:[isOK:%s, exception:%s]",
                    currentState,
                    UT_Control_GetMapString(bool_mapTable, getStateStatus.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, getStateStatus.exceptionCode()));
        UT_ASSERT_TRUE(getStateStatus.isOk());
        UT_ASSERT_EQUAL(currentState, State::READY);

		// Transition to STARTED state
        UT_LOG_INFO("Calling start()");
        Status startStatus = controller->start();
        UT_LOG_INFO("Result start(), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, startStatus.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, startStatus.exceptionCode()));
        UT_ASSERT_TRUE(startStatus.isOk());


        // Verify state transition to STARTING
        UT_ASSERT_TRUE(listener->waitForState(State::READY, State::STARTING, MAX_TRANSITORY_STATE_WAIT_MS));

		// Verify state transition to STARTED
		UT_ASSERT_TRUE(listener->waitForState(State::STARTING, State::STARTED, MAX_FINAL_STATE_WAIT_MS));

		// Verify STARTED state
        UT_LOG_INFO("Calling getState(OUT:currentState[])");
        getStateStatus = mVideoSinks[i]->getState(&currentState);
        UT_LOG_INFO("Result getState(OUT:currentState:[%s]), status:[isOK:%s, exception:%s]",
                    currentState,
                    UT_Control_GetMapString(bool_mapTable, getStateStatus.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, getStateStatus.exceptionCode()));
        UT_ASSERT_TRUE(getStateStatus.isOk());
        UT_ASSERT_EQUAL(currentState, State::STARTED);

		// Transition to FLUSHING state
        UT_LOG_INFO("Calling flush(IN:holdLastFrame:[false])");
        Status flushStatus = controller->flush(false);
        UT_LOG_INFO("Result flush(IN:holdLastFrame:[false]), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, flushStatus.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, flushStatus.exceptionCode()));
        UT_ASSERT_TRUE(flushStatus.isOk());

		// Verify state transition to FLUSHING
        UT_ASSERT_TRUE(listener->waitForState(State::STARTED, State::FLUSHING, MAX_TRANSITORY_STATE_WAIT_MS));

		// Verify state transition to STARTED
        UT_ASSERT_TRUE(listener->waitForState(State::FLUSHING, State::STARTED, MAX_FINAL_STATE_WAIT_MS));

		// Verify STARTED state
        UT_LOG_INFO("Calling getState(OUT:currentState[])");
        getStateStatus = mVideoSinks[i]->getState(&currentState);
        UT_LOG_INFO("Result getState(OUT:currentState:[%s]), status:[isOK:%s, exception:%s]",
                    currentState,
                    UT_Control_GetMapString(bool_mapTable, getStateStatus.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, getStateStatus.exceptionCode()));
        UT_ASSERT_TRUE(getStateStatus.isOk());
        UT_ASSERT_EQUAL(currentState, State::STARTED);

		// Transition to READY state
        UT_LOG_INFO("Calling stop()");
        startStatus = controller->stop();
        UT_LOG_INFO("Result stop(), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, startStatus.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, startStatus.exceptionCode()));
        UT_ASSERT_TRUE(startStatus.isOk());

		// Verify state transition to STOPPING
        UT_ASSERT_TRUE(listener->waitForState(State::STARTED, State::STOPPING, MAX_TRANSITORY_STATE_WAIT_MS));

		// Verify state transition to READY
        UT_ASSERT_TRUE(listener->waitForState(State::STOPPING, State::READY, MAX_FINAL_STATE_WAIT_MS));

        UT_LOG_INFO("Calling getState(OUT:currentState[])");
        Status statestatus = mVideoSinks[i]->getState(&currentState);
        UT_LOG_INFO("Result getState(OUT:currentState:[%s]), status:[isOK:%s, exception:%s]",
                    currentState,
                    UT_Control_GetMapString(bool_mapTable, statestatus.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, statestatus.exceptionCode()));
        UT_ASSERT_TRUE(statestatus.isOk());
        UT_ASSERT_EQUAL(currentState, State::READY);

		// Transition to CLOSED state
        UT_LOG_INFO("Calling close(IN:controller:[0x%X], OUT:closed[])", controller.get());
        bool closed = false;
        Status closeStatus = mVideoSinks[i]->close(controller, &closed);
        UT_LOG_INFO("Result close(IN:controller:[0x%X], OUT:closed:[%d]), status:[isOK:%s, exception:%s]",
                    controller.get(), closed,
                    UT_Control_GetMapString(bool_mapTable, closeStatus.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, closeStatus.exceptionCode()));
        UT_ASSERT_TRUE(closeStatus.isOk());
        UT_ASSERT_TRUE(closed);

		// Verify state transition to CLOSING
        UT_ASSERT_TRUE(listener->waitForState(State::READY, State::CLOSING, MAX_TRANSITORY_STATE_WAIT_MS));

		// Verify state transition to CLOSED
		UT_ASSERT_TRUE(listener->waitForState(State::CLOSING, State::CLOSED, MAX_FINAL_STATE_WAIT_MS));

		// Verify CLOSED state
        UT_LOG_INFO("Calling getState(OUT:currentState[])");
        getStateStatus = mVideoSinks[i]->getState(&currentState);
        UT_LOG_INFO("Result getState(OUT:currentState:[%s]), status:[isOK:%s, exception:%s]",
                    currentState,
                    UT_Control_GetMapString(bool_mapTable, getStateStatus.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, getStateStatus.exceptionCode()));
        UT_ASSERT_TRUE(getStateStatus.isOk());
        UT_ASSERT_EQUAL(currentState, State::CLOSED);
    }
}
#if 0
int main(int argc, char **argv)
{
    android::ProcessState::self()->startThreadPool();
    UT_status_t result;
    UT_init(argc, argv);
    result = UT_run_tests();
    return result;
}
#endif



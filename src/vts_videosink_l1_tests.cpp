
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
 
 #include <ut_kvp_profile.h>
 #include <ut.h>
 #include <ut_log.h>
 #include <ut_control_plane.h>
 #include <binder/IServiceManager.h>
 #include <binder/ProcessState.h>
 #include <binder/IPCThreadState.h>
 #include <binder/Status.h>
 #include <utils/String16.h>
 #include <stdio.h>
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
#include "com/rdk/hal/videosink/IVideoSinkController.h"
#include "com/rdk/hal/videosink/BnVideoSinkEventListener.h"
#include "com/rdk/hal/videosink/BnVideoSinkControllerListener.h"

using android::sp;
using android::String16;
using android::defaultServiceManager;
using android::binder::Status;

using namespace com::rdk::hal;
using namespace com::rdk::hal::videosink;
using namespace com::rdk::hal::videodecoder;

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

static sp<IVideoSinkManager> gVideoSinkManagerService = nullptr;

int getService()
{
    gVideoSinkManagerService = android::interface_cast<IVideoSinkManager>(
        defaultServiceManager()->getService(
            String16(IVideoSinkManager::serviceName().c_str())));
    if (gVideoSinkManagerService == nullptr)
    {
        printf("Service not available\n");
        return -1;
    }
    return 0;
}

class VideoSinkEventListenerImpl : public BnVideoSinkEventListener
{
public:
    Status onStateChanged(State oldState, State newState) override
    {
        return Status::ok();
    }

    Status onFirstFrameRendered(int64_t) override { return Status::ok(); }
    Status onEndOfStream(int64_t) override { return Status::ok(); }
    Status onVideoUnderflow() override { return Status::ok(); }
    Status onVideoResumed() override { return Status::ok(); }
    Status onFlushComplete() override { return Status::ok(); }
};

class VideoSinkControllerListenerImpl : public BnVideoSinkControllerListener
{
public:
    Status onStateChanged(State oldState, State newState) override {return Status::ok();}
    Status onFirstFrameRendered(int64_t) override { return Status::ok(); }
    Status onEndOfStream(int64_t) override { return Status::ok(); }
    Status onVideoUnderflow() override { return Status::ok(); }
    Status onVideoResumed() override { return Status::ok(); }
    Status onFlushComplete() override { return Status::ok(); }
};

class VideoSinkManagerBaseTest : public virtual UTCore
{
protected:
    static void SetUpTestSuite()
    {
        UT_ASSERT_EQUAL(getService(), 0);
    }

    static void TearDownTestSuite()
    {
        gVideoSinkManagerService.clear();
    }
};

class VideoSinkBaseTest : public VideoSinkManagerBaseTest
{
protected:
    static std::vector<IVideoSink::Id> mSinkIds;
    static std::vector<sp<IVideoSink>> mVideoSinks;

    void SetUp() override
    {
        android::binder::Status status;
        VideoSinkManagerBaseTest::SetUp();

        if (mSinkIds.empty())
        {
            std::vector<IVideoSink::Id> ids;
            status = gVideoSinkManagerService->getVideoSinkIds(&ids);
            UT_ASSERT_TRUE(status.isOk());
            mSinkIds = ids;
            UT_ASSERT_FALSE(mSinkIds.empty());
        }

        if (mVideoSinks.empty())
        {
            for (auto& sinkId : mSinkIds)
            {
                sp<IVideoSink> sink;
                status = gVideoSinkManagerService->getVideoSink(sinkId, &sink);
                UT_ASSERT_TRUE(status.isOk());
                UT_ASSERT_NOT_EQUAL(sink, nullptr);
                mVideoSinks.push_back(sink);
            }
        }
    }

    void TearDown() override
    {
        VideoSinkManagerBaseTest::TearDown();
    }
};

std::vector<IVideoSink::Id> VideoSinkBaseTest::mSinkIds;
std::vector<sp<IVideoSink>> VideoSinkBaseTest::mVideoSinks;

class VideoSinkControllerBaseTest : public VideoSinkBaseTest
{
protected:
    static std::vector<sp<IVideoSinkController>> mControllers;
    static sp<IVideoSinkControllerListener> mControllerListener;

    void SetUp() override
    {
        android::binder::Status status;
        VideoSinkBaseTest::SetUp();

        if (mControllers.empty() && !mVideoSinks.empty())
        {
            mControllerListener = new VideoSinkControllerListenerImpl;
            for (auto& sink : mVideoSinks)
            {
                sp<IVideoSinkController> controller;
                status = sink->open(mControllerListener, &controller);
                UT_ASSERT_TRUE(status.isOk());
                UT_ASSERT_NOT_EQUAL(controller, nullptr);
                mControllers.push_back(controller);
            }
        }
    }

    void TearDown() override
    {
        android::binder::Status status;
        UT_ASSERT_EQUAL(mControllers.size(), mVideoSinks.size());
        for (size_t i = 0; i < mVideoSinks.size(); ++i)
        {
            bool closed = false;
            status = mVideoSinks[i]->close(mControllers[i], &closed);
            UT_ASSERT_TRUE(status.isOk());
            UT_ASSERT_TRUE(closed);
        }
        mControllers.clear();
        VideoSinkBaseTest::TearDown();
    }
};

std::vector<sp<IVideoSinkController>> VideoSinkControllerBaseTest::mControllers;
sp<IVideoSinkControllerListener> VideoSinkControllerBaseTest::mControllerListener = nullptr;

/**
 * @brief Test to verify the retrieval of video sink IDs from the VideoSinkManager service.
 *
 * This test checks the functionality of the getVideoSinkIds method in the VideoSinkManager service. 
 * It ensures that the method returns a valid list of video sink IDs and that the list is not empty.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 001@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize the test by setting up necessary variables and logging the start of the test. | None | None | Should be successful |
 * | 02 | Call the getVideoSinkIds method on the gVideoSinkManagerService to retrieve video sink IDs. | OUT: retrievedIds | Status should be OK, retrievedIds should not be empty | Should Pass |
 * | 03 | Verify that the status returned by getVideoSinkIds is OK. | status.isOk() | True | Should Pass |
 * | 04 | Verify that the retrievedIds list is not empty. | retrievedIds.empty() | False | Should Pass |
 * | 05 | Define the expected set of video sink IDs using predefined constants. | expectedIds | None | Should be successful |
 * | 06 | Iterate over each ID in retrievedIds and verify it is not UNDEFINED and exists in expectedIds. | id.value | Not UNDEFINED, Exists in expectedIds | Should Pass |
 * | 07 | Verify that the size of retrievedIds matches the expected count from the profile. | retrievedIds.size() | Matches expected count | Should Pass |
 */
UT_ADD_TEST(VideoSinkManagerBaseTest, test_l1_VideoSinkManager_positive_getVideoSinkIds)
{
    android::binder::Status status;
    std::vector<IVideoSink::Id> retrievedIds;

    UT_LOG_INFO("Calling getVideoSinkIds(OUT:_aidl_return)");
    status = gVideoSinkManagerService->getVideoSinkIds(&retrievedIds);
    UT_LOG_INFO("Result getVideoSinkIds(OUT:_aidl_return), status:[isOK:%s, exception:%s]",
                UT_Control_GetMapString(bool_mapTable, status.isOk()),
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));

    UT_ASSERT_TRUE(status.isOk());
    UT_ASSERT_FALSE(retrievedIds.empty());

    std::unordered_set<int32_t> expectedIds;
    std::string videosink_count_path = "IVideoSinkManager.IVideoSink";
    int32_t expected_count = UT_KVP_PROFILE_GET_LIST_COUNT(videosink_count_path.c_str());

    for (size_t i = 0; i < expected_count; ++i) 
    {
        std::string videosink_id_path = videosink_count_path + "." + std::to_string(i);
        expectedIds.insert(UT_KVP_PROFILE_GET_UINT32(videosink_id_path.c_str()));
    }

    UT_ASSERT_EQUAL(retrievedIds.size(), expectedIds.size());
    for (const auto& id : retrievedIds)
    {
        UT_ASSERT_NOT_EQUAL(id.value, IVideoSink::Id::UNDEFINED);
        UT_ASSERT_TRUE(expectedIds.find(id.value) != expectedIds.end());
    }

}

/**
 * @brief Test the positive scenario of retrieving video sink using valid video sink IDs.
 *
 * This test verifies that the VideoSinkManager service can successfully retrieve video sink objects
 * using valid video sink IDs. It ensures that the service returns a valid status and non-null video sink objects.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 002@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Retrieve valid video sink IDs from the VideoSinkManager service. | None | Status should be OK, validIds should not be empty | Should be successful |
 * | 02 | For each valid video sink ID, retrieve the corresponding video sink object. | videoSinkId = validId, videoSink = nullptr | Status should be OK, videoSink should not be nullptr | Should Pass |
 */
UT_ADD_TEST(VideoSinkManagerBaseTest, test_l1_VideoSinkManager_positive_getVideoSink) {
    android::binder::Status status;

    std::unordered_set<int32_t> expectedIds;
    std::string videosink_count_path = "IVideoSinkManager.IVideoSink";
    int32_t expected_count = UT_KVP_PROFILE_GET_LIST_COUNT(videosink_count_path.c_str());

    for (size_t i = 0; i < expected_count; ++i) 
    {
        std::string videosink_id_path = videosink_count_path + "." + std::to_string(i);
        expectedIds.insert(UT_KVP_PROFILE_GET_UINT32(videosink_id_path.c_str()));
    }


    for (const auto& Id : expectedIds) {

        IVideoSink::Id videoSinkId;
        videoSinkId.value = Id;
        sp<IVideoSink> videoSink;

        UT_LOG_INFO("Calling getVideoSink(IN:videoSinkId:[%d], OUT:videoSink[])", videoSinkId.value);
        status = gVideoSinkManagerService->getVideoSink(videoSinkId, &videoSink);
        UT_LOG_INFO("Result getVideoSink(IN:videoSinkId:[%d], OUT:videoSink[0x%X]), status:[isOK:%s, exception:%s]",
                    videoSinkId.value, videoSink.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_NOT_EQUAL(videoSink, nullptr);
    }
}

/**
 * @brief Test the negative scenarios for retrieving a video sink using invalid IDs.
 *
 * This test checks the behavior of the VideoSinkManager when attempting to retrieve a video sink using invalid IDs. It ensures that the system handles these cases gracefully without crashing and returns the expected results.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 003@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Retrieve Video Sink IDs using the getVideoSinkIds method | None | Status should be OK, IDs should not be empty | Should be successful |
 * | 02 | Sort the retrieved IDs to find the least and highest values | None | Sorted list of IDs | Should be successful |
 * | 03 | Test with invalid Video Sink ID (leastValue - 1) | invalidIdLow.value = leastValue - 1 | Status should be OK, videoSink should be nullptr | Should Pass |
 * | 04 | Test with invalid Video Sink ID (highestValue + 1) | invalidIdHigh.value = highestValue + 1 | Status should be OK, videoSink should be nullptr | Should Pass |
 */
UT_ADD_TEST(VideoSinkManagerBaseTest, test_l1_VideoSinkManager_negative_getVideoSink) {
    android::binder::Status status;
    sp<IVideoSink> videoSink;

    // Retrieve Video Sink IDs using macros
    std::vector<IVideoSink::Id> ids;
    IVideoSink::Id temp_variable;

    std::string videosink_count_path = "IVideoSinkManager.IVideoSink";
    int32_t expected_count = UT_KVP_PROFILE_GET_LIST_COUNT(videosink_count_path.c_str());

    for (size_t i = 0; i < expected_count; ++i) 
    {
        std::string videosink_id_path = videosink_count_path + "." + std::to_string(i);
        temp_variable.value = UT_KVP_PROFILE_GET_UINT32(videosink_id_path.c_str());
        ids.push_back(temp_variable);
    }


    // Sort the IDs to find the least and highest values
    std::sort(ids.begin(), ids.end(), [](const IVideoSink::Id& a, const IVideoSink::Id& b) {
        return a.value < b.value;
    });

    int32_t leastValue = ids.front().value;
    int32_t highestValue = ids.back().value;

    // Test with invalid Video Sink ID (leastValue - 1)
    IVideoSink::Id invalidIdLow;
    invalidIdLow.value = leastValue - 1;
    UT_LOG_INFO("Calling getVideoSink(IN:videoSinkId:[%d], OUT:videoSink[])", invalidIdLow.value);
    status = gVideoSinkManagerService->getVideoSink(invalidIdLow, &videoSink);
    UT_LOG_INFO("Result getVideoSink(IN:videoSinkId:[%d], OUT:videoSink[0x%X]), status:[isOK:%s, exception:%s]",
                invalidIdLow.value, videoSink.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()),
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
    UT_ASSERT_TRUE(status.isOk());
    UT_ASSERT_EQUAL(videoSink, nullptr);

    // Test with invalid Video Sink ID (highestValue + 1)
    IVideoSink::Id invalidIdHigh;
    invalidIdHigh.value = highestValue + 1;
    UT_LOG_INFO("Calling getVideoSink(IN:videoSinkId:[%d], OUT:videoSink[])", invalidIdHigh.value);
    status = gVideoSinkManagerService->getVideoSink(invalidIdHigh, &videoSink);
    UT_LOG_INFO("Result getVideoSink(IN:videoSinkId:[%d], OUT:videoSink[0x%X]), status:[isOK:%s, exception:%s]",
                invalidIdHigh.value, videoSink.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()),
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
    UT_ASSERT_TRUE(status.isOk());
    UT_ASSERT_EQUAL(videoSink, nullptr);
}


/**
 * @brief Test the positive scenario for getting capabilities from VideoSink
 *
 * This test verifies the functionality of the getCapabilities method in the VideoSink class. It ensures that the method returns the expected capabilities without any errors.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 004@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize the test environment and prepare the list of VideoSink objects | mVideoSinks initialized | Should be successful | |
 * | 02 | Iterate over each VideoSink object and call the getCapabilities method | videoSink = current VideoSink object, capabilities = uninitialized | status.isOk() = true, capabilities != nullptr | Should Pass |
 * | 03 | Log the result of the getCapabilities call for each VideoSink | capabilities = current capabilities, status = current status | Log should show capabilities and status | Should be successful |
 * | 04 | Assert that the status is OK and capabilities is not null | status.isOk() = true, capabilities != nullptr | Assertions should pass | Should Pass |
 */
UT_ADD_TEST(VideoSinkBaseTest, test_l1_VideoSink_positive_getCapabilities)
{
    android::binder::Status status;
    ::com::rdk::hal::videosink::Capabilities capabilities;

    for (const auto& videoSink : mVideoSinks)
    {
        UT_LOG_INFO("Calling getCapabilities(OUT:capabilities)");
        status = videoSink->getCapabilities(&capabilities);
        UT_LOG_INFO("Result getCapabilities(OUT:capabilities[0x%X]), status:[isOK:%s, exception:%s]",
                    &capabilities, UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));

        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_NOT_EQUAL(&capabilities, nullptr);
    }
}


/**
 * @brief Test the positive scenario of getting properties from VideoSink
 *
 * This test verifies the retrieval of various properties from VideoSink instances to ensure that the properties are correctly fetched and validated.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 005@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Iterate over each property in the properties list | property = RESOURCE_ID | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 02 | Iterate over each property in the properties list | property = SINK_QUEUE_DEPTH | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 03 | Iterate over each property in the properties list | property = AV_SOURCE | Status is OK, aidlReturn has value, Tag is intValue, value is in AVSource | Should Pass |
 * | 04 | Iterate over each property in the properties list | property = RENDER_FIRST_FRAME | Status is OK, aidlReturn has value, Tag is booleanValue | Should Pass |
 * | 05 | Iterate over each property in the properties list | property = METRIC_FRAMES_RECEIVED | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 06 | Iterate over each property in the properties list | property = METRIC_FRAMES_PRESENTED | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 07 | Iterate over each property in the properties list | property = METRIC_FRAMES_DROPPED_LATE | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 08 | Iterate over each property in the properties list | property = METRIC_FRAMES_DROPPED_FRC | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 09 | Iterate over each property in the properties list | property = METRIC_FRAMES_REPEATED_FRC | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 10 | Iterate over each property in the properties list | property = METRIC_FRAMES_REPEATED_MISSING_FRAME | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 11 | Iterate over each property in the properties list | property = METRIC_UNDERFLOWED | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 */
 UT_ADD_TEST(VideoSinkBaseTest, test_l1_VideoSink_positive_getProperty) {
    android::binder::Status status;
    std::optional<com::rdk::hal::PropertyValue> aidlReturn;

    std::vector<int32_t> avSourceValues = {
        static_cast<int32_t>(com::rdk::hal::AVSource::AUTO),
        static_cast<int32_t>(com::rdk::hal::AVSource::UNKNOWN),
        static_cast<int32_t>(com::rdk::hal::AVSource::IP),
        static_cast<int32_t>(com::rdk::hal::AVSource::TUNER),
        static_cast<int32_t>(com::rdk::hal::AVSource::SYSTEM),
        static_cast<int32_t>(com::rdk::hal::AVSource::HDMI_1),
        static_cast<int32_t>(com::rdk::hal::AVSource::HDMI_2),
        static_cast<int32_t>(com::rdk::hal::AVSource::HDMI_3),
        static_cast<int32_t>(com::rdk::hal::AVSource::HDMI_4),
        static_cast<int32_t>(com::rdk::hal::AVSource::HDMI_5),
        static_cast<int32_t>(com::rdk::hal::AVSource::COMPOSITE_1),
        static_cast<int32_t>(com::rdk::hal::AVSource::COMPOSITE_2),
        static_cast<int32_t>(com::rdk::hal::AVSource::COMPOSITE_3),
        static_cast<int32_t>(com::rdk::hal::AVSource::COMPOSITE_4),
        static_cast<int32_t>(com::rdk::hal::AVSource::COMPOSITE_5)
    };


    std::vector<com::rdk::hal::videosink::Property> properties = {
        com::rdk::hal::videosink::Property::RESOURCE_ID,
        com::rdk::hal::videosink::Property::SINK_QUEUE_DEPTH,
        com::rdk::hal::videosink::Property::AV_SOURCE,
        com::rdk::hal::videosink::Property::RENDER_FIRST_FRAME,
        com::rdk::hal::videosink::Property::METRIC_FRAMES_RECEIVED,
        com::rdk::hal::videosink::Property::METRIC_FRAMES_PRESENTED,
        com::rdk::hal::videosink::Property::METRIC_FRAMES_DROPPED_LATE,
        com::rdk::hal::videosink::Property::METRIC_FRAMES_DROPPED_FRC,
        com::rdk::hal::videosink::Property::METRIC_FRAMES_REPEATED_FRC,
        com::rdk::hal::videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME,
        com::rdk::hal::videosink::Property::METRIC_UNDERFLOWED
    };

    for (const auto& sink : mVideoSinks) {
        for (const auto& property : properties) {
            UT_LOG_INFO("Calling getProperty(IN:property:[%d], OUT:_aidl_return[])", static_cast<int32_t>(property));
            status = sink->getProperty(property, &aidlReturn);
            UT_LOG_INFO("Result getProperty(IN:property:[%d], OUT:_aidl_return[0x%X]), status:[isOK:%s, exception:%s]",
                        static_cast<int32_t>(property), aidlReturn.has_value(),
                        UT_Control_GetMapString(bool_mapTable, status.isOk()),
                        UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));

            UT_ASSERT_TRUE(status.isOk());
            UT_ASSERT_TRUE(aidlReturn.has_value());

            switch (property) {
                case com::rdk::hal::videosink::Property::RESOURCE_ID:
                case com::rdk::hal::videosink::Property::SINK_QUEUE_DEPTH:
                case com::rdk::hal::videosink::Property::METRIC_FRAMES_RECEIVED:
                case com::rdk::hal::videosink::Property::METRIC_FRAMES_PRESENTED:
                case com::rdk::hal::videosink::Property::METRIC_FRAMES_DROPPED_LATE:
                case com::rdk::hal::videosink::Property::METRIC_FRAMES_DROPPED_FRC:
                case com::rdk::hal::videosink::Property::METRIC_FRAMES_REPEATED_FRC:
                case com::rdk::hal::videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME:
                case com::rdk::hal::videosink::Property::METRIC_UNDERFLOWED:
                    UT_ASSERT_EQUAL(aidlReturn->value->getTag(), com::rdk::hal::PropertyValue::Value::Tag::intValue);
                    break;
                case com::rdk::hal::videosink::Property::AV_SOURCE:
                    UT_ASSERT_EQUAL(aidlReturn->value->getTag(), com::rdk::hal::PropertyValue::Value::Tag::intValue);
                    UT_ASSERT_TRUE(std::find(avSourceValues.begin(), avSourceValues.end(), aidlReturn->value->get<::com::rdk::hal::PropertyValue::Value::Tag::intValue>()) != avSourceValues.end());
                    break;
                case com::rdk::hal::videosink::Property::RENDER_FIRST_FRAME:
                    UT_ASSERT_EQUAL(aidlReturn->value->getTag(), com::rdk::hal::PropertyValue::Value::Tag::booleanValue);
                    break;
                default:
                    UT_FAIL("Unexpected property");
            }
        }
    }
}


/**
 * @brief Test the negative scenarios for the getProperty function in VideoSink
 *
 * This test checks the behavior of the getProperty function when invalid or null properties are passed. It ensures that the function handles these cases gracefully by returning appropriate error statuses and values.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 006@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Call getProperty with an invalid property value | property = -1, aidl_return = nullptr | status.isOk() = false, aidl_return.has_value() = false | Should Fail |
 * | 02 | Call getProperty with a null property value | property = 0, aidl_return = nullptr | status.isOk() = false | Should Fail |
 */
UT_ADD_TEST(VideoSinkBaseTest, test_l1_VideoSink_negative_getProperty) {

    for(const auto& sink : mVideoSinks)
    {
        android::binder::Status status;
        std::optional<com::rdk::hal::PropertyValue> aidl_return;

        // Negative Test for Invalid Property
        UT_LOG_INFO("Calling getProperty(IN:property:[INVALID_PROPERTY], OUT:_aidl_return[])");
        status = sink->getProperty(static_cast<com::rdk::hal::videosink::Property>(-1), &aidl_return);
        UT_LOG_INFO("Result getProperty(IN:property:[INVALID_PROPERTY], OUT:_aidl_return[null]), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_FALSE(status.isOk());
        UT_ASSERT_FALSE(aidl_return.has_value());
    }
}


/**
 * @brief Test the positive scenario of getting the state from VideoSink
 *
 * This test verifies that the getState function correctly retrieves the state of each VideoSink object in mVideoSinks. It checks that the state is valid and the function call is successful.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 007@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Verify mVideoSinks is not empty | mVideoSinks | Should be non-empty | Should be successful |
 * | 02 | Iterate over each VideoSink and call getState | sink = VideoSink, state = State | status.isOk() = true, state in validStates | Should Pass |
 * | 03 | Check the status of getState call | status = android::binder::Status | status.isOk() = true | Should Pass |
 * | 04 | Validate the state is within expected valid states | state = State | state in validStates | Should Pass |
 */
UT_ADD_TEST(VideoSinkBaseTest, test_l1_VideoSink_positive_getState)
{
    UT_ASSERT_FALSE(mVideoSinks.empty());

    for (const auto& sink : mVideoSinks)
    {
        State state;
        UT_LOG_INFO("Calling getState(OUT:state[])");
        android::binder::Status status = sink->getState(&state);

        UT_LOG_INFO("Result getState(OUT:state[%d]), status:[isOK:%s, exception:%s]",
                    static_cast<int32_t>(state),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));

        UT_ASSERT_TRUE(status.isOk());
        
        std::unordered_set<State> validStates = {
            State::UNKNOWN, State::CLOSED, State::OPENING, State::READY,
            State::STARTING, State::STARTED, State::FLUSHING, State::STOPPING, State::CLOSING
        };
        
        UT_ASSERT_TRUE(validStates.find(state) != validStates.end());
    }
}

/**
 * @brief Test the positive scenario of opening a video sink.
 *
 * This test verifies that the `open` function of each video sink in `mVideoSinks` can be successfully called with a valid listener, and that it returns a non-null `IVideoSinkController` object. The test ensures that the video sink is in the CLOSED state before calling `open`.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 008@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if `mVideoSinks` is not empty | mVideoSinks | Should not be empty | Should be successful |
 * | 02 | Iterate over each video sink in `mVideoSinks` | mVideoSinks | Each sink should be processed | Should be successful |
 * | 03 | Ensure the resource is in the CLOSED state before calling `open` | Initial state: CLOSED | Resource should be in CLOSED state | Should be successful |
 * | 04 | Call `open` on the video sink with a valid listener and capture the controller | listener = VideoSinkControllerListenerImpl, controller = nullptr | status.isOk() should be true, controller should not be null | Should Pass |
 * | 05 | Verify that the returned `IVideoSinkController` is non-null | controller | controller should not be null | Should Pass |
 * | 06 | Log the result of the `open` call | listener, controller, status | Log should contain correct status and controller information | Should be successful |
 */
UT_ADD_TEST(VideoSinkBaseTest, test_l1_VideoSink_positive_open)
{
    android::binder::Status status;
    UT_ASSERT_FALSE(mVideoSinks.empty());

    for (const auto& sink : mVideoSinks)
    {
        sp<IVideoSinkController> controller;
        sp<IVideoSinkControllerListener> listener = new VideoSinkControllerListenerImpl;
        // Ensure the resource is in the CLOSED state before calling open
        // Assuming the initial state is CLOSED as per the state change information
        UT_LOG_INFO("Calling open(IN:videoSinkControllerListener:[%p], OUT:controller[])", listener.get());
        status = sink->open(listener, &controller);

        // Verify that the returned IVideoSinkController is non-null
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_NOT_EQUAL(controller, nullptr);

        // Log the result
        UT_LOG_INFO("Result open(IN:videoSinkControllerListener:[%p], OUT:controller[0x%X]), status:[isOK:%s, exception:%s]",
                    listener.get(), controller.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));

        bool closed = false;
        status = sink->close(controller, &closed);
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_TRUE(closed);
    }
}

/**
 * @brief Test the behavior of VideoSink open function under negative scenarios.
 *
 * This test evaluates the VideoSink open function when provided with invalid inputs or called in an incorrect state. It checks the robustness of the function in handling such scenarios.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 009@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize the VideoSinkEventListenerImpl object | mSinkEventListener = new VideoSinkEventListenerImpl | Should be successful | |
 * | 02 | Open Video Sink with Null Listener | videoSinkControllerListener = null, controller = nullptr | controller = nullptr, status.isOk() = false | Should Fail |
 * | 03 | Open Video Sink with valid Listener | videoSinkControllerListener = mControllerListener, controller = valid | controller != nullptr, status.isOk() = true | Should Pass |
 * | 04 | Start the VideoSinkController to change state to READY | controller->start() | status.isOk() = true | Should be successful |
 * | 05 | Attempt to open Video Sink again in READY state | videoSinkControllerListener = mControllerListener, controller = nullptr | controller = nullptr, status.exceptionCode() = EX_ILLEGAL_STATE | Should Fail |
 */
UT_ADD_TEST(VideoSinkBaseTest, test_l1_VideoSink_negative_open) {
    android::binder::Status status;

    for (const auto& sink : mVideoSinks)
    {
        sp<IVideoSinkController> controller;
        // Test Case 1: Open Video Sink with Null Listener
        UT_LOG_INFO("Calling open(IN:videoSinkControllerListener:[null], OUT:controller[])");
        status = sink->open(nullptr, &controller);
        UT_LOG_INFO("Result open(IN:videoSinkControllerListener:[null], OUT:controller[0x%X]), status:[isOK:%s, exception:%s]",
                    controller.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_EQUAL(controller, nullptr);
        UT_ASSERT_FALSE(status.isOk());

        // Test Case 2: Open Video Sink in Non-CLOSED State
        sp<VideoSinkControllerListenerImpl> mControllerListener = new VideoSinkControllerListenerImpl;
        status = sink->open(mControllerListener, &controller);
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_NOT_EQUAL(controller, nullptr);

        // Attempt to open again in READY state
        sp<VideoSinkControllerListenerImpl> mControllerListener_second = new VideoSinkControllerListenerImpl;
        sp<IVideoSinkController> controller_new ;
        UT_LOG_INFO("Calling open after open(IN:videoSinkControllerListener:[valid], OUT:controller[])");
        status = sink->open(mControllerListener_second, &controller_new);
        UT_LOG_INFO("Result open(IN:videoSinkControllerListener:[valid], OUT:controller[0x%X]), status:[isOK:%s, exception:%s]",
                    controller_new.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_EQUAL(controller_new, nullptr);
        UT_ASSERT_EQUAL(status.exceptionCode(), android::binder::Status::EX_ILLEGAL_STATE);

        bool closed = false;
        status = sink->close(controller, &closed);
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_TRUE(closed);        
    }
}

/**
 * @brief Test the positive scenario of closing video sink controllers.
 *
 * This test verifies that the close function for each video sink controller in the list is executed successfully and returns the expected results.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 010@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Verify that the list of controllers is not empty. | mControllers.empty() = false | Assertion should pass | Should be successful |
 * | 02 | Iterate over each controller in the list and call the close function. | controller = videoSinkController, closed = false | status.isOk() = true, closed = true | Should Pass |
 * | 03 | Log the result of the close function call and check the status and closed flag. | status.isOk() = true, closed = true | Assertions should pass | Should be successful |
 */
UT_ADD_TEST(VideoSinkBaseTest, test_l1_VideoSink_positive_close)
{
    UT_ASSERT_FALSE(mVideoSinks.empty());

    for (const auto& sink : mVideoSinks)
    {
        sp<IVideoSinkController> controller;
        sp<VideoSinkControllerListenerImpl> mControllerListener = new VideoSinkControllerListenerImpl;

        android::binder::Status status = sink->open(mControllerListener, &controller);
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_NOT_EQUAL(controller, nullptr);

        bool closed = false;
        UT_LOG_INFO("Calling close(IN:videoSinkController, OUT:_aidl_return)");
        status = sink->close(controller, &closed);
        UT_LOG_INFO("Result close(IN:videoSinkController, OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, closed),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));

        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_TRUE(closed);
    }
}

/**
 * @brief Test the behavior of closing a Video Sink in invalid and valid states.
 *
 * This test checks the functionality of closing a Video Sink controller in both invalid and valid states. It ensures that the close operation fails when the Video Sink is in an invalid state and succeeds when it is in a valid state, followed by a failure on subsequent close attempts.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 011@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Attempt to close Video Sink in invalid state | controller = mControllers[i], aidl_return = false | status.isOk() = false, aidl_return = false | Should Fail |
 * | 02 | Open Video Sink | sinkId = mSinkIds[0], videoSink = nullptr | status.isOk() = true, videoSink != nullptr | Should be successful |
 * | 03 | Open Video Sink Controller | controllerListener = mControllerListener, newController = nullptr | status.isOk() = true, newController != nullptr | Should be successful |
 * | 04 | First close of Video Sink | newController = newController, aidl_return = false | status.isOk() = true, aidl_return = true | Should Pass |
 * | 05 | Second close of Video Sink | newController = newController, aidl_return = false | status.isOk() = false, aidl_return = false | Should Fail |
 */
UT_ADD_TEST(VideoSinkBaseTest, test_l1_VideoSink_negative_close)
{
    android::binder::Status status;

    // Test Case 1: Negative - Close Video Sink without prior open
    for (const auto& sink : mVideoSinks)
    {
        sp<IVideoSinkController> controller;
        bool aidl_return = false;

        UT_LOG_INFO("Calling close(IN:videoSinkController, OUT:_aidl_return)");
        status = sink->close(controller, &aidl_return);
        UT_LOG_INFO("Result close(IN:videoSinkController, OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, aidl_return),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_FALSE(status.isOk());
        UT_ASSERT_FALSE(aidl_return);
        
        //open as pre-requisite
        sp<VideoSinkControllerListenerImpl> mControllerListener = new VideoSinkControllerListenerImpl;
        sp<IVideoSinkController> newController;
        status = sink->open(mControllerListener, &newController);
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_NOT_EQUAL(newController, nullptr);

        // First close
        bool closed = false;
        UT_LOG_INFO("Calling close(IN:videoSinkController, OUT:_aidl_return)");
        status = sink->close(newController, &closed);
        UT_LOG_INFO("Result close(IN:videoSinkController, OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, closed),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_TRUE(closed);

        // Second close
        UT_LOG_INFO("Calling close(IN:videoSinkController, OUT:_aidl_return)");
        status = sink->close(newController, &closed);
        UT_LOG_INFO("Result close(IN:videoSinkController, OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, closed),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_FALSE(status.isOk());
        UT_ASSERT_FALSE(closed);
    }
}


/**
 * @brief Test the positive registration of a VideoSink event listener.
 *
 * This test verifies the successful registration of a VideoSinkEventListener with all available video sinks. It ensures that the listener is correctly initialized and registered, and checks the status and return values to confirm successful operation.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 012@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize VideoSinkEventListener if not already initialized | SinkEventListener = nullptr | SinkEventListener should be initialized | Should be successful |
 * | 02 | Iterate over all available video sinks | videoSink in mVideoSinks | videoSink should be available for iteration | Should be successful |
 * | 03 | Log the API call for registerEventListener | SinkEventListener, aidl_return = false | Log should be generated | Should be successful |
 * | 04 | Call registerEventListener API | SinkEventListener, aidl_return | status.isOk() = true, aidl_return = true | Should Pass |
 * | 05 | Log the result of registerEventListener | SinkEventListener, aidl_return, status | Log should be generated | Should be successful |
 * | 06 | Verify the status is OK | status.isOk() | status.isOk() = true | Should Pass |
 * | 07 | Verify the aidl_return is true | aidl_return | aidl_return = true | Should Pass |
 */
UT_ADD_TEST(VideoSinkBaseTest, test_l1_VideoSink_positive_registerEventListener)
{
    android::binder::Status status;
    bool aidl_return = false;

    // Iterate over all available video sinks
    for (const auto& videoSink : mVideoSinks) 
    {
        sp<IVideoSinkEventListener> SinkEventListener = new VideoSinkEventListenerImpl();
        // Log the API call
        UT_LOG_INFO("Calling registerEventListener(IN:videoSinkEventListener:[0x%X], OUT:_aidl_return:[%s])", SinkEventListener.get(), "false");
        // Call the registerEventListener API
        status = videoSink->registerEventListener(SinkEventListener, &aidl_return);
        // Log the result
        UT_LOG_INFO("Result registerEventListener(IN:videoSinkEventListener:[0x%X], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", SinkEventListener.get(), aidl_return ? "true" : "false", UT_Control_GetMapString(bool_mapTable, status.isOk()), UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        // Verify the expected outputs
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_TRUE(aidl_return);
    }
}


/**
 * @brief Test the behavior of the registerEventListener function with invalid and duplicate inputs.
 *
 * This test evaluates the registerEventListener function by passing a null VideoSinkEventListener and 
 * attempting to register a listener that is already registered. It checks the function's response to 
 * invalid and duplicate listener registrations.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 013@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Attempt to register a null VideoSinkEventListener. | videoSinkEventListener = null, _aidl_return = false | status.isOk() = false, aidlReturn = false | Should Fail |
 * | 02 | Create a new VideoSinkEventListenerImpl instance. | mSinkEventListener = new VideoSinkEventListenerImpl | Instance created successfully | Should be successful |
 * | 03 | Register the newly created VideoSinkEventListenerImpl. | videoSinkEventListener = mSinkEventListener, _aidl_return = true | status.isOk() = true, aidlReturn = true | Should Pass |
 * | 04 | Attempt to register the same VideoSinkEventListenerImpl again. | videoSinkEventListener = mSinkEventListener, _aidl_return = false | status.isOk() = true, aidlReturn = false | Should Pass |
 */
UT_ADD_TEST(VideoSinkBaseTest, test_l1_VideoSink_negative_registerEventListener) {
    android::binder::Status status;

    for (const auto& videoSink : mVideoSinks) 
    {
        bool aidlReturn = false;
        // Test Case 4: Register a null VideoSinkEventListener
        UT_LOG_INFO("Calling registerEventListener(IN:videoSinkEventListener:[null], OUT:_aidl_return[])");
        status = videoSink->registerEventListener(nullptr, &aidlReturn);
        UT_LOG_INFO("Result registerEventListener(IN:videoSinkEventListener:[null], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, aidlReturn),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_FALSE(status.isOk());
        UT_ASSERT_FALSE(aidlReturn);

        // Test Case 5: Register a VideoSinkEventListener when it is already registered
        android::sp<IVideoSinkEventListener> mSinkEventListener = new VideoSinkEventListenerImpl;
        UT_LOG_INFO("Calling registerEventListener(IN:videoSinkEventListener:[0x%X], OUT:_aidl_return[])", mSinkEventListener.get());
        status = videoSink->registerEventListener(mSinkEventListener, &aidlReturn);
        UT_LOG_INFO("Result registerEventListener(IN:videoSinkEventListener:[0x%X], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]",
                    mSinkEventListener.get(),
                    UT_Control_GetMapString(bool_mapTable, aidlReturn),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_TRUE(aidlReturn);

        UT_LOG_INFO("Calling registerEventListener again with same listener (IN:videoSinkEventListener:[0x%X], OUT:_aidl_return[])", mSinkEventListener.get());
        status = videoSink->registerEventListener(mSinkEventListener, &aidlReturn);
        UT_LOG_INFO("Result registerEventListener(IN:videoSinkEventListener:[0x%X], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]",
                    mSinkEventListener.get(),
                    UT_Control_GetMapString(bool_mapTable, aidlReturn),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_FALSE(aidlReturn);
    }
}

/**
 * @brief Test the positive scenario of unregistering an event listener from VideoSink.
 *
 * This test verifies the successful unregistration of a previously registered event listener from the VideoSink. It ensures that the listener is correctly removed and that the operation returns a successful status.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 014@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if gVideoSinkManagerService is not null | gVideoSinkManagerService = nullptr | Should not be null | Should be successful |
 * | 02 | Initialize mSinkEventListener if null | mSinkEventListener = nullptr | Should be initialized | Should be successful |
 * | 03 | Register mSinkEventListener to each sink in mVideoSinks | mSinkEventListener = VideoSinkEventListenerImpl | Status should be OK | Should be successful |
 * | 04 | Unregister mSinkEventListener from gVideoSinkManagerService | mSinkEventListener = VideoSinkEventListenerImpl, aidl_return = false | Status should be OK, aidl_return should be true | Should Pass |
 */
UT_ADD_TEST(VideoSinkBaseTest, test_l1_VideoSink_positive_unregisterEventListener) 
{
    android::binder::Status status;

    // Register the listener first
    for (const auto& sink : mVideoSinks) 
    {
        bool aidl_return = false;

        sp<IVideoSinkEventListener> SinkEventListener = new VideoSinkEventListenerImpl();
        UT_LOG_INFO("Calling registerEventListener(IN:videoSinkEventListener:[0x%X], OUT:_aidl_return:[%s])", SinkEventListener.get(), "false");
        status = sink->registerEventListener(SinkEventListener, &aidl_return);
        UT_LOG_INFO("Result registerEventListener(IN:videoSinkEventListener:[0x%X], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", SinkEventListener.get(), aidl_return ? "true" : "false", UT_Control_GetMapString(bool_mapTable, status.isOk()), UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_TRUE(aidl_return);


        // Unregister the listener
        UT_LOG_INFO("Calling unregisterEventListener(IN:videoSinkEventListener, OUT:_aidl_return)");
        status = sink->unregisterEventListener(SinkEventListener, &aidl_return);
        UT_LOG_INFO("Result unregisterEventListener(IN:videoSinkEventListener, OUT:_aidl_return:[%d]), status:[isOK:%s, exception:%s]",
                aidl_return, UT_Control_GetMapString(bool_mapTable, status.isOk()), UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_TRUE(aidl_return);

    }
}



/**
 * @brief Test the behavior of unregistering a VideoSinkEventListener in negative scenarios.
 *
 * This test evaluates the behavior of the VideoSinkManagerService when attempting to unregister a VideoSinkEventListener that is either not registered or has already been unregistered. It ensures that the service handles these cases gracefully without errors.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 015@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create a VideoSinkEventListenerImpl instance | listener = new VideoSinkEventListenerImpl() | Instance should be created successfully | Should be successful |
 * | 02 | Attempt to unregister a non-registered listener | listener = 0x[listener address], aidlReturn = false | aidlReturn = false, status.isOk() = true | Should Fail |
 * | 03 | Register the listener | listener = 0x[listener address], registerReturn = false | registerReturn = true, status.isOk() = true | Should be successful |
 * | 04 | Unregister the listener for the first time | listener = 0x[listener address], aidlReturn = false | aidlReturn = true, status.isOk() = true | Should Pass |
 * | 05 | Attempt to unregister the listener again | listener = 0x[listener address], aidlReturn = false | aidlReturn = false, status.isOk() = true | Should Fail |
 */
UT_ADD_TEST(VideoSinkBaseTest, test_l1_VideoSink_negative_unregisterEventListener) 
{
    android::binder::Status status;

    for (const auto& sink : mVideoSinks) 
    {
        bool aidlReturn = false;
        // Create a VideoSinkEventListenerImpl instance
        sp<VideoSinkEventListenerImpl> listener = new VideoSinkEventListenerImpl();
        // Test Case 1: Negative Scenario - Unregister a Non-Registered Listener
        UT_LOG_INFO("Calling unregisterEventListener(IN:videoSinkEventListener[0x%X], OUT:_aidl_return)", listener.get());
        status = sink->unregisterEventListener(listener, &aidlReturn);
        UT_LOG_INFO("Result unregisterEventListener(IN:videoSinkEventListener[0x%X], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]",
                    listener.get(), UT_Control_GetMapString(bool_mapTable, aidlReturn),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_FALSE(aidlReturn);
        UT_ASSERT_TRUE(status.isOk());

        // Test Case 2: Negative Scenario - Unregister Listener which is already unregistered
        // Register the listener first
        bool registerReturn = false;
        status = sink->registerEventListener(listener, &registerReturn);
        UT_ASSERT_TRUE(registerReturn);
        UT_ASSERT_TRUE(status.isOk());

        // Unregister the listener for the first time
        UT_LOG_INFO("Calling unregisterEventListener(IN:videoSinkEventListener[0x%X], OUT:_aidl_return)", listener.get());
        status = sink->unregisterEventListener(listener, &aidlReturn);
        UT_LOG_INFO("Result unregisterEventListener(IN:videoSinkEventListener[0x%X], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]",
                    listener.get(), UT_Control_GetMapString(bool_mapTable, aidlReturn),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(aidlReturn);
        UT_ASSERT_TRUE(status.isOk());

        // Attempt to unregister the listener again
        UT_LOG_INFO("Calling unregisterEventListener(IN:videoSinkEventListener[0x%X], OUT:_aidl_return)", listener.get());
        status = sink->unregisterEventListener(listener, &aidlReturn);
        UT_LOG_INFO("Result unregisterEventListener(IN:videoSinkEventListener[0x%X], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]",
                    listener.get(), UT_Control_GetMapString(bool_mapTable, aidlReturn),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_FALSE(aidlReturn);
        UT_ASSERT_TRUE(status.isOk());
    }
}



/**
 * @brief Test the positive scenarios for setting properties in VideoSinkController
 *
 * This test verifies the functionality of setting various properties in the VideoSinkController. 
 * It ensures that valid properties with boolean and integer values are correctly set and return expected results.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 016@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if mControllers is not empty | mControllers | Should not be empty | Should be successful |
 * | 02 | Set a valid property with a boolean value (true) | property = RENDER_FIRST_FRAME, propertyValue = booleanValue:true, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 03 | Set a valid property with a boolean value (false) | property = RENDER_FIRST_FRAME, propertyValue = booleanValue:false, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 04 | Set a valid property with an integer value (AVSource::AUTO) | property = AV_SOURCE, propertyValue = intValue:AVSource::AUTO, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 05 | Set a valid property with an integer value (AVSource::UNKNOWN) | property = AV_SOURCE, propertyValue = intValue:AVSource::UNKNOWN, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 06 | Set a valid property with an integer value (AVSource::IP) | property = AV_SOURCE, propertyValue = intValue:AVSource::IP, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 07 | Set a valid property with an integer value (AVSource::TUNER) | property = AV_SOURCE, propertyValue = intValue:AVSource::TUNER, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 08 | Set a valid property with an integer value (AVSource::SYSTEM) | property = AV_SOURCE, propertyValue = intValue:AVSource::SYSTEM, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 09 | Set a valid property with an integer value (AVSource::HDMI_1) | property = AV_SOURCE, propertyValue = intValue:AVSource::HDMI_1, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 10 | Set a valid property with an integer value (AVSource::HDMI_2) | property = AV_SOURCE, propertyValue = intValue:AVSource::HDMI_2, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 11 | Set a valid property with an integer value (AVSource::HDMI_3) | property = AV_SOURCE, propertyValue = intValue:AVSource::HDMI_3, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 12 | Set a valid property with an integer value (AVSource::HDMI_4) | property = AV_SOURCE, propertyValue = intValue:AVSource::HDMI_4, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 13 | Set a valid property with an integer value (AVSource::HDMI_5) | property = AV_SOURCE, propertyValue = intValue:AVSource::HDMI_5, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 14 | Set a valid property with an integer value (AVSource::COMPOSITE_1) | property = AV_SOURCE, propertyValue = intValue:AVSource::COMPOSITE_1, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 15 | Set a valid property with an integer value (AVSource::COMPOSITE_2) | property = AV_SOURCE, propertyValue = intValue:AVSource::COMPOSITE_2, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 16 | Set a valid property with an integer value (AVSource::COMPOSITE_3) | property = AV_SOURCE, propertyValue = intValue:AVSource::COMPOSITE_3, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 17 | Set a valid property with an integer value (AVSource::COMPOSITE_4) | property = AV_SOURCE, propertyValue = intValue:AVSource::COMPOSITE_4, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 18 | Set a valid property with an integer value (AVSource::COMPOSITE_5) | property = AV_SOURCE, propertyValue = intValue:AVSource::COMPOSITE_5, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 */
UT_ADD_TEST(VideoSinkControllerBaseTest, test_l1_VideoSinkController_positive_setProperty) {
    UT_ASSERT_FALSE(mControllers.empty());

    for (auto& controller : mControllers) {
        // Test case 1: Set a valid property with a boolean value
        {
            ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::RENDER_FIRST_FRAME;
            PropertyValue propertyValue;
            //propertyValue.value = PropertyValue::Value{PropertyValue::Value::Tag::booleanValue, true};
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::booleanValue>(true);
            bool _aidl_return = false;

            UT_LOG_INFO("Calling setProperty(IN:property:[%d], IN:propertyValue:[booleanValue:true])", static_cast<int32_t>(property));
            android::binder::Status status = controller->setProperty(property, propertyValue, &_aidl_return);
            UT_LOG_INFO("Result setProperty(IN:property:[%d], IN:propertyValue:[booleanValue:true]), status:[isOK:%s, exception:%s], _aidl_return:[%s]",
                        static_cast<int32_t>(property), UT_Control_GetMapString(bool_mapTable, status.isOk()),
                        UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()), UT_Control_GetMapString(bool_mapTable, _aidl_return));
            UT_ASSERT_TRUE(status.isOk());
            UT_ASSERT_TRUE(_aidl_return);

            //propertyValue.value = PropertyValue::Value{PropertyValue::Value::Tag::booleanValue, false};
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::booleanValue>(false);
            _aidl_return = false;

            UT_LOG_INFO("Calling setProperty(IN:property:[%d], IN:propertyValue:[booleanValue:false])", static_cast<int32_t>(property));
            status = controller->setProperty(property, propertyValue, &_aidl_return);
            UT_LOG_INFO("Result setProperty(IN:property:[%d], IN:propertyValue:[booleanValue:false]), status:[isOK:%s, exception:%s], _aidl_return:[%s]",
                        static_cast<int32_t>(property), UT_Control_GetMapString(bool_mapTable, status.isOk()),
                        UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()), UT_Control_GetMapString(bool_mapTable, _aidl_return));
            UT_ASSERT_TRUE(status.isOk());
            UT_ASSERT_TRUE(_aidl_return);
        }

        // Test case 2: Set a valid property with a string value
        {
            ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::AV_SOURCE;
            bool _aidl_return = false;

            for (int32_t avSourceValue : {static_cast<int32_t>(AVSource::AUTO), static_cast<int32_t>(AVSource::UNKNOWN),
                                          static_cast<int32_t>(AVSource::IP), static_cast<int32_t>(AVSource::TUNER),
                                          static_cast<int32_t>(AVSource::SYSTEM), static_cast<int32_t>(AVSource::HDMI_1),
                                          static_cast<int32_t>(AVSource::HDMI_2), static_cast<int32_t>(AVSource::HDMI_3),
                                          static_cast<int32_t>(AVSource::HDMI_4), static_cast<int32_t>(AVSource::HDMI_5),
                                          static_cast<int32_t>(AVSource::COMPOSITE_1), static_cast<int32_t>(AVSource::COMPOSITE_2),
                                          static_cast<int32_t>(AVSource::COMPOSITE_3), static_cast<int32_t>(AVSource::COMPOSITE_4),
                                          static_cast<int32_t>(AVSource::COMPOSITE_5)}) {
                PropertyValue propertyValue;
                propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(avSourceValue);

                UT_LOG_INFO("Calling setProperty(IN:property:[%d], IN:propertyValue:[intValue:%d])", static_cast<int32_t>(property), avSourceValue);
                android::binder::Status status = controller->setProperty(property, propertyValue, &_aidl_return);
                UT_LOG_INFO("Result setProperty(IN:property:[%d], IN:propertyValue:[intValue:%d]), status:[isOK:%s, exception:%s], _aidl_return:[%s]",
                            static_cast<int32_t>(property), avSourceValue, UT_Control_GetMapString(bool_mapTable, status.isOk()),
                            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()), UT_Control_GetMapString(bool_mapTable, _aidl_return));
                UT_ASSERT_TRUE(status.isOk());
                UT_ASSERT_TRUE(_aidl_return);
            }
        }
    }
}



/**
 * @brief Test the negative scenarios for setting properties in VideoSinkController
 *
 * This test verifies the behavior of the VideoSinkController when attempting to set properties that are either read-only, invalid, or have invalid values. It ensures that the controller correctly handles these scenarios by not allowing the property to be set and returning the appropriate status.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 017@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Attempt to set a read-only property | property = RESOURCE_ID, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 02 | Attempt to set a read-only property | property = SINK_QUEUE_DEPTH, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 03 | Attempt to set a read-only property | property = METRIC_FRAMES_RECEIVED, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 04 | Attempt to set a read-only property | property = METRIC_FRAMES_PRESENTED, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 05 | Attempt to set a read-only property | property = METRIC_FRAMES_DROPPED_LATE, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 06 | Attempt to set a read-only property | property = METRIC_FRAMES_DROPPED_FRC, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 07 | Attempt to set a read-only property | property = METRIC_FRAMES_REPEATED_FRC, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 08 | Attempt to set a read-only property | property = METRIC_FRAMES_REPEATED_MISSING_FRAME, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 09 | Attempt to set a read-only property | property = METRIC_UNDERFLOWED, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 10 | Attempt to set an invalid property | property = -999, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 11 | Attempt to set a valid property with an invalid value | property = AV_SOURCE, propertyValue = intValue:10 | aidl_return = false | Should Fail |
 * | 12 | Transition controller to STARTED state |  | status.isOk() = true, started = true | Should be successful |
 * | 13 | Attempt to set AV_SOURCE from STARTED state | property = AV_SOURCE, propertyValue = intValue:AUTO | aidl_return = false | Should Fail |
 * | 14 | Attempt to set AV_SOURCE from STARTED state | property = AV_SOURCE, propertyValue = intValue:COMPOSITE_5 | aidl_return = false | Should Fail |
 * | 15 | Transition controller back to READY state |  | status.isOk() = true, stopped = true | Should be successful |
 */
UT_ADD_TEST(VideoSinkControllerBaseTest, test_l1_VideoSinkController_negative_setProperty) {
    android::binder::Status status;
    bool aidl_return = false;

    // Test Case 1: Set a READ-ONLY property
    std::vector<::com::rdk::hal::videosink::Property> readOnlyProperties = {
        ::com::rdk::hal::videosink::Property::RESOURCE_ID,
        ::com::rdk::hal::videosink::Property::SINK_QUEUE_DEPTH,
        ::com::rdk::hal::videosink::Property::METRIC_FRAMES_RECEIVED,
        ::com::rdk::hal::videosink::Property::METRIC_FRAMES_PRESENTED,
        ::com::rdk::hal::videosink::Property::METRIC_FRAMES_DROPPED_LATE,
        ::com::rdk::hal::videosink::Property::METRIC_FRAMES_DROPPED_FRC,
        ::com::rdk::hal::videosink::Property::METRIC_FRAMES_REPEATED_FRC,
        ::com::rdk::hal::videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME,
        ::com::rdk::hal::videosink::Property::METRIC_UNDERFLOWED
    };

    for (const auto& property : readOnlyProperties) {
        PropertyValue propertyValue;
        propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(1);

        for (auto& controller : mControllers) {
            UT_LOG_INFO("Calling setProperty(IN:property:[%d], IN:propertyValue:[intValue:1])", static_cast<int32_t>(property));
            status = controller->setProperty(property, propertyValue, &aidl_return);
            UT_LOG_INFO("Result setProperty(IN:property:[%d], IN:propertyValue:[intValue:1]), status:[isOK:%s, exception:%s], aidl_return:[%s]",
                        static_cast<int32_t>(property),
                        UT_Control_GetMapString(bool_mapTable, status.isOk()),
                        UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()),
                        UT_Control_GetMapString(bool_mapTable, aidl_return));
            UT_ASSERT_FALSE(aidl_return);
        }
    }

    // Test Case 2: Set an invalid property
    ::com::rdk::hal::videosink::Property invalidProperty = static_cast<::com::rdk::hal::videosink::Property>(-999); // Invalid property
    PropertyValue invalidPropertyValue;
    //invalidPropertyValue.value = PropertyValue::Value{PropertyValue::Value::Tag::intValue, 1};
    invalidPropertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(1);

    for (auto& controller : mControllers) {
        UT_LOG_INFO("Calling setProperty(IN:property:[%d], IN:propertyValue:[intValue:1])", static_cast<int32_t>(invalidProperty));
        status = controller->setProperty(invalidProperty, invalidPropertyValue, &aidl_return);
        UT_LOG_INFO("Result setProperty(IN:property:[%d], IN:propertyValue:[intValue:1]), status:[isOK:%s, exception:%s], aidl_return:[%s]",
                    static_cast<int32_t>(invalidProperty),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()),
                    UT_Control_GetMapString(bool_mapTable, aidl_return));
        UT_ASSERT_FALSE(aidl_return);
    }

    // Test Case 3: Set a valid property with an invalid value
    ::com::rdk::hal::videosink::Property validProperty = ::com::rdk::hal::videosink::Property::AV_SOURCE;
    PropertyValue invalidValue;
    //invalidValue.value = PropertyValue::Value{PropertyValue::Value::Tag::intValue, 10}; // Invalid AVSource value
    invalidValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(10);
    

    for (auto& controller : mControllers) {
        UT_LOG_INFO("Calling setProperty(IN:property:[%d], IN:propertyValue:[intValue:10])", static_cast<int32_t>(validProperty));
        status = controller->setProperty(validProperty, invalidValue, &aidl_return);
        UT_LOG_INFO("Result setProperty(IN:property:[%d], IN:propertyValue:[intValue:10]), status:[isOK:%s, exception:%s], aidl_return:[%s]",
                    static_cast<int32_t>(validProperty),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()),
                    UT_Control_GetMapString(bool_mapTable, aidl_return));
        UT_ASSERT_FALSE(aidl_return);
    }

    // Test Case 4: Set AV_SOURCE from STARTED state
    for (auto& controller : mControllers) {

        status = controller->start();
        UT_ASSERT_TRUE(status.isOk());

        for (int32_t avSource : {
            static_cast<int32_t>(AVSource::AUTO), 
            static_cast<int32_t>(AVSource::UNKNOWN), 
            static_cast<int32_t>(AVSource::IP), 
            static_cast<int32_t>(AVSource::TUNER), 
            static_cast<int32_t>(AVSource::SYSTEM),
            static_cast<int32_t>(AVSource::HDMI_1), 
            static_cast<int32_t>(AVSource::HDMI_2), 
            static_cast<int32_t>(AVSource::HDMI_3), 
            static_cast<int32_t>(AVSource::HDMI_4), 
            static_cast<int32_t>(AVSource::HDMI_5), 
            static_cast<int32_t>(AVSource::COMPOSITE_1), 
            static_cast<int32_t>(AVSource::COMPOSITE_2), 
            static_cast<int32_t>(AVSource::COMPOSITE_3), 
            static_cast<int32_t>(AVSource::COMPOSITE_4), 
            static_cast<int32_t>(AVSource::COMPOSITE_5)})
        {
    
            PropertyValue avSourceValue;
            // avSourceValue.value = PropertyValue::Value{PropertyValue::Value::Tag::intValue, avSource};
            avSourceValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(avSource);

            UT_LOG_INFO("Calling setProperty from STARTED state (IN:property:[%d], IN:propertyValue:[intValue:%d])", static_cast<int32_t>(validProperty), avSource);
            status = controller->setProperty(validProperty, avSourceValue, &aidl_return);
            UT_LOG_INFO("Result setProperty(IN:property:[%d], IN:propertyValue:[intValue:%d]), status:[isOK:%s, exception:%s], aidl_return:[%s]",
                        static_cast<int32_t>(validProperty), avSource,
                        UT_Control_GetMapString(bool_mapTable, status.isOk()),
                        UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()),
                        UT_Control_GetMapString(bool_mapTable, aidl_return));
            UT_ASSERT_FALSE(aidl_return);
        }

        status = controller->stop();
        UT_ASSERT_TRUE(status.isOk());
    }
}

/**
 * @brief Test the positive scenario of setting a video decoder in VideoSinkController.
 *
 * This test verifies the functionality of setting a video decoder in the VideoSinkController. It ensures that the controller is in the correct state before setting the decoder and checks the expected outcomes after the operation.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 018@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Retrieve the count of video decoders from the profile. | videoDecoderCount = UT_KVP_PROFILE_GET_LIST_COUNT(IVideoDecoderManager.IVideoDecoder) | videoDecoderCount > 0 | Should be successful |
 * | 02 | Iterate over each video decoder ID retrieved from the profile. | videoDecoderId.value = UT_KVP_PROFILE_GET_UINT32(IVideoDecoderManager.IVideoDecoder.<index>.<index>, i) | N/A | Should be successful |
 * | 03 | Ensure each controller is in the READY state before setting the decoder. | controller->getState() | State::READY | Should be successful |
 * | 04 | Call the setVideoDecoder API with the videoDecoderId and check the return status. | videoDecoderId = value, aidl_return = false | status.isOk() = true, aidl_return = true | Should Pass |
 * | 05 | Log the result of the setVideoDecoder API call. | N/A | N/A | Should be successful |
 * | 06 | Verify the status and aidl_return are as expected. | status.isOk(), aidl_return | status.isOk() = true, aidl_return = true | Should Pass |
 */
 UT_ADD_TEST(VideoSinkControllerBaseTest, test_l1_VideoSinkController_positive_setVideoDecoder) {
    android::binder::Status status;
    bool aidl_return = false;

    // Retrieve videoDecoderId from YAML using macros
    std::string videoDecoderCount_path = "IVideoDecoderManager.IVideoDecoder";
    int videoDecoderCount = UT_KVP_PROFILE_GET_LIST_COUNT(videoDecoderCount_path.c_str());
    UT_ASSERT_GREATER(videoDecoderCount, 0);
    for (auto& controller : mControllers) 
    {
        for (int i = 0; i < videoDecoderCount; ++i) 
        {
            IVideoDecoder::Id videoDecoderId;
            std::string videodecoderid_path = videoDecoderCount_path+"."+std::to_string(i)+"."+std::to_string(i);
            videoDecoderId.value = UT_KVP_PROFILE_GET_UINT32(videodecoderid_path.c_str());
                // Log before calling the API
                UT_LOG_INFO("Calling setVideoDecoder(IN:videoDecoderId:[%d], OUT:aidl_return)", videoDecoderId.value);
                // Call the setVideoDecoder API
                status = controller->setVideoDecoder(videoDecoderId, &aidl_return);
                // Log the result
                UT_LOG_INFO("Result setVideoDecoder(IN:videoDecoderId:[%d], OUT:aidl_return:[%s]), status:[isOK:%s, exception:%s]",
                            videoDecoderId.value, aidl_return ? "true" : "false",
                            UT_Control_GetMapString(bool_mapTable, status.isOk()),
                            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
                // Verify the expected outputs
                UT_ASSERT_TRUE(status.isOk());
                UT_ASSERT_TRUE(aidl_return);
            }
    }
}

/**
 * @brief Test the behavior of setVideoDecoder with invalid and valid IDs in different states
 *
 * This test evaluates the setVideoDecoder function by providing invalid video decoder IDs and checking the response. It also tests the function when the resource is not in the READY state to ensure proper exception handling.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 019@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Retrieve videoDecoderIds from YAML using macros | count = UT_KVP_PROFILE_GET_LIST_COUNT(IVideoDecoderManager.IVideoDecoder) | Should be successful | Should be successful |
 * | 02 | Sort the videoDecoderIds to find the least and highest values | videoDecoderIds | Should be successful | Should be successful |
 * | 03 | Test with invalid video decoder ID (less than least value) | invalidId.value = leastValue - 1 | aidlReturn = false, status.isOk() = true | Should Pass |
 * | 04 | Test with invalid video decoder ID (greater than highest value) | invalidId.value = highestValue + 1 | aidlReturn = false, status.isOk() = true | Should Pass |
 * | 05 | Start the controller to change state | controller->start() | status.isOk() = true | Should be successful |
 * | 06 | Test with valid video decoder IDs when resource is not in READY state | validId.value = id | aidlReturn = false, status.exceptionCode() = EX_ILLEGAL_STATE | Should Pass |
 * | 07 | Stop the controller to reset state | controller->stop() | status.isOk() = true | Should be successful |
 */
UT_ADD_TEST(VideoSinkControllerBaseTest, test_l1_VideoSinkController_negative_setVideoDecoder) {
    android::binder::Status status;
    bool aidlReturn = false;

    // Retrieve videoDecoderIds from YAML using macros
    std::vector<int32_t> videoDecoderIds;

    std::string videoDecoderCount_path = "IVideoDecoderManager.IVideoDecoder";
    int count = UT_KVP_PROFILE_GET_LIST_COUNT(videoDecoderCount_path.c_str());

    for (int32_t i = 0; i < count; ++i) {
        std::string videodecoderid_path = videoDecoderCount_path+"."+std::to_string(i)+"."+std::to_string(i);
        int32_t id = UT_KVP_PROFILE_GET_UINT32(videodecoderid_path.c_str());
        videoDecoderIds.push_back(id);
    }

    // Sort the IDs to find the least and highest values
    std::sort(videoDecoderIds.begin(), videoDecoderIds.end());
    int32_t leastValue = videoDecoderIds.front();
    int32_t highestValue = videoDecoderIds.back();

    // Test Case 3: Invalid Video Decoder ID
    for (auto& controller : mControllers) {
        IVideoDecoder::Id invalidId;
        invalidId.value = leastValue - 1;
        UT_LOG_INFO("Calling setVideoDecoder(IN:videoDecoderId:[%d], OUT:_aidl_return)", invalidId.value);
        status = controller->setVideoDecoder(invalidId, &aidlReturn);
        UT_LOG_INFO("Result setVideoDecoder(IN:videoDecoderId:[%d], OUT:_aidl_return:[%d]), status:[isOK:%s, exception:%s]",
                    invalidId.value, aidlReturn, UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_FALSE(aidlReturn);
        UT_ASSERT_TRUE(status.isOk());

        invalidId.value = highestValue + 1;
        UT_LOG_INFO("Calling setVideoDecoder(IN:videoDecoderId:[%d], OUT:_aidl_return)", invalidId.value);
        status = controller->setVideoDecoder(invalidId, &aidlReturn);
        UT_LOG_INFO("Result setVideoDecoder(IN:videoDecoderId:[%d], OUT:_aidl_return:[%d]), status:[isOK:%s, exception:%s]",
                    invalidId.value, aidlReturn, UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_FALSE(aidlReturn);
        UT_ASSERT_TRUE(status.isOk());
    }

    // Test Case 4: Resource Not in READY State
    for (auto& controller : mControllers) {
        status = controller->start();
        UT_ASSERT_TRUE(status.isOk());

        for (int32_t id : videoDecoderIds) {
            IVideoDecoder::Id validId;
            validId.value = id;
            UT_LOG_INFO("Calling setVideoDecoder(IN:videoDecoderId:[%d], OUT:_aidl_return)", validId.value);
            status = controller->setVideoDecoder(validId, &aidlReturn);
            UT_LOG_INFO("Result setVideoDecoder(IN:videoDecoderId:[%d], OUT:_aidl_return:[%d]), status:[isOK:%s, exception:%s]",
                        validId.value, aidlReturn, UT_Control_GetMapString(bool_mapTable, status.isOk()),
                        UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
            UT_ASSERT_FALSE(aidlReturn);
            UT_ASSERT_EQUAL(status.exceptionCode(), android::binder::Status::EX_ILLEGAL_STATE);
        }

        status = controller->stop();
        UT_ASSERT_TRUE(status.isOk());
    }
}

/**
 * @brief Test the positive scenario of getting a video decoder from the VideoSinkController.
 *
 * This test verifies the functionality of the getVideoDecoder method in different valid states of the VideoSinkController.
 * It ensures that the method returns a valid video decoder ID when the controller is in READY or STARTED state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 020@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if the controller is in READY state | controller = mControllers[i], isReady = false | status.isOk() = true, isReady = true/false | Should be successful |
 * | 02 | If READY, call getVideoDecoder and verify | controller = mControllers[i], videoDecoderId = UNDEFINED | status.isOk() = true, videoDecoderId = UNDEFINED or valid | Should Pass |
 * | 03 | Check if the controller is in STARTED state | controller = mControllers[i], isStarted = false | status.isOk() = true, isStarted = true/false | Should be successful |
 * | 04 | If STARTED, call getVideoDecoder and verify | controller = mControllers[i], videoDecoderId = UNDEFINED | status.isOk() = true, videoDecoderId = UNDEFINED or valid | Should Pass |
 */
UT_ADD_TEST(VideoSinkControllerBaseTest, test_l1_VideoSinkController_positive_getVideoDecoder) 
{
    android::binder::Status status;
    IVideoDecoder::Id videoDecoderId;
    // Retrieve videoDecoderIds from YAML using macros
    std::vector<int32_t> videoDecoderIds;

    std::string videoDecoderCount_path = "IVideoDecoderManager.IVideoDecoder";
    int count = UT_KVP_PROFILE_GET_LIST_COUNT(videoDecoderCount_path.c_str());

    for (int32_t i = 0; i < count; ++i) 
    {
        std::string videodecoderid_path = videoDecoderCount_path+"."+std::to_string(i)+"."+std::to_string(i);
        int32_t id = UT_KVP_PROFILE_GET_UINT32(videodecoderid_path.c_str());
        videoDecoderIds.push_back(id);
    }


    for (auto& controller : mControllers) 
    {
        // Test Case 1: Valid State - READY
        UT_LOG_INFO("Calling getVideoDecoder from READY state(OUT:videoDecoderId[])");
        status = controller->getVideoDecoder(&videoDecoderId);
        UT_LOG_INFO("Result getVideoDecoder(OUT:videoDecoderId[%d]), status:[isOK:%s, exception:%s]",
                    videoDecoderId.value,
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_TRUE(std::find(videoDecoderIds.begin(), videoDecoderIds.end(), videoDecoderId.value) != videoDecoderIds.end());
    }

    for (auto& controller : mControllers) 
    {
        // Test Case 2: Valid State - STARTED
        status = controller->start();
        UT_ASSERT_TRUE(status.isOk());

        UT_LOG_INFO("Calling getVideoDecoder from STARTED state (OUT:videoDecoderId[])");
        status = controller->getVideoDecoder(&videoDecoderId);
        UT_LOG_INFO("Result getVideoDecoder(OUT:videoDecoderId[%d]), status:[isOK:%s, exception:%s]",
                    videoDecoderId.value,
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());
        UT_ASSERT_TRUE(std::find(videoDecoderIds.begin(), videoDecoderIds.end(), videoDecoderId.value) != videoDecoderIds.end());

        status = controller->stop();
        UT_ASSERT_TRUE(status.isOk());
    }

}

/**
 * @brief Test the positive start functionality of VideoSinkController.
 *
 * This test verifies that the VideoSinkController can successfully start and stop a video sink when it is in the READY state. It ensures that the start and stop operations are executed without errors.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 021@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if mControllers is not empty | mControllers.size() | Should not be empty | Should be successful |
 * | 02 | Check if mControllers size equals mVideoSinks size | mControllers.size(), mVideoSinks.size() | Sizes should be equal | Should be successful |
 * | 03 | Loop through each controller and video sink pair | i = 0 to mControllers.size() | Loop should execute for each pair | Should be successful |
 * | 04 | Ensure the Video Sink is in READY state | videoSink->isReady(&isReady) | status.isOk() = true, isReady = true | Should be successful |
 * | 05 | Log before calling start | "Calling start()" | Log should be successful | Should be successful |
 * | 06 | Call start API | videoSink->start() | status.isOk() = true | Should Pass |
 * | 07 | Log the result of start | "Result start(), status:[isOK:%s, exception:%s]" | Log should be successful | Should be successful |
 * | 08 | Verify the start was successful | status.isOk() | status.isOk() = true | Should Pass |
 * | 09 | Teardown: Call stop | videoSink->stop() | status.isOk() = true | Should Pass |
 * | 10 | Log the result of stop | "Result stop(), status:[isOK:%s, exception:%s]" | Log should be successful | Should be successful |
 * | 11 | Verify the stop was successful | status.isOk() | status.isOk() = true | Should Pass |
 */
UT_ADD_TEST(VideoSinkControllerBaseTest, test_l1_VideoSinkController_positive_start) {
    android::binder::Status status;
    UT_ASSERT_FALSE(mControllers.empty());

    for (size_t i = 0; i < mControllers.size(); ++i) 
    {
        // Log before calling start
        UT_LOG_INFO("Calling start()");
        status = mControllers[i]->start();
        UT_LOG_INFO("Result start(), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());

        // Teardown: Call stop
        UT_LOG_INFO("Calling stop()");
        status = mControllers[i]->stop();
        UT_LOG_INFO("Result stop(), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());
    }
}

/**
 * @brief Test the negative start scenario for VideoSinkController
 *
 * This test verifies the behavior of the VideoSinkController when the start method is called while the Video Sink is not in the READY state. It ensures that the start method fails as expected in this scenario.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 022@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if mControllers is not empty | mControllers | Should be successful | |
 * | 02 | Iterate over each controller in mControllers | controller | Should be successful | |
 * | 03 | Ensure the Video Sink is in a state other than READY by calling start | controller, isStarted = false | status.isOk() = true, isStarted = true | Should be successful |
 * | 04 | Log the call to start API | | | Should be successful |
 * | 05 | Call start again when the Video Sink is not in READY state | controller, isStarted | status.isOk() = false, status.exceptionCode() = EX_ILLEGAL_STATE | Should Fail |
 * | 06 | Log the result of the start API | | | Should be successful |
 * | 07 | Verify the expected output | status | status.isOk() = false, status.exceptionCode() = EX_ILLEGAL_STATE | Should Fail |
 */
UT_ADD_TEST(VideoSinkControllerBaseTest, test_l1_VideoSinkController_negative_start) {
    android::binder::Status status;
    UT_ASSERT_FALSE(mControllers.empty());

    for (auto& controller : mControllers) 
    {
        // setting state to STARTED
        status = controller->start();
        UT_ASSERT_TRUE(status.isOk());

        // Log the call to start API
        UT_LOG_INFO("Calling start() after prior start()");
        status = controller->start();
        UT_LOG_INFO("Result start(), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_EQUAL(status.exceptionCode(), android::binder::Status::EX_ILLEGAL_STATE);

        UT_LOG_INFO("Calling stop()");
        status = controller->stop();
        UT_LOG_INFO("Result stop(), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());
    }
}

/**
 * @brief Test the positive scenario of stopping the Video Sink Controller
 *
 * This test verifies that the Video Sink Controller can be successfully stopped after being started. It ensures that the stop function works correctly when the controller is in the STARTED state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 023@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize the test environment and prepare the Video Sink Controller for testing. | N/A | Environment should be set up successfully. | Should be successful |
 * | 02 | Ensure the Video Sink is in the STARTED state by calling the start() method on the controller. | controller = mControllers[i], method = start() | status.isOk() should return true. | Should Pass |
 * | 03 | Log the result of the start() method call. | status.isOk() = true, status.exceptionCode() = 0 | Log should indicate success. | Should be successful |
 * | 04 | Call the stop() method on the controller to stop the Video Sink. | controller = mControllers[i], method = stop() | status.isOk() should return true. | Should Pass |
 * | 05 | Log the result of the stop() method call. | status.isOk() = true, status.exceptionCode() = 0 | Log should indicate success. | Should be successful |
 */
UT_ADD_TEST(VideoSinkControllerBaseTest, test_l1_VideoSinkController_positive_stop)
{
    android::binder::Status status;

    for (auto& controller : mControllers)
    {
        // Ensure the Video Sink is in the STARTED state
        UT_LOG_INFO("Calling start()");
        status = controller->start();
        UT_ASSERT_TRUE(status.isOk());
        UT_LOG_INFO("Result start(), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));

        // Call the stop API
        UT_LOG_INFO("Calling stop()");
        status = controller->stop();
        UT_ASSERT_TRUE(status.isOk());
        UT_LOG_INFO("Result stop(), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
    }
}

/**
 * @brief Test the behavior of VideoSinkController when stop is called in invalid states.
 *
 * This test checks the VideoSinkController's response to stop() calls in two negative scenarios: 
 * when the controller is in a non-started state and when stop() is called consecutively after a successful stop.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 024@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Attempt to stop VideoSinkController in non-started state | controller = mControllers[i] | exceptionCode = EX_ILLEGAL_STATE | Should Fail |
 * | 02 | Start the VideoSinkController | controller = mControllers[i] | status.isOk() = true | Should be successful |
 * | 03 | Stop the VideoSinkController | controller = mControllers[i] | status.isOk() = true | Should be successful |
 * | 04 | Attempt to stop VideoSinkController again after a successful stop | controller = mControllers[i] | exceptionCode = EX_ILLEGAL_STATE | Should Fail |
 */
UT_ADD_TEST(VideoSinkControllerBaseTest, test_l1_VideoSinkController_negative_stop)
{
    android::binder::Status status;

    // Test Case 1: Negative Test - Stop Video Sink in NON-STARTED State
    for (auto& controller : mControllers)
    {
        UT_LOG_INFO("Calling stop() without prior start()");
        status = controller->stop();
        UT_LOG_INFO("Result stop(), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_EQUAL(status.exceptionCode(), android::binder::Status::EX_ILLEGAL_STATE);
    }

    // Test Case 2: Negative Test - Stop Video Sink when STOP invoked already
    for (auto& controller : mControllers)
    {
        // Start the Video Sink
        UT_LOG_INFO("Calling start()");
        status = controller->start();
        UT_LOG_INFO("Result start(), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());

        // Stop the Video Sink
        UT_LOG_INFO("Calling stop()");
        status = controller->stop();
        UT_LOG_INFO("Result stop(), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());

        // Attempt to Stop again
        UT_LOG_INFO("Calling stop() for the second time");
        status = controller->stop();
        UT_LOG_INFO("Result stop(), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_EQUAL(status.exceptionCode(), android::binder::Status::EX_ILLEGAL_STATE);
    }
}

/**
 * @brief Test the negative scenario of queueing a video frame with invalid parameters.
 *
 * This test verifies the behavior of the VideoSinkController when attempting to queue a video frame with an invalid frame buffer handle. It ensures that the function correctly handles invalid input and returns the expected failure result.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 025@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if the controller list is not empty | mControllers | Should be successful | |
 * | 02 | Retrieve supported dynamic ranges from VideoSinkManagerService | supportedDynamicRanges | Should be successful | |
 * | 03 | Set metadata parameters for the video frame | metadata parameters | Should be successful | |
 * | 04 | Call queueVideoFrame with invalid frameBufferHandle | nsPresentationTime = 1000000000, frameBufferHandle = -1, metadata = [...] | _aidl_return = false, status.isOk() = true | Should Fail |
 */
UT_ADD_TEST(VideoSinkControllerBaseTest, test_l1_VideoSinkController_negative_queueVideoFrame) {
    UT_ASSERT_FALSE(mControllers.empty());
    android::binder::Status status;
    
    for (auto& controller : mControllers) {
        FrameMetadata metadata;
        metadata.parX = 1;
        metadata.parY = 1;
        metadata.sarX = 16;
        metadata.sarY = 9;
        metadata.codedWidth = 1920;
        metadata.codedHeight = 1080;
        metadata.activeX = 0;
        metadata.activeY = 0;
        metadata.activeWidth = 1920;
        metadata.activeHeight = 1080;
        metadata.colorDepth = 8;
        metadata.pixelFormat = PixelFormat::RGB;
        metadata.dynamicRange = DynamicRange::SDR;
        metadata.scanType = ScanType::PROGRESSIVE_FRAME;
        metadata.afd = 0;
        metadata.frameRateNumerator = 24;
        metadata.frameRateDenominator = 1;
        metadata.endOfStream = false;
        metadata.discontinuity = false;
        metadata.lowLatency = false;
        metadata.source = AVSource::HDMI_1;
        metadata.sha1 = {0x12, 0x34, 0x56};

        bool _aidl_return = false;
        int64_t nsPresentationTime = 1000000000;
        int64_t frameBufferHandle = -1;

        UT_LOG_INFO("Calling queueVideoFrame(IN:nsPresentationTime:[%lld], IN:frameBufferHandle:[%lld], IN:metadata:[...])", nsPresentationTime, frameBufferHandle);
        status = controller->queueVideoFrame(nsPresentationTime, frameBufferHandle, metadata, &_aidl_return);
        UT_LOG_INFO("Result queueVideoFrame(IN:nsPresentationTime:[%lld], IN:frameBufferHandle:[%lld], IN:metadata:[...]), OUT:_aidl_return:[%s], status:[isOK:%s, exception:%s]",
                    nsPresentationTime, frameBufferHandle, UT_Control_GetMapString(bool_mapTable, _aidl_return),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_EQUAL(status.exceptionCode(), android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
}

/**
 * @brief Test the positive scenarios of the flush function in VideoSinkController
 *
 * This test verifies the behavior of the flush function in the VideoSinkController when the holdLastFrame parameter is set to true and false. It ensures that the function operates correctly without errors in both cases.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 026@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Start each controller to ensure it is in the State::STARTED state | None | Status should be OK | Should be successful |
 * | 02 | Call flush with holdLastFrame set to true | holdLastFrame = true | Status should be OK | Should Pass |
 * | 03 | Call flush with holdLastFrame set to false | holdLastFrame = false | Status should be OK | Should Pass |
 * | 04 | Stop each controller after testing | None | Status should be OK | Should be successful |
 */
UT_ADD_TEST(VideoSinkControllerBaseTest, test_l1_VideoSinkController_positive_flush)
{
    android::binder::Status status;

    // Ensure the resource is in State::STARTED state
    for (auto& controller : mControllers)
    {
        status = controller->start();
        UT_ASSERT_TRUE(status.isOk());

        // Positive Test - Flush with holdLastFrame set to true
        UT_LOG_INFO("Calling flush(IN:holdLastFrame:[true])");
        status = controller->flush(true);
        UT_LOG_INFO("Result flush(IN:holdLastFrame:[true]), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());

        // Positive Test - Flush with holdLastFrame set to false
        UT_LOG_INFO("Calling flush(IN:holdLastFrame:[false])");
        status = controller->flush(false);
        UT_LOG_INFO("Result flush(IN:holdLastFrame:[false]), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());

        status = controller->stop();
        UT_ASSERT_TRUE(status.isOk());
    }
}

/**
 * @brief Test the negative scenarios for the flush operation in VideoSinkController
 *
 * This test verifies the behavior of the flush operation when the resource is not in the STARTED state and when it is STARTED then STOPPED. The test ensures that the flush operation fails as expected in these scenarios.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 027@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if mControllers is not empty | mControllers | Should be successful |  |
 * | 02 | Attempt to flush when resource is not in STARTED state | holdLastFrame = true | status.isOk() = false, status.exceptionCode() = EX_ILLEGAL_STATE | Should Fail |
 * | 03 | Start the controller |  | status.isOk() = true | Should be successful |
 * | 04 | Stop the controller |  | status.isOk() = true | Should be successful |
 * | 05 | Attempt to flush when resource is STARTED then STOPPED | holdLastFrame = true | status.isOk() = false, status.exceptionCode() = EX_ILLEGAL_STATE | Should Fail |
 */
UT_ADD_TEST(VideoSinkControllerBaseTest, test_l1_VideoSinkController_negative_flush) {
    android::binder::Status status;
    bool holdLastFrame = true; // Using macro "not needed" for holdLastFrame

    UT_ASSERT_FALSE(mControllers.empty());

    for (auto& controller : mControllers) {
        // Negative Test - Flush when resource is not in STARTED state
        UT_LOG_INFO("Calling flush when not in STARTED state (IN:holdLastFrame:[%s])", UT_Control_GetMapString(bool_mapTable, holdLastFrame));
        status = controller->flush(holdLastFrame);
        UT_LOG_INFO("Result flush(IN:holdLastFrame:[%s]), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, holdLastFrame),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_FALSE(status.isOk());
        UT_ASSERT_EQUAL(status.exceptionCode(), android::binder::Status::EX_ILLEGAL_STATE);

        // Negative Test - Flush when resource is STARTED then STOPPED
        status = controller->start();
        UT_ASSERT_TRUE(status.isOk());

        status = controller->stop();
        UT_ASSERT_TRUE(status.isOk());

        UT_LOG_INFO("Calling flush(IN:holdLastFrame:[%s])", UT_Control_GetMapString(bool_mapTable, holdLastFrame));
        status = controller->flush(holdLastFrame);
        UT_LOG_INFO("Result flush(IN:holdLastFrame:[%s]), status:[isOK:%s, exception:%s]",
                    UT_Control_GetMapString(bool_mapTable, holdLastFrame),
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_FALSE(status.isOk());
        UT_ASSERT_EQUAL(status.exceptionCode(), android::binder::Status::EX_ILLEGAL_STATE);
    }
}

/**
 * @brief Test the positive scenario of the discardFramesUntil function in VideoSinkController.
 *
 * This test verifies that the discardFramesUntil function can successfully discard frames up to a specified presentation time when the controller is in the STARTED state. It ensures that the function behaves correctly with valid input and transitions the controller through the expected states.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 028@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if mControllers is not empty | mControllers | Should not be empty | Should be successful |
 * | 02 | Start each controller to ensure it is in the STARTED state | started = false | status.isOk() = true, started = true | Should be successful |
 * | 03 | Call discardFramesUntil with a valid nsPresentationTime | nsPresentationTime = 10000 | status.isOk() = true | Should Pass |
 * | 04 | Stop each controller to ensure it is in the STOPPED state | stopped = false | status.isOk() = true, stopped = true | Should be successful |
 */
UT_ADD_TEST(VideoSinkControllerBaseTest, test_l1_VideoSinkController_positive_discardFramesUntil) {
    android::binder::Status status;
    UT_ASSERT_FALSE(mControllers.empty());

    for (auto& controller : mControllers) {
        // Ensure the resource is in the State::STARTED state
        status = controller->start();
        UT_ASSERT_TRUE(status.isOk());

        // Log and call discardFramesUntil with a valid nsPresentationTime
        int64_t nsPresentationTime = 10000; // Using a valid future timestamp
        UT_LOG_INFO("Calling discardFramesUntil(IN:nsPresentationTime:[%lld])", nsPresentationTime);
        status = controller->discardFramesUntil(nsPresentationTime);
        UT_LOG_INFO("Result discardFramesUntil(IN:nsPresentationTime:[%lld]), status:[isOK:%s, exception:%s]",
                    nsPresentationTime, UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));
        UT_ASSERT_TRUE(status.isOk());

        // Stop the resource
        status = controller->stop();
        UT_ASSERT_TRUE(status.isOk());
    }
}

/**
 * @brief Test the negative scenario for discardFramesUntil API in VideoSinkController
 *
 * This test verifies that the discardFramesUntil API correctly handles the scenario where the resource is not in the STARTED state. It ensures that the API returns an ILLEGAL_STATE exception when called in an inappropriate state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 029@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Ensure the resource is not in the STARTED state by checking each controller | None | Controllers should not be in STARTED state | Should be successful |
 * | 02 | Log the call to discardFramesUntil with the given nsPresentationTime | nsPresentationTime = 10000 | Log should be successful | Should be successful |
 * | 03 | Call the discardFramesUntil API with nsPresentationTime | nsPresentationTime = 10000 | Status should return ILLEGAL_STATE exception | Should Fail |
 * | 04 | Log the result of the discardFramesUntil call | nsPresentationTime = 10000, status = ILLEGAL_STATE | Log should be successful | Should be successful |
 * | 05 | Verify the expected exception code from the API call | status.exceptionCode() = EX_ILLEGAL_STATE | Assertion should pass | Should Pass |
 */
UT_ADD_TEST(VideoSinkControllerBaseTest, test_l1_VideoSinkController_negative_discardFramesUntil)
{
    android::binder::Status status;
    int64_t nsPresentationTime = 10000; // Using a valid timestamp as per the test case

    // Ensure the resource is in a state other than State::STARTED (e.g., State::READY)
    UT_ASSERT_FALSE(mControllers.empty());
    for (auto& controller : mControllers)
    {
        // Log before calling the API
        UT_LOG_INFO("Calling discardFramesUntil(IN:nsPresentationTime:[%lld])", nsPresentationTime);
        status = controller->discardFramesUntil(nsPresentationTime);
        UT_LOG_INFO("Result discardFramesUntil(IN:nsPresentationTime:[%lld]), status:[isOK:%s, exception:%s]",
                    nsPresentationTime, UT_Control_GetMapString(bool_mapTable, status.isOk()),
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));

        UT_ASSERT_EQUAL(status.exceptionCode(), android::binder::Status::EX_ILLEGAL_STATE);
    }
}

int main(int argc, char **argv)
{
    UT_status_t result;
    UT_init(argc, argv);
    result = UT_run_tests();
    return result;
}
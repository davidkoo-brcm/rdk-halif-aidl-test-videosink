
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

 /**
* @file VideoSinkClient.cpp
* @page VideoSink hal Level 1 Tests
*
* ## Module's Role
* This module includes Level 1 functional tests (success and failure scenarios).
* This is to ensure that the VideoSink hal APIs meet the requirements across all vendors.
*
*/


#include <ut_kvp_profile.h>
#include <ut.h>
#include <ut_log.h>
#include <iostream>
#include <unistd.h>

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <binder/Status.h>
#include <utils/String16.h>
#include <stdio.h>
#include <vector>
#include <optional>
#include <algorithm>
#include <unordered_set>
#include "yaml_utils.h"


#include "com/rdk/hal/videosink/IVideoSinkManager.h"
#include "com/rdk/hal/videosink/BnVideoSinkControllerListener.h"
#include <com/rdk/hal/videosink/BnVideoSinkEventListener.h>

using android::sp;
using android::String16;
using android::defaultServiceManager;
using android::binder::Status;

using namespace com::rdk::hal;
using namespace com::rdk::hal::videosink;


sp<IVideoSinkManager> service;
sp<IVideoSink> _videoSink;
com::rdk::hal::videosink::IVideoSink::Id g_videosinkId;
std::vector<int> _videosink_ids_from_profile;
std::vector<int> _videodecoder_ids_from_profile;

int getService()
{
    service = android::interface_cast<IVideoSinkManager>(
            defaultServiceManager()->getService(String16(IVideoSinkManager::serviceName().c_str())));
    if (service == nullptr)
    {
        printf("Service not available\n");
        return -1;
    }
    return 0;
}

bool helper_areElementsUnique(const std::vector<IVideoSink::Id>& sinkIds) {
    std::unordered_set<int> uniqueElements;
    
    for (const auto& _sinkId : sinkIds) 
    {
        if(_sinkId.value < 0)
        {
            std::cout <<"find Invalid sink ID : " <<_sinkId.value <<std::endl;
            return false;
        } 
        if (uniqueElements.find(_sinkId.value) != uniqueElements.end()) 
        {
            std::cout<<"sink ID is not unique : " <<_sinkId.value <<std::endl;
            return false;
        }
        uniqueElements.insert(_sinkId.value);
    }
    return true;
}

bool helper_validatePropertyValue( const std::optional<::com::rdk::hal::PropertyValue>& propertyValue, const ::com::rdk::hal::videosink::Property property)
{
    if (!propertyValue.has_value() || !propertyValue->value.has_value())
    {
        return false;
    }

    std::variant<int32_t, std::string> val;

    const auto& propVal = propertyValue->value.value();
    switch (propVal.getTag())
    {
        case PropertyValue::Value::Tag::intValue:
            val = propVal.get<PropertyValue::Value::Tag::intValue>();
            break;
        case PropertyValue::Value::Tag::byteValue:
            val = propVal.get<PropertyValue::Value::Tag::byteValue>();
            break;
        case PropertyValue::Value::Tag::stringValue:
        {
            android::String16 string16Val = propVal.get<PropertyValue::Value::Tag::stringValue>();
            android::String8 string8Val(string16Val);  // Convert String16 to String8
            val = std::string(string8Val.string());    // Convert String8 to std::string
        }
            break;
        case PropertyValue::Value::Tag::booleanValue:
            val = propVal.get<PropertyValue::Value::Tag::booleanValue>() ? 1 : 0;  // Store as int32_t (1 for true, 0 for false)
            break;

        default:
            std::cerr << "Unsupported type in PropertyValue::Value" << std::endl;
            break;
    }


    switch (property)
    {
        case ::com::rdk::hal::videosink::Property::RESOURCE_ID:
             if(std::holds_alternative<int32_t>(val) && std::get<int32_t>(val) == g_videosinkId.value)
             {
                return true;
             }
             else
             {
                std::cout<<"invalid RESOURCE_ID "<<std::endl;
                return false;
             }

        case ::com::rdk::hal::videosink::Property::AV_SOURCE:
            return std::holds_alternative<int32_t>(val) &&
                   (std::get<int32_t>(val) == static_cast<int32_t>(::com::rdk::hal::AVSource::AUTO) ||
                    std::get<int32_t>(val) == static_cast<int32_t>(::com::rdk::hal::AVSource::UNKNOWN) ||
                    (std::get<int32_t>(val) >= static_cast<int32_t>(::com::rdk::hal::AVSource::IP) &&
                     std::get<int32_t>(val) <= static_cast<int32_t>(::com::rdk::hal::AVSource::SYSTEM)) ||
                    (std::get<int32_t>(val) >= static_cast<int32_t>(::com::rdk::hal::AVSource::HDMI_1) &&
                     std::get<int32_t>(val) <= static_cast<int32_t>(::com::rdk::hal::AVSource::HDMI_5)) ||
                    (std::get<int32_t>(val) >= static_cast<int32_t>(::com::rdk::hal::AVSource::COMPOSITE_1) &&
                     std::get<int32_t>(val) <= static_cast<int32_t>(::com::rdk::hal::AVSource::COMPOSITE_5)));


        /*
        * TODO: below parameters dont have specific value or range of values in the AIDL file Property.aidl
        *
        * SINK_QUEUE_DEPTH
        * METRIC_FRAMES_RECEIVED
        * METRIC_FRAMES_PRESENTED
        * METRIC_FRAMES_DROPPED_LATE
        * METRIC_FRAMES_DROPPED_FRC
        * METRIC_FRAMES_REPEATED_FRC
        * METRIC_FRAMES_REPEATED_MISSING_FRAME
        * METRIC_UNDERFLOWED
        */

        default:
            return true;
    }
}

class VideoSinkControllerListener : public BnVideoSinkControllerListener 
{
    public:

        VideoSinkControllerListener() {};
        virtual ~VideoSinkControllerListener() = default;

        ::android::binder::Status onStateChanged(::com::rdk::hal::State oldState, ::com::rdk::hal::State newState) override{
            return ::android::binder::Status::ok();
        }
        ::android::binder::Status onFirstFrameRendered(int64_t nsPresentationTime) override {
            return ::android::binder::Status::ok();
        }
        ::android::binder::Status onEndOfStream(int64_t nsPresentationTime) override {
            return ::android::binder::Status::ok();
        }
        ::android::binder::Status onVideoUnderflow() override {
            return ::android::binder::Status::ok();
        }
        ::android::binder::Status onVideoResumed() override {
            return ::android::binder::Status::ok();
        }
        ::android::binder::Status onFlushComplete() override {
            return ::android::binder::Status::ok();
        }
};

class VideoSinkEventListener : public BnVideoSinkEventListener 
{
    public:
        VideoSinkEventListener() {};
        virtual ~VideoSinkEventListener() = default;

        ::android::binder::Status onStateChanged(::com::rdk::hal::State oldState, ::com::rdk::hal::State newState) override{
            return ::android::binder::Status::ok();
        }
        ::android::binder::Status onFirstFrameRendered(int64_t nsPresentationTime) override {
            return ::android::binder::Status::ok();
        }
        ::android::binder::Status onEndOfStream(int64_t nsPresentationTime) override {
            return ::android::binder::Status::ok();
        }
        ::android::binder::Status onVideoUnderflow() override {
            return ::android::binder::Status::ok();
        }
        ::android::binder::Status onVideoResumed() override {
            return ::android::binder::Status::ok();
        }
        ::android::binder::Status onFlushComplete() override {
            return ::android::binder::Status::ok();
        }
};

class L1VideoSinkOpenTest : public virtual UTCore
{
    protected:
        void SetUp() override
        {
         //   UTCore::SetUp();
            printf("Set Up : L1VideoSinkOpenTest\n");		
            //prerequisite - checking state
            ::com::rdk::hal::State state;
            ::android::binder::Status status = _videoSink->getState(&state);
            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(state, ::com::rdk::hal::State::CLOSED);

            sp<IVideoSinkControllerListener> listener = new VideoSinkControllerListener();
            status = _videoSink->open(listener, &videoSinkController);
            ASSERT_TRUE(status.isOk());
            ASSERT_NE(videoSinkController, nullptr) << "videoSinkController is null after open()\n";

            status = _videoSink->getState(&state);
            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(state, ::com::rdk::hal::State::READY);            
        }
        void TearDown() override
        {
           // UTCore::TearDown();
            printf("Teardown : L1AudioSinkOpenTest\n");
	    ::com::rdk::hal::State state;
            ::android::binder::Status status = _videoSink->getState(&state);
            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(state, ::com::rdk::hal::State::READY);    

            bool _aidl_return;
            status = _videoSink->close(videoSinkController, &_aidl_return);
            ASSERT_TRUE(status.isOk());        
        }
        android::sp<IVideoSinkController> videoSinkController;
};

class L1VideoSinkCloseTest : public L1VideoSinkOpenTest
{
    protected:
        void SetUp() override
        {

            //prerequisite - checking state
	 //   UTCore::SetUp();
            printf("Set Up : L1VideoSinkCloseTest\n");
            ::com::rdk::hal::State state;
            ::android::binder::Status status = _videoSink->getState(&state);
            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(state, ::com::rdk::hal::State::CLOSED);

            sp<IVideoSinkControllerListener> listener = new VideoSinkControllerListener();
            status = _videoSink->open(listener, &videoSinkController);
            ASSERT_TRUE(status.isOk());
            ASSERT_NE(videoSinkController, nullptr) << "videoSinkController is null after open()\n";

            status = _videoSink->getState(&state);
            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(state, ::com::rdk::hal::State::READY);            
        }
//	void TearDown() override
  //  {
  //	    UTCore::TearDown();
   // }

        android::sp<IVideoSinkController> videoSinkController;
};

class L1VideoSinkEventRegisterTest : public UTCore
{
    protected:
        void SetUp() override
        {
            UTCore::SetUp();		
            videoSinkEventListener = new VideoSinkEventListener();
            bool _aidl_return;
            ::android::binder::Status status = _videoSink->registerEventListener(videoSinkEventListener, &_aidl_return);
            ASSERT_TRUE(status.isOk());
            ASSERT_TRUE(_aidl_return);
        }
	  void TearDown() override
    {
         UTCore::TearDown();
    }

        android::sp<::com::rdk::hal::videosink::IVideoSinkEventListener> videoSinkEventListener;
};

class L1VideoSinkStartTest : public L1VideoSinkOpenTest
{
    protected:
        void SetUp() override
        {
            L1VideoSinkOpenTest::SetUp();
	    printf("Set Up : L1VideoSinkStartTest\n");
            Status status = videoSinkController->start();
            ASSERT_TRUE(status.isOk());

            ::com::rdk::hal::State state;
            status = _videoSink->getState(&state);
            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(state, ::com::rdk::hal::State::STARTED);

        }
        void TearDown() override
        {
	      printf("Teardown : L1VideoSinkStartTest\n");	
            ::com::rdk::hal::State state;
            Status status = _videoSink->getState(&state);
            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(state, ::com::rdk::hal::State::STARTED);

            status = videoSinkController->stop();
            ASSERT_TRUE(status.isOk());
            L1VideoSinkOpenTest::TearDown();
        }
};

class L1VideoSinkStopTest : public L1VideoSinkOpenTest
{
    protected:
        void SetUp() override
        {
            L1VideoSinkOpenTest::SetUp();
	    printf("Set Up : L1VideoSinkStopTest\n");
            Status status = videoSinkController->start();
            ASSERT_TRUE(status.isOk());

            ::com::rdk::hal::State state;
            status = _videoSink->getState(&state);
            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(state, ::com::rdk::hal::State::STARTED);

        }
        void TearDown() override
        {
	   printf("Teardown : L1VideoSinkStopTest\n");	
            L1VideoSinkOpenTest::TearDown();
        }
    };

/**
* @brief Test the functionality of retrieving video sink IDs.
*
* This test evaluates the `getVideoSinkIds` method of the video sink manager service to ensure
* that it retrieves video sink IDs correctly and validates them against the HAL feature profile.
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
* | 01     | Call the `getVideoSinkIds` method from the service and get the video sink IDs | service = instance of video sink manager service | Should retrieve video sink IDs successfully | Should Pass |
* | 02     | Check that the status from `getVideoSinkIds` is Ok | status from the service call | status.isOk() should return true | Should be successful |
* | 03     | Verify that the `videoSinkIds` vector is not empty | videoSinkIds = result from above step | videoSinkIds.empty() should return false | Should be successful |
* | 04     | Call the helper function `helper_areElementsUnique` to check if all elements of `videoSinkIds` are unique | videoSinkIds from above step | helper_areElementsUnique(videoSinkIds) should return true | Should Pass |
* | 05     | Loop through the `videoSinkIds` and check if each ID exists in `_videosink_ids_from_profile` | videoSinkIds from above step, _videosink_ids_from_profile = predefined list | the ids from the api should match with ids listed in the profile | Should be successful |
*/
TEST(L1VideoSinkManagerTest, test_l1_videosink_positive_1_getVideoSinkIds)
{
    ::std::vector<IVideoSink::Id> videoSinkIds;

    ::android::binder::Status status = service->getVideoSinkIds(&videoSinkIds);
    EXPECT_TRUE(status.isOk());
    EXPECT_FALSE(videoSinkIds.empty());
    ASSERT_TRUE(helper_areElementsUnique(videoSinkIds));

    /* checking whether retrieved video sink ids from the api getVideoSinkIds() is listed in the HAL FEATURE PROFILE */
    for(const auto& _videoSinkId: videoSinkIds)
    {   
        auto result = std::find(_videosink_ids_from_profile.begin(), _videosink_ids_from_profile.end(), _videoSinkId.value);
        EXPECT_NE(result, _videosink_ids_from_profile.end());
    }
}

/**
* @brief Test the positive scenario of retrieving a video sink using valid video sink IDs.
*
* This test verifies that the `getVideoSink` API correctly retrieves video sink instances 
* for a set of valid video sink IDs sourced from a profile. It ensures the API returns a valid status 
* and non-null video sink instances.
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
* | 01 | Iterate over each video sink ID from the profile and set it in `videoSinkId`. | VideoSinkId = _videoSinkId from profile | VideoSinkId should be set correctly. | Should be successful |
* | 02 | Call the `getVideoSink` API with the current `videoSinkId` and capture the status and video sink instance. | VideoSinkId = _videoSinkId from profile, VideoSink = variable to store sink | Status should be OK and video sink should not be null. | Should Pass |
* | 03 | Assert that the status returned by `getVideoSink` is OK. | Status from API call | Status should be OK (true). | Should be successful |
* | 04 | Assert that the video sink instance is not null. | VideoSink instance | VideoSink should not be null. | Should be successful |
*/
TEST(L1VideoSinkManagerTest, test_l1_videosink_positive_1_getVideoSink)
{
    IVideoSink::Id videoSinkId;
    ::android::sp<::com::rdk::hal::videosink::IVideoSink> videoSink;

    for(const auto& _videoSinkId: _videosink_ids_from_profile)
    {
        videoSinkId.value = _videoSinkId;
        android::binder::Status status = service->getVideoSink(videoSinkId, &videoSink);
        EXPECT_TRUE(status.isOk());
        EXPECT_TRUE(videoSink != nullptr);
    }
}

/**
* @brief Tests the retrieval and property validation of video sink instances.
*
* This test validates the retrieval of video sink instances and ensures that properties of these 
* instances remain consistent across multiple invocations. It checks the RESOURCE_ID property 
* to verify that it remains the same for the same instance ID.
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
* | 01 | Loop through each video sink ID from the profile | video sink ids from profile | All steps should execute for each ID | Should be successful |
* | 02 | Set the video sink ID | videoSinkId.value = id from profile | videoSinkId = _videoSinkId | Should be successful |
* | 03 | Retrieve the video sink instance for the given ID | videoSink | status.isOk() should be true, videoSink should not be nullptr | Should Pass |
* | 04 | Get the property of the video sink instance | Property = RESOURCE_ID, PropertyValue = valid reference for PropertyValue | status.isOk() should be true | Should Pass |
* | 05 | Retrieve the video sink instance again for the same ID | same sink id used in step 2 | status.isOk() should be true, videoSink_1 should not be nullptr | Should Pass |
* | 06 | Get the property of the second video sink instance | Property = RESOURCE_ID, PropertyValue = valid reference for PropertyValue | status.isOk() should be true | Should Pass |
* | 07 | Extract RESOURCE_ID from both property results | _resource_id_1, resource_id_2 | _resource_id_1 = value1, _resource_id_2 = value2 | Should be successful |
* | 08 | Verify that RESOURCE_ID values are equal | EXPECT_EQ(_resource_id_1, _resource_id_2) | _resource_id_1 should be equal to _resource_id_2 | Should Pass |
*/
TEST(L1VideoSinkManagerTest, test_l1_videosink_positive_2_getVideoSink)
{
    IVideoSink::Id videoSinkId;
    ::android::sp<::com::rdk::hal::videosink::IVideoSink> videoSink;
    ::android::sp<::com::rdk::hal::videosink::IVideoSink> videoSink_1;
    ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::RESOURCE_ID;
    std::optional<::com::rdk::hal::PropertyValue> resource_id_1;
    std::optional<::com::rdk::hal::PropertyValue> resource_id_2;

    for(const auto& _videoSinkId: _videosink_ids_from_profile)
    {
        videoSinkId.value = _videoSinkId;
        android::binder::Status status = service->getVideoSink(videoSinkId, &videoSink);
        EXPECT_TRUE(status.isOk());
        EXPECT_TRUE(videoSink != nullptr);

        status = videoSink->getProperty(property, &resource_id_1);
        EXPECT_TRUE(status.isOk());

        status = service->getVideoSink(videoSinkId, &videoSink_1);
        EXPECT_TRUE(status.isOk());
        EXPECT_TRUE(videoSink_1 != nullptr);

        status = videoSink_1->getProperty(property, &resource_id_2);
        EXPECT_TRUE(status.isOk());

        int32_t _resource_id_1 = resource_id_1->value->get<::com::rdk::hal::PropertyValue::Value::Tag::intValue>();
        int32_t _resource_id_2 = resource_id_2->value->get<::com::rdk::hal::PropertyValue::Value::Tag::intValue>();

        /* validating whether multiple invocation gives same videosink interface instance by checking RESOURCE_ID */
        EXPECT_EQ(_resource_id_1, _resource_id_2)<<"GOT DIFFERENT INSTANCE OF VIDEOSINK DURING MULTIPLE INVOCATION. ID for first invocation : "<<_resource_id_1 <<" ID for second invocation : "<<_resource_id_2 <<std::endl;
    }
}

/**
* @brief Test to ensure the getVideoSink method returns a valid video sink.
*
* This test checks whether the getVideoSink method of the service returns a valid
* video sink object for the default UNDEFINED video sink ID. It helps in verifying
* the correct functioning of the getVideoSink method for an uninitialized video sink ID.
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
* | Variation / Step | Description                                             | Test Data                                        | Expected Result                   |Notes                            |
* | :----:           | --------------------------------------------------------| -------------------------------------------------| ----------------------------------|----------------------------------|
* | 01               | Initialize videoSinkId with UNDEFINED value             | videoSinkId.value = IVideoSink::Id::UNDEFINED    | Should be successful              |                                 |
* | 02               | Declare a smart pointer to IVideoSink                   | ::android::sp<::com::rdk::hal::videosink::IVideoSink> videoSink | Should be successful              |                                 |
* | 03               | Call getVideoSink with the videoSinkId                  | service->getVideoSink(videoSinkId, &videoSink)   | Should Pass                       |                                 |
* | 04               | Assert the status returned from getVideoSink is OK      | status.isOk() should return true                 | Should be successful              |                                 |
* | 05               | Assert the videoSink is not nullptr                     | videoSink pointer shouldn't be null              | Should be successful              |                                 |
*/
TEST(L1VideoSinkManagerTest, test_l1_videosink_positive_3_getVideoSink)
{
    IVideoSink::Id videoSinkId ;
    videoSinkId.value = IVideoSink::Id::UNDEFINED;
    ::android::sp<::com::rdk::hal::videosink::IVideoSink> videoSink;
    ::android::binder::Status status = service->getVideoSink(videoSinkId, &videoSink);
    ASSERT_TRUE(status.isOk());
    ASSERT_NE(videoSink, nullptr);
}

/**
* @brief This test validates the behavior of the getVideoSink function in the negative scenario where an invalid video sink ID is provided.
*
* This test verifies that when an invalid video sink ID is given, the getVideoSink function correctly fails and returns a null video sink object.
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
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 |Initialize video sink ID with an invalid value | videoSinkId.value = -2 | sinkID should be set to -2 |Should be successful |
* | 02 |Initialize a smart pointer for IVideoSink | videoSink = nullptr initially | videoSink should be null initially |Should be successful |
* | 03 |Invoke getVideoSink with invalid video sink ID | videoSinkId = -2, videoSink = nullptr | getVideoSink should return status indicating failure |Should Fail |
* | 04 |Check that videoSink is null after function call | videoSink = nullptr | videoSink should remain null |Should pass |
*/
TEST(L1VideoSinkManagerTest, test_l1_videosink_negative_1_getVideoSink)
{
    IVideoSink::Id videoSinkId ;
    videoSinkId.value = -2;
    ::android::sp<::com::rdk::hal::videosink::IVideoSink> videoSink;
    ::android::binder::Status status = service->getVideoSink(videoSinkId, &videoSink);
    ASSERT_FALSE(status.isOk());
    ASSERT_EQ(videoSink, nullptr);
}

/**
* @brief This test validates if the getCapabilities method of L1VideoSink correctly fetches the capabilities
*
* The purpose of this test is to ensure that the getCapabilities function operates correctly and returns the expected values in the capabilities structure. This is crucial to verify that the video sink component can report its capabilities accurately.@n
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
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Initialize capabilities structure | capabilities initialized |capabilities |Should be successful |
* | 02| Call getCapabilities API | capabilities, status |status = success, capabilities filled |Should Pass |
* | 03| Verify the status | status | status.isOk() should return true |Should Pass |
*/
TEST(L1VideoSinkTest, test_l1_videosink_positive_1_getCapabilities)
{
    ::com::rdk::hal::videosink::Capabilities capabilities;
    ::android::binder::Status status = _videoSink->getCapabilities(&capabilities);
    ASSERT_TRUE(status.isOk());
}

/**
 * @brief Test for retrieving various property values from a video sink.
 *
 * This test verifies that properties can be successfully retrieved from a video sink,
 * and that the retrieved values are valid according to specific property validation logic.
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
 * | 01 | Initialize the test properties for the video sink | Property::RESOURCE_ID, Property::SINK_QUEUE_DEPTH, etc. | Properties are initialized successfully | Should be successful |
 * | 02 | Loop through each property to retrieve its value using getProperty | prop = Property::RESOURCE_ID, &propertyValue | Status should be OK | Should be successful |
 * | 03 | Validate the retrieved property value using helper_validatePropertyValue | propertyValue and prop | Returns true if property value is valid | Should Pass |
 */
TEST(L1VideoSinkTest, test_l1_videosink_positive_1_getProperty)
{
    std::vector<::com::rdk::hal::videosink::Property> properties = {
            Property::RESOURCE_ID,
            Property::SINK_QUEUE_DEPTH,
            Property::AV_SOURCE,
            Property::METRIC_FRAMES_RECEIVED,
            Property::METRIC_FRAMES_PRESENTED,
            Property::METRIC_FRAMES_DROPPED_LATE,
            Property::METRIC_FRAMES_DROPPED_FRC,
            Property::METRIC_FRAMES_REPEATED_FRC,
            Property::METRIC_FRAMES_REPEATED_MISSING_FRAME,
            Property::METRIC_UNDERFLOWED
    };
    for (Property prop : properties)
    {
        std::optional<PropertyValue> propertyValue;
        Status status = _videoSink->getProperty(prop, &propertyValue);
        ASSERT_TRUE(status.isOk());
        EXPECT_TRUE(helper_validatePropertyValue(propertyValue, prop));
    }
}

/**
* @brief Test the L1 VideoSink's getProperty method with an invalid property.
*
* This test verifies the behavior of the L1 VideoSink's getProperty method when it is called with an invalid property value. 
* This helps ensure that the method handles invalid input correctly and does not crash or return incorrect values.
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
* | :--------------: | ----------- | --------- | --------------- | ----- |
* | 01               | Initialize the Optional PropertyValue variable | None | Should be successful | None |
* | 02               | Cast an invalid value to Property | invalid_property = static_cast<::com::rdk::hal::videosink::Property>(-1) | Should be successful | None |
* | 03               | Call the getProperty method with the invalid property | invalid_property = -1, propertyValue = std::optional<::com::rdk::hal::PropertyValue> | status.isOk() should return false | Should fail |
* | 04               | Assert that the method status is not Ok | status.isOk() | False | Should Pass |
* | 05               | Assert that the propertyValue does not have a value | propertyValue should be null | False | Should Pass |
*/
TEST(L1VideoSinkTest, test_l1_videosink_negative_1_getProperty)
{
    std::optional<::com::rdk::hal::PropertyValue> propertyValue;
    ::com::rdk::hal::videosink::Property invalid_property = static_cast<::com::rdk::hal::videosink::Property>(-1);
    ::android::binder::Status status = _videoSink->getProperty(invalid_property, &propertyValue);
    ASSERT_FALSE(status.isOk());
    ASSERT_FALSE(propertyValue.has_value());
}

/**
* @brief This test verifies the functionality of setting the AV_SOURCE property of the video sink in various scenarios.
*
* The test checks if the _videoSink interface can successfully change the AV_SOURCE property to different enum values and revert to the initial value as part of the clean-up process.
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
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Initialize the property variable with AV_SOURCE and declare optional and boolean for storing values. | property = AV_SOURCE, initialvalue, _aidl_return | Variables initialized successfully. | Should be successful |
* | 02| Call the getProperty function of _videoSink to store the initial value of the AV_SOURCE. | property=AV_SOURCE, initialvalue | status.isOk() should return true, initial value of AV_SOURCE should be stored. | Should Pass |
* | 03| Iterate through all AV_SOURCE enum values and set the property using setProperty function of _videoSink. | enum values from AVSource, property=AV_SOURCE | status.isOk() should return true, property should be set successfully, _aidl_return should be true. | Should Pass |
* | 04| Call the setProperty function with initial value to revert back to original state. | property=AV_SOURCE, initialvalue | status.isOk() should return true, property should be reverted successfully. | Should Pass |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_positive_1_setProperty) 
{
   ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::AV_SOURCE;
    std::optional<PropertyValue> initialvalue;
    bool _aidl_return;

    /* storing initial value */
    Status status = _videoSink->getProperty(property, &initialvalue);
    ASSERT_TRUE(status.isOk()) << "getProperty failed for property: AV_SOURCE";

    for (auto avSource :
    {
        ::com::rdk::hal::AVSource::AUTO,
        ::com::rdk::hal::AVSource::UNKNOWN,
        ::com::rdk::hal::AVSource::IP,
        ::com::rdk::hal::AVSource::TUNER,
        ::com::rdk::hal::AVSource::SYSTEM,
        ::com::rdk::hal::AVSource::HDMI_1,
        ::com::rdk::hal::AVSource::HDMI_2,
        ::com::rdk::hal::AVSource::HDMI_3,
        ::com::rdk::hal::AVSource::HDMI_4,
        ::com::rdk::hal::AVSource::HDMI_5,
        ::com::rdk::hal::AVSource::COMPOSITE_1,
        ::com::rdk::hal::AVSource::COMPOSITE_2,
        ::com::rdk::hal::AVSource::COMPOSITE_3,
        ::com::rdk::hal::AVSource::COMPOSITE_4,
        ::com::rdk::hal::AVSource::COMPOSITE_5
    }) {
            PropertyValue propertyValue;
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(int32_t(static_cast<int32_t>(avSource)));
            status = videoSinkController->setProperty(property, propertyValue, &_aidl_return);
            ASSERT_TRUE(status.isOk());
            ASSERT_TRUE(_aidl_return);
    }
    
    /*clean-up setting back initial value*/
    status = videoSinkController->setProperty(property, initialvalue.value(), &_aidl_return);
    ASSERT_TRUE(status.isOk()) << "Failed to revert value for property : AV_SOURCE";
}

/**
* @brief This test case verifies the behavior of the `setProperty` function when it is provided with RESOURCE_ID
*
* This test case sets a property of a video sink component using an RESOURCE_ID to check that the function handles invalid inputs as expected by failing accordingly
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
* | 01| Assign resource ID property to variable | property = PROPERTY::RESOURCE_ID | Property assigned successfully | Should be successful |
* | 02| Set invalid property value | propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(1) | Invalid property value set | Should be successful |
* | 03| Invoke setProperty method with invalid property |  property = RESOURCE_ID, propertyValue = 1 |_aidl_return = false, status.isOk() = true | Should fail |
* | 04| Check that status is  okay | status.isOk() | status.isOk() should be true | Should Pass |
* | 05| Check that _aidl_return is false | _aidl_return | _aidl_return should be false | Should Pass |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_1_setProperty) 
{
   ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::RESOURCE_ID;
    PropertyValue propertyValue;
    propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(1);
    bool _aidl_return;
    android::binder::Status status = videoSinkController->setProperty(property, propertyValue, &_aidl_return);
    EXPECT_TRUE(status.isOk());
    EXPECT_FALSE(_aidl_return);
}

/**
* @brief Test to verify negative scenario for setting property in L1VideoSink
*
* The test checks the behavior when setting an invalid property value 
* and ensures that the API correctly fails and returns the expected error state.
*
* **Test Group ID:** Basic: 01 @n
* **Test Case ID:** 011 @n
* **Priority:** High @n
* @n
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** None
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Initialize the property variable to SINK_QUEUE_DEPTH | property = ::com::rdk::hal::videosink::Property::SINK_QUEUE_DEPTH |Should be successful |  |
* | 02 | Create a PropertyValue object and assign an invalid int value of -5 | propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(-5) | Should be successful |  |
* | 03 | Call the setProperty method on the _videoSink object with the invalid property value | property = SINK_QUEUE_DEPTH, propertyValue = -5, _aidl_return = false | API should fail, _aidl_return should be false | Should Fail |
* | 04 | Verify the status returned by setProperty method is  OK |status.isOk()  | status.isOk() = true | Should be successful |
* | 05 | Verify the _aidl_return value is false as expected | _aidl_return | _aidl_return = false | Should be successful |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_2_setProperty) 
{
   ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::SINK_QUEUE_DEPTH;
    PropertyValue propertyValue;
    propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(-5);
    bool _aidl_return;
    android::binder::Status status = videoSinkController->setProperty(property, propertyValue, &_aidl_return);
    EXPECT_TRUE(status.isOk());
    EXPECT_FALSE(_aidl_return);
}

/**
* @brief Test the negative scenario of setting a property with an invalid value
*
* This test validates the behavior of the VideoSink when an invalid property value is set, ensuring that the property setting fails as expected@n
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
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Create Property to set `METRIC_FRAMES_RECEIVED` | property = METRIC_FRAMES_RECEIVED | Should be successful| |
* | 02| Set invalid value -5 to PropertyValue | propertyValue.value = -5 | Should be successful| |
* | 03| Call setProperty with invalid `propertyValue` | property, propertyValue, _aidl_return | Should Fail| |
* | 04| Check if the status is  OK | status.isOk() | status.isOk() = true | Should pass |
* | 05| Confirm that `_aidl_return` is false | _aidl_return |  _aidl_return = false | Should pass |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_3_setProperty) 
{
   ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::METRIC_FRAMES_RECEIVED;
    PropertyValue propertyValue;
    propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(-5);
    bool _aidl_return;
    android::binder::Status status = videoSinkController->setProperty(property, propertyValue, &_aidl_return);
    EXPECT_TRUE(status.isOk());
    EXPECT_FALSE(_aidl_return);
}

/**
* @brief Tests the negative behavior of setting a video sink property to an invalid value
*
* This test verifies that attempting to set an invalid value (-5) for a video sink property (METRIC_FRAMES_PRESENTED)
* fails as expected. This ensures the robustness and error handling capability of the setProperty function.
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
* | 01 | Create a Property with type METRIC_FRAMES_PRESENTED | property = Property::METRIC_FRAMES_PRESENTED | Property created with specified type | Should be successful |
* | 02 | Create and assign invalid property value (-5) | propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(-5) | PropertyValue assigned an invalid value | Should be successful |
* | 03 | Call setProperty with the Property and invalid PropertyValue | property = Property::METRIC_FRAMES_PRESENTED, propertyValue = -5 | setProperty should fail | Should Fail |
* | 04 | Verify that status.isOk() is true | status.isOk() | status.isOk() should be true | Should pass |
* | 05 | Verify that _aidl_return is false | _aidl_return | _aidl_return should be false | Should pass |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_4_setProperty) 
{
   ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::METRIC_FRAMES_PRESENTED;
    PropertyValue propertyValue;
    propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(-5);
    bool _aidl_return;
    android::binder::Status status = videoSinkController->setProperty(property, propertyValue, &_aidl_return);
    EXPECT_TRUE(status.isOk());
    EXPECT_FALSE(_aidl_return);
}

/**
* @brief This test case is designed to validate the `setProperty` method's handling of negative values.
*
* The test checks that the `setProperty` method correctly rejects invalid property values when a negative value is used. This helps in ensuring the robustness and correctness of the input validation in the method.
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
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Set the `property` variable to `Property::METRIC_FRAMES_DROPPED_LATE` | property = `METRIC_FRAMES_DROPPED_LATE` | Should be successful |  |
* | 02| Initialize `propertyValue` with `intValue` set to -5 | propertyValue.value = -5 | Should be successful |  |
* | 03| Call the `setProperty` method with the `property` and `propertyValue`, and store the result in `_aidl_return` | property = `METRIC_FRAMES_DROPPED_LATE`, propertyValue.value = -5 | Should fail | |
* | 04| Verify that the `status.isOk()` returns true | status.isOk() | status.isOk() Should be true | Should Pass |
* | 05| Verify that `_aidl_return` is false | _aidl_return | _aidl_return Should be false | Should Pass |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_5_setProperty) 
{
   ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::METRIC_FRAMES_DROPPED_LATE;
    PropertyValue propertyValue;
    propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(-5);
    bool _aidl_return;
    android::binder::Status status = videoSinkController->setProperty(property, propertyValue, &_aidl_return);
    EXPECT_TRUE(status.isOk());
    EXPECT_FALSE(_aidl_return);
}

/**
* @brief This test checks the behavior of the setProperty method when an invalid value is passed.
*
* This test validates that the setProperty method of the L1VideoSink interface correctly handles invalid input by setting a negative integer value to METRIC_FRAMES_DROPPED_FRC property. The test ensures that the operation fails as expected for invalid inputs.
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
* | :----: | --------- | ---------- | -------------- | ----- |
* | 01 | Set the property to METRIC_FRAMES_DROPPED_FRC | property = METRIC_FRAMES_DROPPED_FRC | Property is set | Should be successful |
* | 02 | Assign a negative integer value to the propertyValue | propertyValue.value = -5 | Value assigned | Should be successful |
* | 03 | Call the setProperty method with the specified property and invalid propertyValue | property = METRIC_FRAMES_DROPPED_FRC, propertyValue.value = -5 | status.isOk() = true, _aidl_return = false | Should Fail |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_6_setProperty) 
{
   ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::METRIC_FRAMES_DROPPED_FRC;
    PropertyValue propertyValue;
    propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(-5);
    bool _aidl_return;
    android::binder::Status status = videoSinkController->setProperty(property, propertyValue, &_aidl_return);
    EXPECT_TRUE(status.isOk());
    EXPECT_FALSE(_aidl_return);
}

/**
* @brief This test case is intended to test the handling of negative values being set for a property in L1 video sink.
*
* The objective is to verify the behaviour of the `setProperty` method of the `L1VideoSink` when a negative integer value is provided for METRIC_FRAMES_REPEATED_FRC. The method should return an error and flag that the operation was unsuccessful.
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
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Initialize the Property object with `METRIC_FRAMES_REPEATED_FRC` | property = METRIC_FRAMES_REPEATED_FRC | Property should be initialized successfully | Should be successful |
* | 02| Set the PropertyValue to contain a negative integer value | propertyValue = -5 | PropertyValue should be set with a negative integer value | Should be successful |
* | 03| Call the `setProperty` method with initialized Property and PropertyValue objects | property = METRIC_FRAMES_REPEATED_FRC, propertyValue = -5 | Method should return false and indicate that the operation failed | Should Fail |
* | 04| Verify that the status is  OK | status.isOk() | status.isOk() = true | Should pass |
* | 05| Verify that `_aidl_return` is false | _aidl_return | _aidl_return = false | Should pass |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_7_setProperty) 
{
   ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::METRIC_FRAMES_REPEATED_FRC;
    PropertyValue propertyValue;
    propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(-5);
    bool _aidl_return;
    android::binder::Status status = videoSinkController->setProperty(property, propertyValue, &_aidl_return);
    EXPECT_TRUE(status.isOk());
    EXPECT_FALSE(_aidl_return);
}

/**
* @brief Tests the behavior of setting a property with a negative value.
*
* This test validates the `setProperty` method of the L1 VideoSink by attempting to set the property METRIC_FRAMES_REPEATED_MISSING_FRAME with an invalid negative value. The objective is to ensure that the method correctly handles improper input and returns appropriate error statuses.
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
* | :----: | --------- | ---------- | -------------- | ----- |
* | 01 | Initialize the `property` variable with the `METRIC_FRAMES_REPEATED_MISSING_FRAME` property. | property = METRIC_FRAMES_REPEATED_MISSING_FRAME | Should initialize successfully | Should be successful |
* | 02 | Set the `propertyValue` variable's `intValue` to -5. | propertyValue = -5 | Should set successfully | Should be successful |
* | 03 | Call the `setProperty` method with negative property value and check the status. | property = METRIC_FRAMES_REPEATED_MISSING_FRAME, propertyValue = -5 | status.isOk() should be true | Should Fail |
* | 04 | Verify the value of `_aidl_return`. | _aidl_return | Should be false | Should pass |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_8_setProperty) 
{
   ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME;
    PropertyValue propertyValue;
    propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(-5);
    bool _aidl_return;
    android::binder::Status status = videoSinkController->setProperty(property, propertyValue, &_aidl_return);
    EXPECT_TRUE(status.isOk());
    EXPECT_FALSE(_aidl_return);
}

/**
* @brief Test the negative scenario of setting a property with an invalid value.
*
* This test evaluates the `setProperty` method of the `videosink` with an invalid value for METRIC_UNDERFLOWED to ensure that it handles error conditions properly.
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
* | :----: | --------- | ---------- |--------------| ----- |
* | 01 | Choose the property to be set |property = ::com::rdk::hal::videosink::Property::METRIC_UNDERFLOWED | Should be successful |  |
* | 02 | Set the property value to an invalid integer value | propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(-5) | Should be successful | |
* | 03 | Call the `setProperty` method with the property and invalid value | property = METRIC_UNDERFLOWED, propertyValue = -5 | status.isOk() = true, _aidl_return = false | Should Fail |
* | 04 | Assert that the status returned is  OK | EXPECT_TRUE(status.isOk()) | status.isOk() = true | Should pass |
* | 05 | Assert that the aidl return value is false | EXPECT_FALSE(_aidl_return) | _aidl_return = false | Should pass |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_9_setProperty) 
{
   ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::METRIC_UNDERFLOWED;
    PropertyValue propertyValue;
    propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(-5);
    bool _aidl_return;
    android::binder::Status status = videoSinkController->setProperty(property, propertyValue, &_aidl_return);
    EXPECT_TRUE(status.isOk());
    EXPECT_FALSE(_aidl_return);
}

/**
* @brief Test the handling of a negative integer value for the AV_SOURCE property.
*
* This test checks the behavior of the `setProperty` function when attempting to set an invalid negative integer value for the AV_SOURCE property. This ensures the function correctly handles invalid inputs.
*
* **Test Group ID:** Basic: 01 @n
* **Test Case ID:** 019 @n
* **Priority:** High @n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Initialize the property | property = AV_SOURCE, propertyValue.value = -5 | property and propertyValue initialized correctly | Should be successful |
* | 02| Call setProperty with initialized values | property = ::com::rdk::hal::videosink::Property::AV_SOURCE, propertyValue.value = -5 | status return and _aidl_return captured | Should Fail |
* | 03| Verify the status and return | status.isOk(), _aidl_return | status.isOk() is false and _aidl_return is false | Should pass |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_10_setProperty) 
{
    ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::AV_SOURCE;
    PropertyValue propertyValue;
    propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(-5);
    bool _aidl_return;
    android::binder::Status status = videoSinkController->setProperty(property, propertyValue, &_aidl_return);
    ASSERT_FALSE(status.isOk());
    ASSERT_FALSE(_aidl_return);
}

/**
* @brief Test the positive scenario of the getState method in the L1VideoSink module.
*
* This test aims to verify that the `getState` method in the L1VideoSink module functions correctly by returning one of the expected states. 
* This is crucial for determining if the video sink is in a valid state and ready for subsequent operations.
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
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01    | Instantiate the state variable | state | Variable should be instantiated  | Should be successful |
* | 02    | Call the getState method of videoSink instance | _videoSink->getState(&state) |status.isOk() should return true | Should Pass |
* | 03    | Verify if the returned state is one of the expected states | State values: UNKNOWN, CLOSED, OPENING, READY, STARTING, STARTED, FLUSHING, STOPPING, CLOSING | State should be one of the expected values | Should Pass |
*/
TEST(L1VideoSinkTest, test_l1_videosink_positive_1_getState) 
{
    ::com::rdk::hal::State state;
    ::android::binder::Status status = _videoSink->getState(&state);
    ASSERT_TRUE(status.isOk());
    ASSERT_TRUE(state == ::com::rdk::hal::State::UNKNOWN ||
                state == ::com::rdk::hal::State::CLOSED  ||
                state == ::com::rdk::hal::State::OPENING ||
                state == ::com::rdk::hal::State::READY   ||
                state == ::com::rdk::hal::State::STARTING||
                state == ::com::rdk::hal::State::STARTED ||
                state == ::com::rdk::hal::State::FLUSHING||
                state == ::com::rdk::hal::State::STOPPING||
                state == ::com::rdk::hal::State::CLOSING);
}

/**
* @brief Test opening and closing the video sink in a positive scenario
*
* This test verifies that the video sink can be opened and closed successfully 
* by checking the initial state, opening the sink, and then closing it. This is 
* crucial for ensuring that the video component functions correctly in the typical 
* usage scenarios.
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
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Verify initial state of the video sink | state  | state = CLOSED | Should be successful |
* | 02 | Create video sink controller listener | listener = valid listener | listener != nullptr | Should be successful |
* | 03 | Open video sink with listener and obtain controller | status.isOk() , videoSinkController | status.isOk() = true, videoSinkController != nullptr | Should pass |
* | 04 | Close video sink with controller | status.isOk(), _aidl_return | status.isOk() = true, _aidl_return = true | Should pass |
*/
TEST(L1VideoSinkTest, test_l1_videosink_positive_1_open) 
{
        //prerequisite - checking state
    ::com::rdk::hal::State state;
    ::android::binder::Status status = _videoSink->getState(&state);
    ASSERT_TRUE(status.isOk());
    ASSERT_EQ(state, ::com::rdk::hal::State::CLOSED);

    android::sp<IVideoSinkController> videoSinkController;
    sp<IVideoSinkControllerListener> listener = new VideoSinkControllerListener();
    status = _videoSink->open(listener, &videoSinkController);
    ASSERT_TRUE(status.isOk());
    ASSERT_NE(videoSinkController, nullptr);

    //cleaning up
    bool _aidl_return;
    status = _videoSink->close(videoSinkController, &_aidl_return);
    ASSERT_TRUE(status.isOk());
    ASSERT_TRUE(_aidl_return);
}

/**
* @brief Test for video sink open functionality in an illegal state
*
* This test verifies that the video sink open functionality correctly throws an EX_ILLEGAL_STATE
* exception when attempting to open the video sink from the READY state.
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
* | 01 | Check initial state | N/A |State = CLOSED |Done by Pre-requisite SetUp function |
* | 02 | Open the video sink and obtain controller | N/A |videoSinkController != nullptr |Done by Pre-requisite SetUp function |
* | 03 | Check state after open | N/A |State = READY |Done by Pre-requisite SetUp function |
* | 04 | Call the open() function on _videoSink from READY state and capture status | listener, aidl_return | status.exceptionCode() == android::binder::Status::EX_ILLEGAL_STATE | Should Fail |
* | 05 | Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 06 | Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_1_open) 
{
    android::sp<IVideoSinkController> aidl_return;
    sp<IVideoSinkControllerListener> listener = new VideoSinkControllerListener();
    //calling open() from READY state (EX_ILLEGAL_STATE)
    ::android::binder::Status status = _videoSink->open(listener, &aidl_return);
    ASSERT_EQ(status.exceptionCode(), android::binder::Status::EX_ILLEGAL_STATE);
}

/**
* @brief This test aims to verify the correct functionality of closing a video sink.
*
* The test ensures that the `close` function of the video sink works properly by first opening the video sink and 
* then attempting to close it. It verifies that the sink transitions to the correct state and returns the expected result.
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
* | Variation / Step | Description                                   | Test Data                                                   | Expected Result                      | Notes                           |
* | :--------------: | --------------------------------------------- | ----------------------------------------------------------- | ------------------------------------ | -------------------------------- |
* | 01               | Invoke SetUp to initialize the state          | N/A                                                         | State should be `CLOSED`             | Done by Pre-requisite SetUp function |
* | 02               | Open videoSink                                | listener = new VideoSinkControllerListener()                | videoSinkController should not be null | Should be successful             |
* | 03               | Check state to be READY after open            | N/A                                                         | State should be `READY`              | Should be successful             |
* | 04               | Ensure _videoSink is not null                 | _videoSink should be initialized                            | Should proceed with test             | Should be successful             |
* | 05               | Ensure videoSinkController is not null        | videoSinkController should be initialized                   | Should proceed with test             | Should be successful             |
* | 06               | Call close on _videoSink                      | videoSinkController, output: _aidl_return                   | Status should be OK and return true  | Should Pass                      |
* | 07               | Assert the close operation was successful     | _aidl_return should be true                                 | Should be true                       | Should Pass                      |
*/
TEST_F(L1VideoSinkCloseTest, test_l1_videosink_positive_1_close) 
{
    ASSERT_NE(_videoSink, nullptr) << "videoSink is null, cannot proceed with test.";
    ASSERT_NE(videoSinkController, nullptr) << "videoSinkController is null, cannot proceed with test.";
    bool _aidl_return;
    android::binder::Status status = _videoSink->close(videoSinkController, &_aidl_return);
    ASSERT_TRUE(status.isOk());
    ASSERT_TRUE(_aidl_return);
}

/**
* @brief Negative test case for closing the video sink while in STARTED state
*
* This test verifies the behavior of the close() function when the video sink is in the STARTED state. The close() function is expected to fail with an ILLEGAL_STATE exception because the video sink is not in an appropriate state for closing.
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
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Check initial state | | state = CLOSED |Should be successful |
* | 02| Open the video sink and assign videoSinkController| listener, videoSinkController | videoSinkController != null |Should Pass |
* | 03| Check the state after opening | state | state = READY |Should be successful |
* | 04| start  the videoSinkController | |status.isOk() should return true |Should be successful |
* | 05| Attempt to close the video sink from STARTED state| Started state | status.isok() = true, aidl_return = false |Should Fail |
* | 06| Clean up by stopping the video sink | | status.isOk() should return true |Should be successful |
* | 07| Close the video sink after stopping| |status.isOk() should return true | Should be successful |
*/
TEST_F(L1VideoSinkCloseTest, test_l1_videosink_negative_1_close) 
{
    bool aidl_return;
    //changing state to STARTED
    Status status = videoSinkController->start();
    ASSERT_TRUE(status.isOk());
    //calling close() from STARTED state (EX_ILLEGAL_STATE)
    status = _videoSink->close(videoSinkController, &aidl_return);
    ASSERT_TRUE(status.isOk());
    EXPECT_FALSE(aidl_return);
    //clean up
    status = videoSinkController->stop();
    ASSERT_TRUE(status.isOk());
    status = _videoSink->close(videoSinkController, &aidl_return);
    ASSERT_TRUE(status.isOk());
    ASSERT_TRUE(aidl_return);
}

/**
* @brief Test the registration of an event listener with the video sink.
*
* This test ensures that the function `registerEventListener` properly registers an event listener 
* for the video sink object and returns successful status and result. It also includes the 
* cleanup step to unregister the event listener to maintain a clean state.
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
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Create a new event listener | videoSinkEventListener | videoSinkEventListener object should be created successfully | Should be successful |
* | 02 | Register the event listener with the video sink | _aidl_return |  _aidl_return = true | Should Pass |
* | 03 | Check if the status after registration is OK | status.isOk() | status.isOk() should be true | Should Pass |
* | 04 | Check if the event listener is registered successfully | _aidl_return | _aidl_return should be true  | Should Pass |
* | 05 | Unregister the event listener | videoSinkEventListener | input: videoSinkEventListener, _aidl_return; output: _aidl_return = true | Should Pass |
* | 06 | Check if the status after unregistration is OK | status.isOk() | input: status; output: true | Should Pass |
*/
TEST(L1VideoSinkTest, test_l1_videosink_positive_1_registerEventListener) 
{
    android::sp<IVideoSinkEventListener> videoSinkEventListener = new VideoSinkEventListener();
    bool _aidl_return;
    ::android::binder::Status status = _videoSink->registerEventListener(videoSinkEventListener, &_aidl_return);
    ASSERT_TRUE(status.isOk());
    ASSERT_TRUE(_aidl_return);
    //clean up
    status = _videoSink->unregisterEventListener(videoSinkEventListener, &_aidl_return);
    ASSERT_TRUE(status.isOk());
    ASSERT_TRUE(_aidl_return);
}

/**
* @brief Test the behaviour of registering an already registered VideoSinkEventListener for a video sink.
*
* This test checks the behavior of the API when attempting to register an already registered event listener.@n
* It ensures that the system does not allow duplicate listener registrations.@n
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
* | 01| Setup the test environment by creating a new VideoSinkEventListener instance and registering it to the video sink.| | Should be successful | Done by Pre-requisite SetUp function |
* | 02| Try to register the already registered eventlistener again.| | status.isOk() is true  and _aidl_return as false.  |Should Fail |
* | 03| Unregister the existing event listener from the video sink.| | status.isOk() and _aidl_return as true. | Should Pass |
*/
TEST_F(L1VideoSinkEventRegisterTest, test_l1_videosink_negative_1_registerEventListener) 
{
  bool _aidl_return;
  /* trying to register eventlistener which is allready registered in the setup*/
  Status status = _videoSink->registerEventListener(videoSinkEventListener, &_aidl_return);
  EXPECT_TRUE(status.isOk());
  EXPECT_FALSE(_aidl_return);

  status = _videoSink->unregisterEventListener(videoSinkEventListener, &_aidl_return);
  ASSERT_TRUE(status.isOk()); 
  ASSERT_TRUE(_aidl_return); 
}

/**
 * @brief Test the unregisterEventListener function in a positive scenario
 *
 * This test verifies that the `unregisterEventListener` function works correctly by 
 * initially registering an event listener and then unregistering it. It ensures that 
 * the listener registration is successful and subsequently verifies that it can be 
 * unregistered successfully.
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
 * | Variation / Step | Description                                       | Test Data                                          | Expected Result                         | Notes                         |
 * | :----:           | ---------                                         | ----------                                        |--------------                           | -----                         |
 * | 01               | Setup by initializing a new `VideoSinkEventListener` object and register it to `_videoSink`. | None, Registers videoSinkEventListener with _videoSink          | Registration should succeed            | Done by Pre-requisite SetUp function   |
 * | 02               | Unregister the `videoSinkEventListener` from `_videoSink`.  | videoSinkEventListener = <pointer>               | Status should be Ok, _aidl_return should be true | Should Pass                  |
 */
TEST_F(L1VideoSinkEventRegisterTest, test_l1_videosink_positive_1_unregisterEventListener)
{
    bool _aidl_return;
    android::binder::Status status = _videoSink->unregisterEventListener(videoSinkEventListener, &_aidl_return);
    EXPECT_TRUE(status.isOk());
    EXPECT_TRUE(_aidl_return);
}

/**
* @brief Unregisters an event listener from the video sink in a negative test scenario.
*
* This test attempts to unregister an event listener which was not registered and verifies that the operation fails as expected.@n
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
* | Variation / Step | Description | Test Data |Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Instantiate a new VideoSinkEventListener | listener = new VideoSinkEventListener() | A new VideoSinkEventListener instance should be created | Should be successful |
* | 02 | Try to unregister the event listener from the video sink | listener, &_aidl_return | status.isok() = true , _aidl_return = false | Should Fail |
* | 03 | Assert that the status is  OK | status.isOk() | status.isOk() = true | Should be successful |
* | 04 | Assert that the _aidl_return is false | _aidl_return | _aidl_return = false | Should be successful |
*/
TEST(L1VideoSinkTest, test_l1_videosink_negative_1_unregisterEventListener)
{
    sp<IVideoSinkEventListener> listener = new VideoSinkEventListener();
    bool _aidl_return;
    android::binder::Status status = _videoSink->unregisterEventListener(listener, &_aidl_return);
    ASSERT_TRUE(status.isOk());
    ASSERT_FALSE(_aidl_return);
}

/**
* @brief Test for verifying error handling when attempting to unregister an already unregistered event listener.
*
* This test validates that the system correctly handles the case where an attempt is made to unregister a video sink event listener twice. 
* It ensures the error handling mechanisms in place are effective and robust.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 029@n
* **Priority:** High@n
*
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
*
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Setup the test environment by creating and registering video sink event listener using the SetUp function | None | The video sink event listener should be registered successfully | Done by Pre-requisite SetUp function |
* | 02| Unregister the event listener that was registered in the setup function | videoSinkEventListener = valid instance | The system should successfully unregister the listener | Should Pass |
* | 03| Attempt to unregister the already unregistered listener | videoSinkEventListener = invalid instance | The system should fail to unregister the listener and return an error | Should Fail |
*/
TEST_F(L1VideoSinkEventRegisterTest, test_l1_videosink_negative_2_unregisterEventListener)
{
    // Unregister the listener, which was registered by setup function
    bool _aidl_return;
    Status status = _videoSink->unregisterEventListener(videoSinkEventListener, &_aidl_return);
    ASSERT_TRUE(status.isOk());
    ASSERT_TRUE(_aidl_return);

    // Try to unregister again
    status = _videoSink->unregisterEventListener(videoSinkEventListener, &_aidl_return);
    ASSERT_TRUE(status.isOk());
    ASSERT_FALSE(_aidl_return);
}

/**
* @brief Test to verify the positive scenario of setting the video decoder in the VideoSink.
*
* This test validates the functionality of the setVideoDecoder method in IVideoSinkController. It ensures that the method can set the video Decoder successfully when provided with a valid decoder ID.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 030@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Check initial state | N/A |State = CLOSED |Done by Pre-requisite SetUp function |
* | 02 | Open the video sink and obtain controller | N/A |videoSinkController != nullptr |Done by Pre-requisite SetUp function |
* | 03 | Check state after open | N/A |State = READY |Done by Pre-requisite SetUp function |
* | 02 | Verify that videoSink and videoSinkController are not null.| None |Non-null videoSink and videoSinkController| Should be successful |
* | 03 | Set video decoder with ID UNDEFINED and verify the operation.| videoDecoderId.value = UNDEFINED, _aidl_return = true | _aidl_return should be true, status should be Ok | Should Pass |
* | 05 | Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 06 | Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_positive_1_setVideoDecoder) 
{

    ASSERT_NE(_videoSink, nullptr) << "videoSink is null, cannot proceed with test.";
    ASSERT_NE(videoSinkController, nullptr) << "videoSinkController is null, cannot proceed with test.";

    bool _aidl_return;
    ::com::rdk::hal::videodecoder::IVideoDecoder::Id videoDecoderId;
    videoDecoderId.value = ::com::rdk::hal::videodecoder::IVideoDecoder::Id::UNDEFINED;

    Status status = videoSinkController->setVideoDecoder(videoDecoderId, &_aidl_return);
    EXPECT_TRUE(_aidl_return);
    EXPECT_TRUE(status.isOk());
}

/**
* @brief This test checks the setVideoDecoder function of the IVideoSinkController.
*
* This test verifies that the setVideoDecoder method functionally works as expected with various valid video decoder IDs. It ensures the method call returns successfully and the correct values.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 031@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- | -------------- | ----- |
* | 01 | Prerequisite SetUp function to check initial state and open videoSink | listener = new VideoSinkControllerListener() | state = READY, videoSinkController initialized | Done by Pre-requisite SetUp function |
* | 02 | Iterate through _videodecoder_ids_from_profile and set each as videoDecoderId |  videoDecoderId.value = _id | _aidl_return = true, status = true | Should Pass |
* | 03 | Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 04 | Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_positive_2_setVideoDecoder) 
{
    bool _aidl_return;
    ::com::rdk::hal::videodecoder::IVideoDecoder::Id videoDecoderId;

    for(const auto& _id: _videodecoder_ids_from_profile)
    {
        videoDecoderId.value = _id;
        Status status = videoSinkController->setVideoDecoder(videoDecoderId, &_aidl_return);
        EXPECT_TRUE(_aidl_return);
        EXPECT_TRUE(status.isOk());
    }
}

/**
* @brief Test for setting an invalid video decoder ID to the video sink controller
*
* This test aims to verify the behavior of the video sink controller when an invalid video decoder ID is set. The test checks if the function correctly handles invalid input parameters by returning a failure status.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 032@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |--------------| ----- |
* | 01 | Check initial state | N/A |State = CLOSED |Done by Pre-requisite SetUp function |
* | 02 | Open the video sink and obtain controller | N/A |videoSinkController != nullptr |Done by Pre-requisite SetUp function |
* | 03 | Check state after open | N/A |State = READY |Done by Pre-requisite SetUp function |
* | 04 | Set invalid video decoder ID | videoDecoderId = -2 | _aidl_return = false, status.isOk() = false | Should Fail |
* | 05 | Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 06 | Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_1_setVideoDecoder) 
{
    bool _aidl_return;
    ::com::rdk::hal::videodecoder::IVideoDecoder::Id videoDecoderId;
    videoDecoderId.value = -2; // Invalid ID

    Status status = videoSinkController->setVideoDecoder(videoDecoderId, &_aidl_return);
    EXPECT_FALSE(_aidl_return);
    EXPECT_FALSE(status.isOk()); // Assuming non-Ok status for invalid ID
}

/**
* @brief This test case validates the behavior of the `setVideoDecoder` method for negative scenarios
*
* This test verifies that the `setVideoDecoder` method returns an ILLEGAL_STATE exception code when the video sink is in an invalid state for setting a video decoder. This test ensures robustness of the `setVideoDecoder` method by checking its response to improper state transitions.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 033@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Pre-requisite SetUp function from L1VideoSinkOpenTest::SetUp | | | Done by Pre-requisite SetUp function |
* | 02 | Check initial state | N/A |State = CLOSED |Done by Pre-requisite SetUp function |
* | 03 | Open the video sink and obtain controller | N/A |videoSinkController != nullptr |Done by Pre-requisite SetUp function |
* | 04 | Check state after open | N/A |State = READY |Done by Pre-requisite SetUp function |
* | 05 | Pre-requisite SetUp function in L1VideoSinkStartTest::SetUp | | | Done by Pre-requisite SetUp function |
* | 06 | Starting the video sink.| N/A  | The state should be STARTED, and the method should execute without errors. | Should be successful |
* | 07 | Checking the state after starting the video sink.  |  N/A  | State should be STARTED.  | Should be successful |
* | 08 | Initialize `videoDecoderId` with first  ID  from profile | videoDecoderId.value = _videodecoder_ids_from_profile[0] | videoDecoderId contains a valid ID | Should be successful |
* | 09 | Try setting the video decoder with `videoDecoderId` |  videoDecoderId.value = _videodecoder_ids_from_profile[0] | status.exceptionCode() = ::android::binder::Status::EX_ILLEGAL_STATE | Should Fail |
* | 10 | Pre-requisite TearDown function from L1VideoSinkStartTest::TearDown | | | Done by Pre-requisite TearDown function |
* | 11 | Stopping the video sink. | N/A | The state should change to READY | Done by Pre-requisite TearDown function |
* | 12 | Pre-requisite TearDown function in L1VideoSinkOpenTest::TearDown | | | Done by Pre-requisite TearDown function |
* | 13 | Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 14 | Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkStartTest, test_l1_videosink_negative_2_setVideoDecoder) 
{
    bool _aidl_return;
    ::com::rdk::hal::videodecoder::IVideoDecoder::Id videoDecoderId;
    ASSERT_TRUE(_videodecoder_ids_from_profile.size()) <<"No Videodecoder is available";

    videoDecoderId.value = _videodecoder_ids_from_profile[0];
    Status status = videoSinkController->setVideoDecoder(videoDecoderId, &_aidl_return);
    EXPECT_EQ(status.exceptionCode(), ::android::binder::Status::EX_ILLEGAL_STATE);
}

/**
* @brief Test case for checking the video decoder retrieval when video sink is in READY state
*
* This test aims to verify that the getVideoDecoder() function works correctly after a video sink has been opened and is in the READY state. It ensures that the API successfully retrieves the video decoder ID without any errors.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 034@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description                                             | Test Data                                     |Expected Result             |Notes                            |
* | :--------------: | ------------------------------------------------------- | -------------------------------------------- | -------------------------- | ------------------------------- |
* | 01 | Check initial state | N/A |State = CLOSED |Done by Pre-requisite SetUp function |
* | 02 | Open the video sink and obtain controller | N/A |videoSinkController != nullptr |Done by Pre-requisite SetUp function |
* | 03 | Check state after open | N/A |State = READY |Done by Pre-requisite SetUp function |
* | 04 | Call getVideoDecoder() API | None | Should return valid decoder ID | Should Pass |
* | 05 | Check if the function executed successfully | None | status.isOk() should return true | Should Pass |
* | 06 | Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 07 | Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_positive_1_getVideoDecoder) 
{
    ::com::rdk::hal::videodecoder::IVideoDecoder::Id decoderId;
    // invoking getVideoDecoder() api from READY state
    Status status = videoSinkController->getVideoDecoder(&decoderId);
    ASSERT_TRUE(status.isOk());
}

/**
* @brief Executes a test case to retrieve the video decoder ID from a video sink controller in the STARTED state.
*
* This test checks the functionality of the video sink controller's method to get the video decoder ID when the video sink is in the STARTED state. It ensures that the API call completes successfully and returns the expected results.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 035@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Pre-requisite SetUp function from L1VideoSinkOpenTest::SetUp | | | Done by Pre-requisite SetUp function |
* | 02 | Check initial state | N/A |State = CLOSED |Done by Pre-requisite SetUp function |
* | 03 | Open the video sink and obtain controller | N/A |videoSinkController != nullptr |Done by Pre-requisite SetUp function |
* | 04 | Check state after open | N/A |State = READY |Done by Pre-requisite SetUp function |
* | 05 | Pre-requisite SetUp function in L1VideoSinkStartTest::SetUp | | | Done by Pre-requisite SetUp function |
* | 06 | Starting the video sink.| N/A  | The state should be STARTED, and the method should execute without errors. | Should be successful |
* | 07 | Checking the state after starting the video sink.  |  N/A  | State should be STARTED.  | Should be successful |
* | 08 | Invoke getVideoDecoder() to retrieve the video decoder ID while in STARTED state | None | status.isOk() should be true and decoder ID should be valid | Should Pass |
* | 09 | Pre-requisite TearDown function from L1VideoSinkStartTest::TearDown | | | Done by Pre-requisite TearDown function |
* | 10 | Stopping the video sink. | N/A | The state should change to READY | Done by Pre-requisite TearDown function |
* | 11 | Pre-requisite TearDown function in L1VideoSinkOpenTest::TearDown | | | Done by Pre-requisite TearDown function |
* | 12 | Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 13 | Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkStartTest, test_l1_videosink_positive_2_getVideoDecoder) 
{
    ::com::rdk::hal::videodecoder::IVideoDecoder::Id decoderId;
    // invoking getVideoDecoder() api from STARTED state
    Status status = videoSinkController->getVideoDecoder(&decoderId);
    ASSERT_TRUE(status.isOk());
}

/**
* @brief Test the functionality of starting the video sink using a positive scenario.
*
* This test verifies that the video sink can successfully be started when the controller is in the ready state and that the subsequent stop operation also succeeds. It ensures that the start and stop functionalities are operational in positive scenarios.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 036@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | SetUp function to ensure the initial state is ready and open the video sink controller. | None | state should be READY | Done by Pre-requisite SetUp function |
* | 02 | Call start() on videoSinkController | None | status.isOk() should be true | Should Pass |
* | 03 | Call stop() on videoSinkController | None | status.isOk() should be true | Should Pass |
* | 04 | TearDown function to close the video sink controller. | None | Proper teardown of state. | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_positive_1_start)
{
    Status status = videoSinkController->start();
    ASSERT_TRUE(status.isOk());
    //cleaning up
    status = videoSinkController->stop();
    ASSERT_TRUE(status.isOk());
}

/**
* @brief This test validates the behavior of the `start` method when called in the STARTED state.
*
* This test ensures that calling the `start` method from the STARTED state returns an ILLEGAL_STATE exception, validating proper state management in the video sink.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 037@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description  | Test Data | Expected Result | Notes  |
* | :--------------: | -------------| --------- | -------------- | ------------------------------ |
* | 01 | Check initial state | N/A |State = CLOSED |Done by Pre-requisite SetUp function |
* | 02 | Open the video sink and obtain controller | N/A |videoSinkController != nullptr |Done by Pre-requisite SetUp function |
* | 03 | Check state after open | N/A |State = READY |Done by Pre-requisite SetUp function |
* | 04 | Starting the video sink.| N/A  | The state should be STARTED, and the method should execute without errors. | Should be successful |
* | 05 | Checking the state after starting the video sink.  |  N/A  | State should be STARTED.  | Should be successful |
* | 06 | Calling start() again when already in STARTED state.  | status.exceptionCode() | The method should return an ILLEGAL_STATE exception. | Should fail |
* | 07 | Stopping the video sink. | N/A | The state should change to READY | Done by Pre-requisite TearDown function |
* | 08 | Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 09 | Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkStartTest, test_l1_videosink_negative_1_start)
{
    //calling start() from state STARTED
    Status status = videoSinkController->start();
    ASSERT_EQ(status.exceptionCode(), ::android::binder::Status::EX_ILLEGAL_STATE);
}

/**
* @brief Test the stop function of the video sink controller
*
* This test verifies that the `stop` function of the video sink controller works correctly by 
* ensuring the controller can stop without errors and that the state changes as expected.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 038@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Pre-requisite SetUp function from L1VideoSinkOpenTest::SetUp | | | Done by Pre-requisite SetUp function |
* | 02 | Check initial state | N/A |State = CLOSED |Done by Pre-requisite SetUp function |
* | 03 | Open the video sink and obtain controller | N/A |videoSinkController != nullptr |Done by Pre-requisite SetUp function |
* | 04 | Check state after open | N/A |State = READY |Done by Pre-requisite SetUp function |
* | 05 | Pre-requisite SetUp function in L1VideoSinkStartTest::SetUp | | | Done by Pre-requisite SetUp function |
* | 06 | Starting the video sink.| N/A  | The state should be STARTED, and the method should execute without errors. | Should be successful |
* | 07 | Checking the state after starting the video sink.  |  N/A  | State should be STARTED.  | Should be successful |
* | 08 | Stop the video sink controller | N/A | status.isOk() = true | Should Pass |
* | 09 | Pre-requisite TearDown function in L1VideoSinkOpenTest::TearDown | | | Done by Pre-requisite TearDown function |
* | 10 | Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 11 | Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkStopTest, test_l1_videosink_positive_1_stop)
{
    Status status = videoSinkController->stop();
    ASSERT_TRUE(status.isOk());
}

/**
* @brief This test verifies the stop() method behavior when the VideoSinkController is in an invalid state.
*
* The test ensures that the stop() method properly handles and reports an illegal state exception when it is called on the VideoSinkController while in the 'READY' state, which is not a valid state to invoke stop().
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 039@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Check initial state | N/A |State = CLOSED |Done by Pre-requisite SetUp function |
* | 02 | Open the video sink and obtain controller | N/A |videoSinkController != nullptr |Done by Pre-requisite SetUp function |
* | 03 | Check state after open | N/A |State = READY |Done by Pre-requisite SetUp function |
* | 04 | Attempt to stop when in READY state | None |status.exceptionCode() = ::android::binder::Status::EX_ILLEGAL_STATE | Should Fail |
* | 05 | Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 06 | Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_1_stop)
{
    Status status = videoSinkController->stop();
    ASSERT_EQ(status.exceptionCode(), ::android::binder::Status::EX_ILLEGAL_STATE);
}

/**
* @brief This test function verifies the flush operation on the Video Sink Controller.
*
* The test validates that the flush operation works correctly by ensuring that the status returned is OK. This is important to confirm that the Video Sink can handle the flush command when in the STARTED state.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 040@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Pre-requisite SetUp function from L1VideoSinkOpenTest::SetUp | | | Done by Pre-requisite SetUp function |
* | 02 | Check initial state | N/A |State = CLOSED |Done by Pre-requisite SetUp function |
* | 03 | Open the video sink and obtain controller | N/A |videoSinkController != nullptr |Done by Pre-requisite SetUp function |
* | 04 | Check state after open | N/A |State = READY |Done by Pre-requisite SetUp function |
* | 05 | Pre-requisite SetUp function in L1VideoSinkStartTest::SetUp | | | Done by Pre-requisite SetUp function |
* | 06 | Starting the video sink.| N/A  | The state should be STARTED, and the method should execute without errors. | Should be successful |
* | 07 | Checking the state after starting the video sink.  |  N/A  | State should be STARTED.  | Should be successful |
* | 08 | Flush the Video Sink | holdLastFrame = true | status.isOk() should be true | Should pass |
* | 09 | Pre-requisite TearDown function from L1VideoSinkStartTest::TearDown | | | Done by Pre-requisite TearDown function |
* | 10 | Stopping the video sink. | N/A | The state should change to READY | Done by Pre-requisite TearDown function |
* | 11 | Pre-requisite TearDown function in L1VideoSinkOpenTest::TearDown | | | Done by Pre-requisite TearDown function |
* | 12 | Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 13 | Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
 */
TEST_F(L1VideoSinkStartTest, test_l1_videosink_positive_1_flush)
{
    bool holdLastFrame = true;
    Status status = videoSinkController->flush(holdLastFrame);
    ASSERT_TRUE(status.isOk());
}

/**
* @brief Test the negative scenario of flushing the video sink in an incorrect state
*
* This test verifies that calling the flush() function on the video sink controller while the video sink is in the READY state results in an EX_ILLEGAL_STATE exception. This ensures that the system properly handles illegal state operations.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 041@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Check initial state | N/A |State = CLOSED |Done by Pre-requisite SetUp function |
* | 02| Open the video sink and obtain controller | N/A |videoSinkController != nullptr |Done by Pre-requisite SetUp function |
* | 03| Check state after open | N/A |State = READY |Done by Pre-requisite SetUp function |
* | 04| Attempt to flush the video sink while in the READY state | holdLastFrame = true | status.exceptionCode() = Status::EX_ILLEGAL_STATE | Should fail |
* | 05| Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 06| Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_1_flush)
{
    bool holdLastFrame = true;
    //calling flush() from READY state (EX_ILLEGAL_STATE)
    Status status = videoSinkController->flush(holdLastFrame);
    ASSERT_EQ(status.exceptionCode(), Status::EX_ILLEGAL_STATE);
}

/**
* @brief This test verifies the functionality of discarding frames in the video sink until a given presentation time.
*
* This test is designed to validate whether the `discardFramesUntil` function correctly discards frames up to the specified presentation time.
*
* **Test Group ID:** Basic: 01 @n
* **Test Case ID:** 042 @n
* **Priority:** High @n
* @n
* **Pre-Conditions:** None @n
* **Dependencies:** None @n
* **User Interaction:** None @n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Pre-requisite SetUp function from L1VideoSinkOpenTest::SetUp | | | Done by Pre-requisite SetUp function |
* | 02 | Check initial state | N/A |State = CLOSED |Done by Pre-requisite SetUp function |
* | 03 | Open the video sink and obtain controller | N/A |videoSinkController != nullptr |Done by Pre-requisite SetUp function |
* | 04 | Check state after open | N/A |State = READY |Done by Pre-requisite SetUp function |
* | 05 | Pre-requisite SetUp function in L1VideoSinkStartTest::SetUp | | | Done by Pre-requisite SetUp function |
* | 06 | Starting the video sink.| N/A  | The state should be STARTED, and the method should execute without errors. | Should be successful |
* | 07 | Checking the state after starting the video sink.  |  N/A  | State should be STARTED.  | Should be successful |
* | 08 | Invoke discardFramesUntil with nsPresentationTime argument | nsPresentationTime = 1000000000 | status.isOk() = true | Should Pass |
* | 09 | Pre-requisite TearDown function from L1VideoSinkStartTest::TearDown | | | Done by Pre-requisite TearDown function |
* | 10 | Stopping the video sink. | N/A | The state should change to READY | Done by Pre-requisite TearDown function |
* | 11 | Pre-requisite TearDown function in L1VideoSinkOpenTest::TearDown | | | Done by Pre-requisite TearDown function |
* | 12 | Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 13 | Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkStartTest, test_l1_videosink_positive_1_discardFramesUntil) 
{
    int64_t nsPresentationTime = 1000000000; 
    android::binder::Status status = videoSinkController->discardFramesUntil(nsPresentationTime);
    EXPECT_TRUE(status.isOk());
}

/**
* @brief Test the discardFramesUntil method to handle incorrect state transitions
*
* This test case validates if the discardFramesUntil method correctly handles an illegal state transition 
* by checking if it fails when invoked from the READY state.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 043@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Check initial state | N/A |State = CLOSED |Done by Pre-requisite SetUp function |
* | 02| Open the video sink and obtain controller | N/A |videoSinkController != nullptr |Done by Pre-requisite SetUp function |
* | 03| Check state after open | N/A |State = READY |Done by Pre-requisite SetUp function |
* | 04| Invoke discardFramesUntil from READY state | nsPresentationTime = 500000000 |status.exceptionCode() = android::binder::Status::EX_ILLEGAL_STATE |Should Fail |
* | 05| Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 06| Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkOpenTest, test_l1_videosink_negative_1_discardFramesUntil) 
{
    int64_t nsPresentationTime = 500000000; 
    /* API invocation from READY STATE*/
    android::binder::Status status = videoSinkController->discardFramesUntil(nsPresentationTime);
    EXPECT_EQ(status.exceptionCode(), android::binder::Status::EX_ILLEGAL_STATE);
}
/**
* @brief Test the discardFramesUntil method with a negative presentation time.
*
* This test checks the discardFramesUntil method by providing a negative presentation time. 
* The method is expected to fail and return a status indicating the failure.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 044@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Pre-requisite SetUp function from L1VideoSinkOpenTest::SetUp | | | Done by Pre-requisite SetUp function |
* | 02 | Check initial state | N/A |State = CLOSED |Done by Pre-requisite SetUp function |
* | 03 | Open the video sink and obtain controller | N/A |videoSinkController != nullptr |Done by Pre-requisite SetUp function |
* | 04 | Check state after open | N/A |State = READY |Done by Pre-requisite SetUp function |
* | 05 | Pre-requisite SetUp function in L1VideoSinkStartTest::SetUp | | | Done by Pre-requisite SetUp function |
* | 06 | Starting the video sink.| N/A  | The state should be STARTED, and the method should execute without errors. | Should be successful |
* | 07 | Checking the state after starting the video sink.  |  N/A  | State should be STARTED.  | Should be successful |
* | 08 | Invoking discardFramesUntil method with a negative presentation time | nsPresentationTime = -1000000000 | status.isOk() == false | Should Fail |
* | 09 | Pre-requisite TearDown function from L1VideoSinkStartTest::TearDown | | | Done by Pre-requisite TearDown function |
* | 10 | Stopping the video sink. | N/A | The state should change to READY | Done by Pre-requisite TearDown function |
* | 11 | Pre-requisite TearDown function in L1VideoSinkOpenTest::TearDown | | | Done by Pre-requisite TearDown function |
* | 12 | Check state before closing | State check before close | state = READY | Done by Pre-requisite TearDown function |
* | 13 | Close VideoSinkController | videoSinkController | status.isOk() = true | Done by Pre-requisite TearDown function |
*/
TEST_F(L1VideoSinkStartTest, test_l1_videosink_negative_2_discardFramesUntil) 
{
    int64_t nsPresentationTime = -1000000000; 
    android::binder::Status status = videoSinkController->discardFramesUntil(nsPresentationTime);
    EXPECT_FALSE(status.isOk());
}

int main(int argc, char **argv) 
{
    std::string yaml_file;
    int opt;
    int result1;
    int result2;
    int number_of_videosink;
    int number_of_videodecoders;

    std::string videosinkpath = "components.IVideoSinkManager.IVideoSink";
    std::string videodecoderpath = "components.IVideoDecoderManager.IVideoDecoder";

//    ::testing::InitGoogleTest(&argc, argv);
   UT_init(argc, argv);
    if(getService() == -1)
    {
        return -1;
    }

 optind = 1;

    while ((opt = getopt(argc, argv, "p:")) != -1) 
    {
        switch (opt) 
        {
            case 'p':
                yaml_file = optarg;
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " -p <yaml_file>" << std::endl;
                return 1;
        }
    }

    if (!yaml_file.empty()) 
    {
        try 
        {
            YAML::Node config = YAML::LoadFile(yaml_file.c_str());

            number_of_videodecoders = get_list_Count_from_profile(config, videodecoderpath);
            if(number_of_videodecoders <= 0)
            {
                std::cout<< " No videodecoders find in the profile" <<std::endl;
                return -1;
            }
            std::cout <<"videodecoders : " <<number_of_videodecoders <<std::endl;

            for(int i=0; i<number_of_videodecoders; ++i)
            {
                std::string videodecoderidpath = videodecoderpath+"."+std::to_string(i) + ".id";
                int decoder_id = get_integer_value_from_profile(config,videodecoderidpath);
                _videodecoder_ids_from_profile.push_back(decoder_id);
            }

            for(const auto& _id: _videodecoder_ids_from_profile)
            {
                std::cout <<"--------------------> " <<_id <<std::endl;
            }


            number_of_videosink = get_list_Count_from_profile(config, videosinkpath);
            if(number_of_videosink <= 0)
            {
                std::cout<< " No videosink find in the profile" <<std::endl;
                return -1;
            }
            std::cout <<"videosink count : " <<number_of_videosink <<std::endl;

            for(int i=0; i<number_of_videosink; ++i)
            {
                std::string videosinkidpath = videosinkpath+"."+std::to_string(i) + ".id";
                int id = get_integer_value_from_profile(config,videosinkidpath);
                _videosink_ids_from_profile.push_back(id);
            }

            for(const auto& _id: _videosink_ids_from_profile)
            {
                g_videosinkId.value = _id;
                Status status = service->getVideoSink(g_videosinkId, &_videoSink);
                ::testing::GTEST_FLAG(filter) = "L1VideoSinkTest.*:L1VideoSinkEventRegisterTest.*:L1VideoSinkOpenTest.*:L1VideoSinkStartTest.*:L1VideoSinkStopTest.*:L1VideoSinkCloseTest.*";
                result1 = RUN_ALL_TESTS();
            }
            ::testing::GTEST_FLAG(filter) = "L1VideoSinkManagerTest.*";
            result2 = RUN_ALL_TESTS();
            return (result1 || result2);
        }
        catch (const YAML::BadFile& e) 
        {
            std::cerr << "Error: Could not open YAML file. " << e.what() << std::endl;
            return -1;
        } 
        catch (const YAML::BadConversion& e) 
        {
            std::cerr << "Error: Bad type conversion in YAML. " << e.what() << std::endl;
            return -1;
        }
        catch (const std::runtime_error& e) 
        {
            std::cerr << "Runtime Error: " << e.what() << std::endl;
            return -1;
        } 
        catch (const std::exception& e) 
        {
            std::cerr << "Error: " << e.what() << std::endl;
            return -1;
        }
    }
    else
    {
        std::cout << "USAGE : ./VideoSinkClient -p <config_file>" <<std::endl;
        return -1;
    }

}


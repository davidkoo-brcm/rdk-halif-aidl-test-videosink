/**
 *  If not stated otherwise in this file or this component's LICENSE
 *  file the following copyright and licenses apply:
 *
 *  Copyright 2024 RDK Management
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

/**
 * @addtogroup HPK Hardware Porting Kit
 * @{
 * @par The Hardware Porting Kit
 * HPK is the next evolution of the well-defined Hardware Abstraction Layer
 * (HAL), but augmented with more comprehensive documentation and test suites
 * that OEM or SOC vendors can use to self-certify their ports before taking
 * them to RDKM for validation or to an operator for final integration and
 * deployment. The Hardware Porting Kit effectively enables an OEM and/or SOC
 * vendor to self-certify their own Video Accelerator devices, with minimal RDKM
 * assistance.
 *
 */

/**
 * @addtogroup VIDEO_SINK Video Sink Module
 * @{
 */

/**
 * @addtogroup VIDEO_SINK_HALTEST Video Sink HAL Tests
 * @{
 */

/**
 * @defgroup VIDEO_SINK_HALTEST_PARSE_TESTCONFIG Video Sink HAL Tests parse test configuration file
 * @{
 * @parblock
 *
 * ### Parse test configuration functions for Video Sink :
 *
 * Parse configuration functions required for the module across all vendors.
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_videosink_parse_testconfig.cpp
 *
 */

#include <filesystem>
#include "vts_videosink_parse_testconfig.h"
#include "vts_videosink.h"

using namespace std;

UT_status_t vts_videosink_parse_testconfig(void* pTestInstance, const char *module, const char *testGroup, const char *testName, vector<videoTestConfiguration_t>& testConfigurations)
{
    ut_kvp_instance_t* kvpTestInstance = (ut_kvp_instance_t*)pTestInstance;
    string testList = string(module) + "." + string(testGroup) + "." + string(testName);
    string controlList = testList + ".Controls";
    string streamList = testList + "." + "Inputs" + "." + "Streams";
    int32_t streamCount = 0;

    streamCount = ut_kvp_getListCount(kvpTestInstance, streamList.c_str());
    if (streamCount <= 0)
    {
        UT_LOG_ERROR("No streams found in %s", streamList.c_str());
        return UT_STATUS_FAILURE;
    }
    UT_LOG_INFO("Found %d streams", streamCount);

    for (int32_t i = 0; i < streamCount; i++)
    {

        videoTestConfiguration_t testDetails = {};
        // === Only parse Control Parameters ===
        testDetails.signalDiscontinuityPts = ut_kvp_getUInt64Field(kvpTestInstance, (controlList + ".discontinuityPTS").c_str());
        testDetails.testName = std::string(testName ? testName : "");

        string vSink_discardFramesUntilPts = streamList + "." + to_string(i) + ".sink_discardFramesUntilPts";
        string vSink_underflowPts          = streamList + "." + to_string(i) + ".sink_underflow_pts";
        string vSink_resumeTriggerPts      = streamList + "." + to_string(i) + ".sink_resumeTriggerPts";
        string vSink_repeatPerFrame        = streamList + "." + to_string(i) + ".sink_repeatPerFrame";
        string vSink_flush_pts             = streamList + "." + to_string(i) + ".sink_flush_pts";
        string vSink_flush_reset_pts       = streamList + "." + to_string(i) + ".sink_flush_reset_pts";
        string vSink_eos_pts               = streamList + "." + to_string(i) + ".sink_eos_pts";
        string vSink_interFrameDelayMs     = streamList + "." + to_string(i) + ".sink_interFrameDelayMs";
        string vSink_discontinuityPTS      = streamList + "." + to_string(i) + ".sink_discontinuityPTS";

        testDetails.vSink_discardFramesUntilPts = ut_kvp_getUInt64Field(kvpTestInstance, vSink_discardFramesUntilPts.c_str());
        testDetails.vSink_underflowPts          = ut_kvp_getUInt64Field(kvpTestInstance, vSink_underflowPts.c_str());
        testDetails.vSink_resumeTriggerPts      = ut_kvp_getUInt64Field(kvpTestInstance, vSink_resumeTriggerPts.c_str());
        testDetails.vSink_repeatPerFrame        = ut_kvp_getUInt64Field(kvpTestInstance, vSink_repeatPerFrame.c_str());
        testDetails.vSink_flush_pts             = ut_kvp_getUInt64Field(kvpTestInstance, vSink_flush_pts.c_str());
        testDetails.vSink_flush_reset_pts       = ut_kvp_getUInt64Field(kvpTestInstance, vSink_flush_reset_pts.c_str());
        testDetails.vSink_eos_pts               = ut_kvp_getUInt64Field(kvpTestInstance, vSink_eos_pts.c_str());
        testDetails.vSink_interFrameDelayMs     = ut_kvp_getUInt64Field(kvpTestInstance, vSink_interFrameDelayMs.c_str());
        testDetails.vSink_discontinuityPTS      = ut_kvp_getUInt64Field(kvpTestInstance, vSink_discontinuityPTS.c_str());

        // No input or frames parsed here — decoder side provides them
        testConfigurations.push_back(testDetails);

        // Optional: log for debug
        UT_LOG_INFO("Parsed Sink Config: testName=%s, eos=%ld, underflow=%ld, resume=%ld, flush=%ld, interDelay=%d",
                    testDetails.testName.c_str(),
                    testDetails.vSink_eos_pts,
                    testDetails.vSink_underflowPts,
                    testDetails.vSink_resumeTriggerPts,
                    testDetails.vSink_flush_pts,
                    testDetails.vSink_interFrameDelayMs);
    }
    return UT_STATUS_OK;
}





/** @} */ // End of VIDEO_SINK_HALTEST_PARSE_TESTCONFIG
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK

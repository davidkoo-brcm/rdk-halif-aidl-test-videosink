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

static uint32_t codecFromString(const std::string& codecStr)
{
    if (codecStr == "MPEG") return 1;
    if (codecStr == "H264") return 2;
    if (codecStr == "H265") return 3;
    if (codecStr == "VP9")  return 4;
    if (codecStr == "AV1")  return 5;
    return 0; // unknown codec
}

UT_status_t vts_videosink_parse_testconfig(void* pTestInstance, const char *module, const char *testGroup, const char *testName, vector<videoTestConfiguration_t>& testConfigurations)
{
    ut_kvp_instance_t* kvpTestInstance = (ut_kvp_instance_t*)pTestInstance;
    string testList = string(module) + "." + string(testGroup) + "." + string(testName);
    string controlList = testList + ".Controls";

    videoTestConfiguration_t testDetails;

    // === Only parse Control Parameters ===
    testDetails.signalDiscontinuityPts = ut_kvp_getUInt64Field(kvpTestInstance, (controlList + ".discontinuityPTS").c_str());
    testDetails.flushPts              = ut_kvp_getUInt64Field(kvpTestInstance, (controlList + ".flush_pts").c_str());
    testDetails.flushResetPts        = ut_kvp_getUInt64Field(kvpTestInstance, (controlList + ".flush_reset_pts").c_str());
    testDetails.eosPts               = ut_kvp_getUInt64Field(kvpTestInstance, (controlList + ".eos_pts").c_str());
    testDetails.repeatPerFrame       = ut_kvp_getUInt32Field(kvpTestInstance, (controlList + ".repeatPerFrame").c_str());
    testDetails.underflowPts         = ut_kvp_getUInt64Field(kvpTestInstance, (controlList + ".underflow_pts").c_str());
    testDetails.resumeTriggerPts     = ut_kvp_getUInt64Field(kvpTestInstance, (controlList + ".resumeTriggerPts").c_str());
    testDetails.discardFramesUntilPts= ut_kvp_getUInt64Field(kvpTestInstance, (controlList + ".discardFramesUntilPts").c_str());
    testDetails.interFrameDelayMs    = ut_kvp_getUInt32Field(kvpTestInstance, (controlList + ".interFrameDelayMs").c_str());
    testDetails.testName             = testName;

    // No input or frames parsed here — decoder side provides them
    testConfigurations.push_back(testDetails);

    // Optional: log for debug
    UT_LOG_INFO("Parsed Sink Config: testName=%s, eos=%ld, underflow=%ld, resume=%ld, flush=%ld, interDelay=%d",
                testDetails.testName.c_str(),
                testDetails.eosPts,
                testDetails.underflowPts,
                testDetails.resumeTriggerPts,
                testDetails.flushPts,
                testDetails.interFrameDelayMs);

    return UT_STATUS_OK;
}





/** @} */ // End of VIDEO_SINK_HALTEST_PARSE_TESTCONFIG
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK

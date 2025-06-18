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

void* vts_kvp_create_instance(char *fileName)
{
    ut_kvp_instance_t* instance;
    ut_kvp_status_t result;

    UT_LOG("In %s", __FUNCTION__);

    instance = ut_kvp_createInstance();

    if(!instance)
    {
        UT_LOG_ERROR("Failed to create ut kvp instance");
        return nullptr;
    }

    result = ut_kvp_open(instance, fileName);

    if(result != UT_KVP_STATUS_SUCCESS)
    {
        UT_LOG_ERROR("Failed to open test configuration file");
        ut_kvp_destroyInstance(instance);
        return nullptr;
    }

    UT_LOG("Out  %s", __FUNCTION__);

    return (ut_kvp_instance_t*)instance;
}

void vts_kvp_destroy_instance(void *instance)
{
    ut_kvp_destroyInstance((ut_kvp_instance_t*)instance);
}


UT_status_t vts_videosink_parse_testconfig(void* pTestInstance, const char *module, const char *testGroup, const char *testName, vector<videoTestConfiguration_t>& testConfigurations)
{
    ut_kvp_status_t status;
    ut_kvp_instance_t* kvpTestInstance = (ut_kvp_instance_t*)pTestInstance;
    char streamInputPath[UT_KVP_MAX_ELEMENT_SIZE] = {0};
    string testList = string(module) + "." + string(testGroup) + "." + string(testName);
    string controlList = testList + ".Controls";
    string streamList = testList + ".Inputs.Streams";

    status = ut_kvp_getStringField(kvpTestInstance, "input_streams_path", streamInputPath, UT_KVP_MAX_ELEMENT_SIZE);
    int32_t streamCount = ut_kvp_getListCount(kvpTestInstance, streamList.c_str());

    for (int32_t i = 0; i < streamCount; ++i)
    {
        char resultKVP[UT_KVP_MAX_ELEMENT_SIZE] = {0};
        videoTestConfiguration_t testDetails;
        string streamNameKey = streamList + "." + to_string(i) + ".Name";
        ut_kvp_getStringField(kvpTestInstance, streamNameKey.c_str(), resultKVP, UT_KVP_MAX_ELEMENT_SIZE);
        string streamKey = string("VideoStreams.") + resultKVP;

        // Try to access a required field; if missing, assume failure
        char testFieldCheck[UT_KVP_MAX_ELEMENT_SIZE] = {0};
        status = ut_kvp_getStringField(kvpTestInstance, (streamKey + ".input").c_str(), testFieldCheck, UT_KVP_MAX_ELEMENT_SIZE);
        if (status != UT_KVP_STATUS_SUCCESS) {
            return UT_STATUS_FAILURE;
        }

        // Populate frame metadata
        testDetails.frameMetadata.name = resultKVP;
        testDetails.frameMetadata.pixelFormat = ut_kvp_getUInt32Field(kvpTestInstance, (streamKey + ".pixel_format").c_str());
        testDetails.frameMetadata.width = ut_kvp_getUInt32Field(kvpTestInstance, (streamKey + ".width").c_str());
        testDetails.frameMetadata.height = ut_kvp_getUInt32Field(kvpTestInstance, (streamKey + ".height").c_str());
        testDetails.frameMetadata.colorDepth = ut_kvp_getUInt32Field(kvpTestInstance, (streamKey + ".color_depth").c_str());
        testDetails.frameMetadata.bufferSize = ut_kvp_getUInt32Field(kvpTestInstance, (streamKey + ".buffer_size").c_str());

        // Get PTS list
        string ptsListKey = streamList + "." + to_string(i) + ".PTS";
        int32_t ptsCount = ut_kvp_getListCount(kvpTestInstance, ptsListKey.c_str());
        for (int32_t k = 0; k < ptsCount; ++k)
        {
            string eachPTS = ptsListKey + "." + to_string(k);
            int64_t PTS = ut_kvp_getUInt64Field(kvpTestInstance, eachPTS.c_str());

            videoFrame_t frame;
            frame.size = testDetails.frameMetadata.bufferSize;
            frame.pts = PTS;
            frame.errorFrame = false;
            testDetails.frames.push_back(frame);
        }

        // Read control parameters
        testDetails.signalDiscontinuityPts = ut_kvp_getUInt64Field(kvpTestInstance, (controlList + ".signal_discontinuity_pts").c_str());
        testDetails.flushPts = ut_kvp_getUInt64Field(kvpTestInstance, (controlList + ".flush_pts").c_str());
        testDetails.flushResetPts = ut_kvp_getUInt64Field(kvpTestInstance, (controlList + ".flush_reset_pts").c_str());
        testDetails.eosPts = ut_kvp_getUInt64Field(kvpTestInstance, (controlList + ".eos_pts").c_str());

        testConfigurations.push_back(testDetails);
    }

    return UT_STATUS_OK;
}


 /** @} */ // End of VIDEO_SINK_HALTEST_PARSE_TESTCONFIG
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK
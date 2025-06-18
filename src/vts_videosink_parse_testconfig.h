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
 * @defgroup VIDEO_SINK_HALTEST_PARSE_TESTCONFIG_HEADER Video Sink HAL Tests parse test configuration Header File
 * @{
 * @parblock
 *
 * ### Parse test configuration functions for Video Sink :
 *
 * Parse test configuration functions required for the module across all vendors.
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_videosink_parse_testconfig.h
 *
 */
#ifndef __VTS_VIDEOSINK_PARSE_TESTCONFIG_H__
#define __VTS_VIDEOSINK_PARSE_TESTCONFIG_H__

#include <ut.h>
#include <ut_log.h>
#include "ut_kvp_profile.h"
#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"

#define VTS_TEST_CONFIG_PATH "./test_configuration/vts_l3_test_config.yaml"
#define VTS_STREAM_CONFIG_PATH "./test_configuration/vts_stream_list.yaml"
#define STREAM_DOWNLOAD_PATH "IVideoSinkManager.stream_download_path"
#define STREAM_INPUT_PATH "IVideoSinkManager.input_streams_path"
#define STREAM_REF_PATH "IVideoSinkManager.reference_streams_path"

struct videoFrame_t {
    uint32_t size;
    int64_t pts;
    bool errorFrame;
};

struct frameMetadata_t {
    std::string name;
    uint32_t pixelFormat;
    uint32_t width;
    uint32_t height;
    uint32_t colorDepth;
    uint32_t bufferSize;
};

struct videoTestConfiguration_t {
    frameMetadata_t frameMetadata;
    std::vector<videoFrame_t> frames;
    uint64_t signalDiscontinuityPts;
    uint64_t flushPts;
    uint64_t flushResetPts;
    uint64_t eosPts;
};

void* vts_kvp_create_instance(char *fileName);
UT_status_t vts_videosink_parse_testconfig(void* pTestInstance, const char *module, const char *testGroup, const char *testName, std::vector<videoTestConfiguration_t>& streamDetails);
void vts_kvp_destroy_instance(void *instance);

#endif //__VTS_VIDEOSINK_PARSE_TESTCONFIG_H__


/** @} */ // End of VIDEO_SINK_HALTEST_PARSE_TESTCONFIG_HEADER
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK
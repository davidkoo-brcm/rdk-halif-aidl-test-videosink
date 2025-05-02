/**
 *  If not stated otherwise in this file or this component's LICENSE
 *  file the following copyright and licenses apply:
 *
 *  Copyright 2025 RDK Management
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
 * @defgroup VIDEO_SINK_HALTEST_PARSE_HFP Video Sink HAL Tests parse Hardware feature profile
 * @{
 * @parblock
 *
 * ### Parse HFP functions for Video Sink :
 *
 * Parse HFP functions required for the module across all vendors.
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_vidosink_parse_hfp.cpp
 *
 */

#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"

std::vector<int32_t> gVideosinks_hfp;

UT_status_t vts_videosink_parseVideoSinkHFP()
{
    std::map<std::string , int32_t> componentMap;
    std::string vidosinkpath = VIDEOSINKMGR_IVIDEOSINK;
    int32_t number_of_vidosinks;
    int id;

    number_of_vidosinks = UT_KVP_PROFILE_GET_LIST_COUNT(vidosinkpath.c_str());
    if(number_of_vidosinks <= 0)
    {
        UT_LOG_ERROR("No vidosinks found in the HFP");
        return UT_STATUS_FAILURE;
    }

    for(int32_t i = 0; i < number_of_vidosinks; ++i)
    {
        std::string vidosinkidpath = vidosinkpath + "." + std::to_string(i) + ".id";

        id = UT_KVP_PROFILE_GET_UINT32(vidosinkidpath.c_str());
        gVideosinks_hfp.push_back(id);
    }
    return UT_STATUS_OK;
}

/** @} */ // End of VIDEO_SINK_HALTEST_PARSE_HFP
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK

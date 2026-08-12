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
#include "ut_kvp_profile.h"
#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"
#include "vts_videosink.h"

videoSinkHFP_t gVideosinks_hfp;

UT_status_t vts_videosink_parseVideoSinkHFP()
{
    const std::string videosinkPath = VIDEOSINKMGR_IVIDEOSINK;
    const int32_t videosinkCount = UT_KVP_PROFILE_GET_LIST_COUNT(videosinkPath.c_str());

    if (videosinkCount <= 0)
    {
        UT_LOG_ERROR("No videosinks found in the HFP");
        return UT_STATUS_FAILURE;
    }

    gVideosinks_hfp.videosinks_hfp.clear();

    for (int32_t i = 0; i < videosinkCount; ++i)
    {
        const std::string videosinkPropertyPath = videosinkPath + "." + std::to_string(i);

        videoSinkHFP_t::properties_t sinkProperties;
        sinkProperties.sinkId = UT_KVP_PROFILE_GET_UINT32((videosinkPropertyPath + ".id").c_str());
        sinkProperties.supportsAVSync = UT_KVP_PROFILE_GET_BOOL((videosinkPropertyPath + ".supportsAVSync").c_str());
        sinkProperties.vsyncDisplayLatencyNs = UT_KVP_PROFILE_GET_INT64((videosinkPropertyPath + ".vsyncDisplayLatencyNs").c_str());
        sinkProperties.supportsHoldLastFrame = UT_KVP_PROFILE_GET_BOOL((videosinkPropertyPath + ".supportsHoldLastFrame").c_str());
        sinkProperties.supportsSecure = UT_KVP_PROFILE_GET_BOOL((videosinkPropertyPath + ".supportsSecure").c_str());

        gVideosinks_hfp.videosinks_hfp.push_back(sinkProperties);
    }

    return UT_STATUS_OK;
}

/** @} */ // End of VIDEO_SINK_HALTEST_PARSE_HFP
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK

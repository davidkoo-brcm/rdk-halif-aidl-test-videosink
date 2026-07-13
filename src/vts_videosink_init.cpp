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
 * @defgroup VIDEO_SINK_HALTEST_INIT Video Sink HAL Tests helper functions
 * @{
 * @parblock
 *
 * ### Initialization functions for VTS tests :
 *
 * Initialization functions required for the module across all vendors.
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_videosink_init.cpp
 *
 */

#include <ut.h>
#include <ut_log.h>
#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"
#include "vts_videosink_init.h"
#include "vts_videosink.h"

sp<IVideoSinkManager> gVideoSinkManager;

void *gVideoSinkTestConfiguration;

UT_status_t VTS_VideoSink_init()
{
    if(vts_videosink_getVideoSinkService() != UT_STATUS_OK)
    {
        UT_LOG_ERROR("Failed to process the HFP file");
        return UT_STATUS_FAILURE;
    }
    if(vts_videosink_parseVideoSinkHFP() != UT_STATUS_OK)
    {
        UT_LOG_ERROR("Failed to process the HFP file");
        return UT_STATUS_FAILURE;
    }
    gVideoSinkTestConfiguration = vts_kvp_create_instance(VIDEO_SINK_TEST_CONFIG_PATH);

    return UT_STATUS_OK;
}

void VTS_VideoSink_destroy()
{
    vts_kvp_destroy_instance(gVideoSinkTestConfiguration);
}

/** @} */ // End of VIDEO_SINK_HALTEST_INIT
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK
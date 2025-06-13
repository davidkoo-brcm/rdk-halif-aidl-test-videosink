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
 * @defgroup VIDEO_SINK_HALTEST Video Sink HAL Tests
 * @{
 */

/**
 * @defgroup VIDEO_SINK_HALTEST_MAIN Video Sink HAL Tests Main File
 *  @{
 * @parblock
 *
 * ### Tests for Video Sink HAL :
 *
 * This is to ensure that the API meets the operational requirements of the module across all vendors.
 *
 * **Pre-Conditions:** None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 */

/**
 * @file main.c
 *
 */

#include <ut.h>
#include <ut_log.h>
#include "vts_videosink_init.h"

int main(int argc, char **argv)
{
    UT_status_t result;

    UT_init(argc, argv);

    UT_LOG_INFO("==========================================================================");
    UT_LOG_INFO("\t AIDL HALIF Version: \033[0;32m%s\033[0m",RDK_HALIF_AIDL_VERSION);
    UT_LOG_INFO("\t Video Sink HALIF Test Version: \033[0;32m%s\033[0m",HALIF_TEST_TAG_VERSION);
    UT_LOG_INFO("==========================================================================");

    result = VTS_VideoSink_init();

    result = UT_run_tests();
    return result;
}

/** @} */ // End of VIDEO_SINK_HALTEST_MAIN
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK Module
/** @} */ // End of HPK
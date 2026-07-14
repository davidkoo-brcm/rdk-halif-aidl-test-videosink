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
 * @defgroup VIDEO_SINK_HALTEST_HELPER_HEADER Video Sink HAL Tests helper functions Header File
 * @{
 * @parblock
 *
 * ### Helper functions for VTS tests :
 *
 * Helper functions required for the module across all vendors.
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_videosink_helper_functions.h
 *
 */

#ifndef __VTS_VIDEOSINK_HELPER_H__
#define __VTS_VIDEOSINK_HELPER_H__

#include <ut.h>
#include <ut_kvp_profile.h>
#include <ut_log.h>
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
#include <string>

#include "com/rdk/hal/videosink/IVideoSinkManager.h"
#include "com/rdk/hal/videosink/BnVideoSinkControllerListener.h"
#include "com/rdk/hal/videosink/BnVideoSinkEventListener.h"
#include "com/rdk/hal/videodecoder/PixelFormat.h"

using android::sp;
using android::String16;
using android::defaultServiceManager;
using android::binder::Status;

using namespace com::rdk::hal;
using namespace com::rdk::hal::videosink;
using namespace com::rdk::hal::videodecoder;

UT_status_t vts_videosink_getVideoSinkService();
bool vts_videosink_helper_validatePropertyValue(int decoder_id, const std::optional<PropertyValue>& propertyValue, const com::rdk::hal::videosink::Property property);
bool vts_videosink_helper_areElementsUnique(const std::vector<int>& values);
void vts_videosink_helper_validateCapabilities(int decoder_id, const com::rdk::hal::videosink::Capabilities& capabilities);
com::rdk::hal::videodecoder::Codec vts_videodecoder_helper_StringTocodec(const std::string& format);

float getBytesPerPixelMultiplier(const std::string& basePixelFormat, int colorDepth);
PixelFormat getPixelFormatEnum(const std::string& basePixelFormat);

#endif //__VTS_VIDEOSINK_HELPER_H__

/** @} */ // End of VIDEO_SINK_HALTEST_HELPER_HEADER
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK
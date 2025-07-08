/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
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
 * @addtogroup HPK Hardware Porting Kit
 * @{
 *
 */

/**
 * @addtogroup VIDEO_SINK Video Sink Module
 * @{
 *
 */

/**
 * @defgroup VIDEO_SINK_HALTEST Video Sink HAL Tests
 * @{
 *
 */

/**
 * @defgroup VTS_L3_AVBUFFER Video Sink HAL Tests L3 File
 * @{
 * @parblock
 *
 * ### AVBUFFER CALLS FOR L3:
 *
 * This module includes av buffer calls *
 * **Pre-Conditions:** @n
 * **Dependencies:** None@n
 *
 * @endparblock
 *
 */

/**
 * @file vts_l3_avbuffer.cpp
 *
 */
#include "vts_l3_avbuffer.h"

using android::sp;
using com::rdk::hal::avbuffer::IAVBuffer;
using android::binder::Status;

sp<IAVBuffer> vts_avbuffer_getService()
{
    return
        android::interface_cast<IAVBuffer>(
            android::defaultServiceManager()->getService(
                android::String16(IAVBuffer::serviceName().c_str())
            )
        );
}

// Constructor
AVBufferSpaceListener::AVBufferSpaceListener() = default;

// onSpaceAvailable implementation
Status AVBufferSpaceListener::onSpaceAvailable()
{
    // Add log if needed
    return Status::ok();
}

// Destructor
AVBufferSpaceListener::~AVBufferSpaceListener() = default;

/** @} */ // End of VTS_L3_AVBUFFER
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK

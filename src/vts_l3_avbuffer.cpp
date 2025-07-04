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
# Testing Suite For Video Sink HAL

## Build Steps

### 1. Build linux binder

The test suite depends on the Linux binder with `AIDL` support. Follow these steps to build it:

For detail build instruction, please refer to [Linux Binder](https://github.com/rdk-e/linux_binder_idl/tree/main)

Here outline the simplest steps are

```bash
git clone -b develop 'ssh://git@github.com/rdk-e/linux_binder_idl.git'

cd linux_binder_idl

./build-linux-binder-aidl.sh 64
```

### 2. Build this test

Clone and build the test suite:

```bash
git clone git@github.com:rdkcentral/rdk-halif-binder-test-videosink.git
```

```bash
cd rdk-halif-binder-test-videosink
```

Set below environment variable:

- AIDL_BIN: Path to the `AIDL` compiler (e.g., /path/to/linux_binder_idl/out/aidl).

- LINUX_BINDER_OUT_DIR: Path to the Linux binder output directory (e.g., /path/to/linux_binder_idl/local).

- UT_CORE_VERSION: Specific version of the UT-Core framework (e.g., 4.7.0). Defaults to the latest tagged version.

- RDK_HALIF_AIDL_VERSION: Specific version of the AIDL HAL Specification (e.g, 0.7.1). Defaults to the latest HEAD.

Example:

```bash
export UT_CORE_VERSION=4.7.0
export RDK_HALIF_AIDL_VERSION=0.7.1
export AIDL_BIN=/path/linux_binder_idl/out/aidl
export LINUX_BINDER_OUT_DIR=/path//linux_binder_idl/local/
```

Disable Logging:

Too many logs can slow down the test and delay frame pushing from the decoder to the sink.
To avoid this and make sure frames are queued on time, run the following command before starting the build:

```bash
export DISABLE_UT_DEBUG_LOGGING=1
```

This will turn off debug logs and help the test run smoothly.

Run the build script:

```bash
./build.sh all
```

The build.sh script:

- Clones the `rdk-halif-aidl` repository and UT-Core framework.

- Compiles the test suite.

- Generates binaries in the `build/bin` directory.

Example directory structure :

```bash

build/bin/
├── assets
│   └── streams
│       ├── input
│       └── reference
├── lib
├── libaudiodecoderaidl.so
├── libavbufferaidl.so
├── libbase.so
├── libbinder.so
├── libcutils.so
├── libcutils_sockets.so
├── libhal_logger.so
├── liblog.so
├── libut_control.so
├── libutils.so
├── libvideodecoderaidl.so
├── libvideosinkaidl.so
├── MockVideoSinkService
├── profiles
│   ├── hfp-videosink_vdevice_instance_1.yaml
│   ├── hfp-videosink_vdevice_instance_2.yaml
│   ├── hfp-videosink_vdevice_instance_3.yaml
│   ├── hfp-videosink_vdevice.yaml
│   └── hfp-videosink.yaml
├── run.sh
├── servicemanager
├── test_configuration
│   ├── vts_l3_H264_HD_30_YUV420.yaml
│   ├── vts_l3_H264_HD_30_YUV444.yaml
│   ├── vts_l3_H264_HD_60_YUV420.yaml
│   ├── vts_l3_H264_HD_60_YUV444.yaml
│   ├── vts_l3_H264_SD_30_YUV420.yaml
│   ├── vts_l3_H264_SD_30_YUV444.yaml
│   ├── vts_l3_H264_SD_60_YUV420.yaml
│   ├── vts_l3_H264_SD_60_YUV444.yaml
│   ├── vts_l3_H264_UHD_30_YUV420.yaml
│   ├── vts_l3_H264_UHD_30_YUV444.yaml
│   ├── vts_l3_H264_UHD_60_YUV420.yaml
│   ├── vts_l3_H264_UHD_60_YUV444.yaml
│   ├── vts_l3_Sink_Concurrent_NonTunnel_NonSecure_DiscardFramesUntil.yaml
│   ├── vts_l3_Sink_Concurrent_NonTunnel_NonSecure_EOS.yaml
│   ├── vts_l3_Sink_Concurrent_NonTunnel_NonSecure_FramesDroppedFRC.yaml
│   ├── vts_l3_Sink_Concurrent_NonTunnel_NonSecure_FramesDroppedLate.yaml
│   ├── vts_l3_Sink_Concurrent_NonTunnel_NonSecure_FramesMiss.yaml
│   ├── vts_l3_Sink_Concurrent_NonTunnel_NonSecure_FramesReceived.yaml
│   ├── vts_l3_Sink_Concurrent_NonTunnel_NonSecure_FramesRepeatedFRC.yaml
│   ├── vts_l3_Sink_Concurrent_NonTunnel_NonSecure_QueueDepth.yaml
│   ├── vts_l3_Sink_Concurrent_NonTunnel_NonSecure_UnderflowResumed.yaml
│   ├── vts_l3_Sink_Concurrent_NonTunnel_NonSecure_VideoFlush.yaml
│   ├── vts_l3_Sink_Sequential_NonTunnel_NonSecure_DiscardFramesUntil.yaml
│   ├── vts_l3_Sink_Sequential_NonTunnel_NonSecure_EOS.yaml
│   ├── vts_l3_Sink_Sequential_NonTunnel_NonSecure_FramesDroppedFRC.yaml
│   ├── vts_l3_Sink_Sequential_NonTunnel_NonSecure_FramesDroppedLate.yaml
│   ├── vts_l3_Sink_Sequential_NonTunnel_NonSecure_FramesMiss.yaml
│   ├── vts_l3_Sink_Sequential_NonTunnel_NonSecure_FramesReceived.yaml
│   ├── vts_l3_Sink_Sequential_NonTunnel_NonSecure_FramesRepeatedFRC.yaml
│   ├── vts_l3_Sink_Sequential_NonTunnel_NonSecure_QueueDepth.yaml
│   ├── vts_l3_Sink_Sequential_NonTunnel_NonSecure_UnderflowResumed.yaml
│   ├── vts_l3_Sink_Sequential_NonTunnel_NonSecure_VideoFlush.yaml
│   ├── vts_l3_Sink_Testcase_Configurations.yaml
│   └── vts_l3_test_config.yaml
└── vts_videosink
```

## Run test

- Package the Binaries:

```bash
cd build
tar -zcf videosink_vts.tar.gz bin
```

- Transfer to Target Device:

Copy the tarball to the target device (e.g., using scp):

```bash
scp videosink_vts.tar.gz root@target-device:/home/root/

```

- Extract on Target Device:

Log into the target device and extract the tarball:

```bash
ssh root@target-device
cd /home/root
tar -zxf videosink_vts.tar.gz
cd bin
```

To execute `L3` test cases, elementary streams are required. Please copy all necessary test streams into the `bin/test_configuration` folder on the target device.

- Run the Test:

Execute the run.sh script as root: (Note: Disable logging to prevent delays and ensure smooth test execution. See "Disable Logging" under Build Steps for details.)

```bash
sudo ./run.sh -p profiles/hfp-videosink_vdevice.yaml
```

To run the binary with two processes simultaneously:

Open two separate SSH sessions to the target device. In each session, navigate to the `bin` directory  and execute one of the following commands:

```bash
 sudo ./run.sh -p profiles/hfp-videosink_vdevice_instance_1.yaml
```

```bash
 sudo ./run.sh -p profiles/hfp-videosink_vdevice_instance_2.yaml
```

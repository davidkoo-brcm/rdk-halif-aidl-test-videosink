L1 Testing Suite For HAL Video Sink
Build Steps
1. Build linux binder
For detail build instruction, please refer to https://github.com/rdk-e/linux_binder_idl/tree/main

Here outline the simplest steps are

git clone -b develop 'ssh://git@github.com/rdk-e/linux_binder_idl.git'

cde linux_binder_idl

./build-linux-binder-aidl.sh 64
2. Build this test
Using the following directory structure:

├── linux_binder_idl
└── rdk-halif-binder-test-videosink
    ├── build.sh
    ├── Makefile
    ├── profiles
    │   └── hfp-videosink.yaml
    ├── README.md
    ├── src
    │   ├── VideoSinkClient.cpp
    │   ├── yaml_utils.cpp
    │   └── yaml_utils.h
    └── vcomponent
    ├── Makefile
    └── src
        ├── VideoSink.cpp
        ├── VideoSink.h
        └── VideoSinkService.cpp

cd rdk-halif-binder-test-videosink

bash ./build.sh
Addition environment variables can be used:

AIDL_BIN
Path of the aidl compiler
LINUX_BINDER_OUT_DIR
Path of output that is installed after building linux_binder_idl, example contains bin/servicemanager, lib/libbinder etc
UT_CORE_VERSION
Specific version of ut-core to be used. Default using the latest tagged version (not necessary same as branch head).
Example:

UT_CORE_VERSION=4.3.0 AIDL_BIN=../linux_binder_idl/out/aidl LINUX_BINDER_OUT_DIR=../linux_binder_idl/local/ bash ./build.sh
There are test binaries and libraries, and binaries and libraries necessary for this test is located in build/bin

Example directory structure:

build/bin/
├── EmulateVideoSinkService
├── hal_test
├── lib
├── libbase.so
├── libbinder.so
├── libcutils.so
├── libcutils_sockets.so
├── liblog.so
├── libut_control.so
├── libutils.so
├── libvideosinkaidl.so
├── profiles
│   └── hfp-videosink.yaml
├── run.sh
└── servicemanager
Run test
cd build

tar -zcf videosink_l1.tar.gz bin

# scp videosink_l1.tar.gz into taraget device and extract it
cd bin

# Require root to access binder device
sudo bash ./run.sh

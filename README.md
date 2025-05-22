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

Addition environment variables can be used:

- AIDL_BIN: Path to the `AIDL` compiler (e.g., /path/to/linux_binder_idl/out/aidl).

- LINUX_BINDER_OUT_DIR: Path to the Linux binder output directory (e.g., /path/to/linux_binder_idl/local).

- UT_CORE_VERSION: Specific version of the UT-Core framework (e.g., 4.6.0). Defaults to the latest tagged version.

- RDK_HAL_VERSION: Specific version of the HAL Specification (e.g, 0.7.1). Defaults to the latest HEAD.

Example:

```bash
export UT_CORE_VERSION=4.6.0
export RDK_HAL_VERSION=0.7.1
export AIDL_BIN=/path/linux_binder_idl/out/aidl
export LINUX_BINDER_OUT_DIR=/path//linux_binder_idl/local/
```

Run the build script:

```bash
./build.sh
```

The build.sh script:

- Clones the `rdk-halif-aidl` repository and UT-Core framework.

- Compiles the test suite.

- Generates binaries in the `build/bin` directory.

Example directory structure :

```bash
build/bin/
├── aidl
├── EmulateVideoSInkService
├── lib
├── libbase.so
├── libbinder.so
├── libcutils.so
├── libcutils_sockets.so
├── liblog.so
├── libut_control.so
├── libutils.so
├── libvideosinkaidl.so
├── libvideodecoderaidl.so
├── profiles
│   └── hfp-videosink.yaml
├── run.sh
├── servicemanager
└── vts-videoSInk
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

- Run the Test:
Execute the run.sh script as root:

```bash
sudo ./run.sh -p profiles/hfp-videosink.yaml
```
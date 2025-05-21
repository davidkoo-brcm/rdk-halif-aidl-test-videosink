#* ******************************************************************************
#*  If not stated otherwise in this file or this component's LICENSE
#*  file the following copyright and licenses apply:
#*
#*  Copyright 2025 RDK Management
#*
#*  Licensed under the Apache License, Version 2.0 (the License);
#*  you may not use this file except in compliance with the License.
#*  You may obtain a copy of the License at
#*
#*  http://www.apache.org/licenses/LICENSE-2.0
#*
#*  Unless required by applicable law or agreed to in writing, software
#*  distributed under the License is distributed on an AS IS BASIS,
#*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#*  See the License for the specific language governing permissions and
#*  limitations under the License.
#*
#* ******************************************************************************

# Define the directory where the repository will be cloned
declare -r TOP=`dirname $(realpath $0)`
declare -r UT_CORE_DIR="${TOP}/ut/ut-core"
declare -r BUILD_DIR="${TOP}/build"
declare -r BUILD_INSTALL_PREFIX=${BUILD_DIR}/usr

DEF_LINUX_BINDER="`realpath ${TOP}/../`/linux_binder_idl"
# AIDL_BIN: path of aidl compiler
AIDL_BIN=${AIDL_BIN:-${DEF_LINUX_BINDER}/out/aidl}
# LINUX_BINDER_OUT_DIR: output installed after building linux_binder_idl
# contains bin/servicemanager, lib/libbinder etc
LINUX_BINDER_OUT_DIR=${LINUX_BINDER_OUT_DIR:-${DEF_LINUX_BINDER}/local}

mkdir -p $BUILD_DIR

if [ ! -x $AIDL_BIN ]; then
	echo "AIDL_BIN: path does not exists, consdier building the linux_binder_idl in $DEF_LINUX_BINDER"
	exit 1
fi
if [ ! -d $LINUX_BINDER_OUT_DIR ]; then
	echo "LINUX_BINDER_OUT_DIR: path does not exists, consider building the linux_binder_idl in $DEF_LINUX_BINDER"
	exit 1
fi

# Turning them into absolute path because we may calling change current working directory
# whiling running the script
declare -r AIDL_BIN=`realpath ${AIDL_BIN}`
declare -r LINUX_BINDER_OUT_DIR=`realpath ${LINUX_BINDER_OUT_DIR}`
: {$RDK_HAL_VERSION:="main"}

install_linux_binder()
{
	echo "Install linux-binder \`service manager, headers and libraries'"
	install -d ${BUILD_INSTALL_PREFIX}/bin ${BUILD_INSTALL_PREFIX}/lib ${BUILD_INSTALL_PREFIX}/include
	install -t ${BUILD_INSTALL_PREFIX}/bin ${LINUX_BINDER_OUT_DIR}/bin/*
	install -t ${BUILD_INSTALL_PREFIX}/lib ${LINUX_BINDER_OUT_DIR}/lib/*
	cp -apr ${LINUX_BINDER_OUT_DIR}/include/* ${BUILD_INSTALL_PREFIX}/include/
}


generate_videosink_hal()
{
    declare -r HAL_BUILD_DIR=${BUILD_DIR}/videosink_hal
    declare -r VERSION=current
    declare -r AIDL_GEN_DIR="${BUILD_DIR}/videosink_gen/$VERSION"
    declare -r RDK_HAL_DIR=${TOP}/rdk-halif-aidl

    # Clone the repository if not already present
    if [ ! -d "$RDK_HAL_DIR" ]; then
        echo "Checking out HAL version: " $RDK_HAL_VERSION
        git clone -b $RDK_HAL_VERSION 'ssh://git@github.com/rdkcentral/rdk-halif-aidl.git' $RDK_HAL_DIR
    fi

    for m in videosink  videodecoder; do
        cmake \
        -DAIDL_BIN=$AIDL_BIN \
        -DAIDL_GEN_DIR=$AIDL_GEN_DIR \
        -DAIDL_SRC_VERSION=$VERSION \
        -DAIDL_TARGET=$m  \
	-B ${HAL_BUILD_DIR}/gen/build/${m}_test \
        $RDK_HAL_DIR
    done

}


build_vcomponent()
{
    make -j`nproc` \
		-C ${TOP}/vcomponent
}

build_ut_core()
{
	if [ ! -d ${UT_CORE_DIR} ]; then
		git clone https://github.com/rdkcentral/ut-core.git ${UT_CORE_DIR}
		if [ $? -ne 0 ]; then
			echo "Failed to clone the repository. Exiting."
			exit 1
		fi
	fi

	pushd ${UT_CORE_DIR}

	git fetch -q -t origin
	# get the latest version of ut-core
	if [ -z "$UT_CORE_VERSION" ]; then
		UT_CORE_VERSION=`git tag | sort -Vr | head -n1`
	fi
	git checkout $UT_CORE_VERSION

	./build.sh TARGET=linux VARIANT=CPP
	if [ $? -ne 0 ]; then
		echo "Build script failed. Exiting."
		exit 1
	fi

	echo "Build completed successfully."
	popd
}

install_test()
{
	declare -r INSTALL_PATH=${BUILD_DIR}/bin

	install -t ${INSTALL_PATH} ${BUILD_INSTALL_PREFIX}/bin/*
	install -t ${INSTALL_PATH} ${BUILD_INSTALL_PREFIX}/lib/*
	install -t ${INSTALL_PATH} run.sh
	cp -r ${TOP}/profiles/ ${INSTALL_PATH}
}

install_linux_binder
generate_videosink_hal
build_vcomponent
build_ut_core
# Building test module using ut-core framework
make -j`nproc` -C ${TOP}
install_test

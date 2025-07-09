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
declare -r UT_CORE_DIR="${TOP}/ut-core"
declare -r BUILD_DIR="${TOP}/build"
declare -r BUILD_INSTALL_PREFIX=${BUILD_DIR}/usr
declare -r MOCK_SERVICE_DIR="${TOP}/mockservice"
declare -r AIDL_LIB_DIR=${TOP}/aidl_lib
declare -r RDK_HAL_DIR=${TOP}/rdk-halif-aidl
declare -r INSTALL_PATH=${BUILD_DIR}/bin

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
RDK_HALIF_AIDL_VERSION="${RDK_HALIF_AIDL_VERSION="main"}"


install_linux_binder()
{
    echo "Install linux-binder \`service manager, headers and libraries'"
    install -d ${BUILD_INSTALL_PREFIX}/bin ${BUILD_INSTALL_PREFIX}/lib ${BUILD_INSTALL_PREFIX}/include
    install -t ${BUILD_INSTALL_PREFIX}/bin ${LINUX_BINDER_OUT_DIR}/bin/*
    install -t ${BUILD_INSTALL_PREFIX}/lib ${LINUX_BINDER_OUT_DIR}/lib/*
    cp -apr ${LINUX_BINDER_OUT_DIR}/include/* ${BUILD_INSTALL_PREFIX}/include/
}

generate_audiodecoder_hal()
{
    declare -r HAL_BUILD_DIR=${BUILD_DIR}/audiodecoder_aidl
    declare -r AUDIODECODER_VERSION=current
    declare -r AIDL_GEN_DIR="${BUILD_DIR}/audiodecoder_gen/$AUDIODECODER_VERSION"
    declare -r RDK_HAL_DIR=${TOP}/rdk-halif-aidl

    # Clone the repository if not already present
    if [ ! -d "$RDK_HAL_DIR" ]; then
        git clone -b $RDK_HAL_VERSION 'ssh://git@github.com/rdkcentral/rdk-halif-aidl.git' $RDK_HAL_DIR
    fi

    # Prepare cmake build directories and configuration for each module
    cmake \
        -DAIDL_BIN=$AIDL_BIN \
        -DAIDL_GEN_DIR=$AIDL_GEN_DIR \
        -DAIDL_SRC_VERSION=$AUDIODECODER_VERSION \
        -DAIDL_TARGET=audiodecoder \
        -B ${HAL_BUILD_DIR}  \
        $RDK_HAL_DIR
}

generate_avbuffer_hal()
{
    declare -r HAL_BUILD_DIR=${BUILD_DIR}/avbuffer_hal
    declare -r VERSION=current
    declare -r AIDL_GEN_DIR="${BUILD_DIR}/avbuffer_gen/$VERSION"
    declare -r RDK_HAL_DIR=${TOP}/rdk-halif-aidl

    # Clone the repository if not already present
    if [ ! -d "$RDK_HAL_DIR" ]; then
        git clone -b $RDK_HAL_VERSION 'ssh://git@github.com/rdkcentral/rdk-halif-aidl.git' $RDK_HAL_DIR
    fi

    for m in avbuffer; do
        cmake \
        -DAIDL_BIN=$AIDL_BIN \
        -DAIDL_GEN_DIR=$AIDL_GEN_DIR \
        -DAIDL_SRC_VERSION=$VERSION \
        -DAIDL_TARGET=$m  \
        -B ${HAL_BUILD_DIR} \
        $RDK_HAL_DIR
    done
}

generate_videodecoder_hal()
{
    declare -r HAL_BUILD_DIR=${BUILD_DIR}/videodecoder_aidl
    declare -r VIDEODECODER_VERSION=current
    declare -r AIDL_GEN_DIR="${BUILD_DIR}/videodecoder_gen/$VIDEODECODER_VERSION"
    declare -r RDK_HAL_DIR=${TOP}/rdk-halif-aidl

    # Clone the repository if not already present
    if [ ! -d "$RDK_HAL_DIR" ]; then
        git clone -b $RDK_HAL_VERSION 'ssh://git@github.com/rdkcentral/rdk-halif-aidl.git' $RDK_HAL_DIR
    fi

    cmake \
        -DAIDL_BIN=$AIDL_BIN \
        -DAIDL_GEN_DIR=$AIDL_GEN_DIR \
        -DAIDL_SRC_VERSION=$VIDEODECODER_VERSION \
        -DAIDL_TARGET=videodecoder \
        -B ${HAL_BUILD_DIR} \
        $RDK_HAL_DIR

}

generate_videosink_hal()
{
    declare -r HAL_BUILD_DIR=${BUILD_DIR}/videosink_hal
    declare -r VERSION=current
    declare -r AIDL_GEN_DIR="${BUILD_DIR}/videosink_gen/$VERSION"
    declare -r RDK_HAL_DIR=${TOP}/rdk-halif-aidl

    # Clone the repository if not already present
    if [ ! -d "$RDK_HAL_DIR" ]; then
        echo "Checking out HAL version: " $RDK_HALIF_AIDL_VERSION
        git clone -b $RDK_HALIF_AIDL_VERSION 'http://git@github.com/rdkcentral/rdk-halif-aidl.git' $RDK_HAL_DIR
    fi

    # Build only videosink
    cmake \
        -DAIDL_BIN=$AIDL_BIN \
        -DAIDL_GEN_DIR=$AIDL_GEN_DIR \
        -DAIDL_SRC_VERSION=$VERSION \
        -DAIDL_TARGET=videosink  \
        -B ${HAL_BUILD_DIR} \
        $RDK_HAL_DIR
}

generate_avbufferhelper_skeleton()
{
    declare -r AVBUFFER_HELPER_DIR=${MOCK_SERVICE_DIR}/avbufferhelper

    mkdir -p $AVBUFFER_HELPER_DIR/build

    cd $AVBUFFER_HELPER_DIR/build

    cmake ..
    make

    cp libavbufferhelper.so ${BUILD_DIR}/usr/lib/

    cd -

    rm -rf $AVBUFFER_HELPER_DIR/build
}

build_aidl_libs()
{
    pushd ${AIDL_LIB_DIR}
    make -j`nproc`
    echo "AIDL Build completed successfully."
    popd
}

build_mockservice()
{
    make -j`nproc` \
        -C ${MOCK_SERVICE_DIR}
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

build_hal_log()
{
    declare -r INSTALL_PATH=${BUILD_DIR}/bin
    declare -r HAL_LOG_DIR="${TOP}/mockservice/hal_log"
    pushd ${HAL_LOG_DIR}
    make

    cp -f ${HAL_LOG_DIR}/libhal_logger.so ${INSTALL_PATH}/
    cp ${HAL_LOG_DIR}/libhal_logger.so ${BUILD_DIR}/usr/lib/
    make clean
    popd
}

install_test()
{
	install -t ${INSTALL_PATH} ${BUILD_INSTALL_PREFIX}/bin/*
	install -t ${INSTALL_PATH} ${BUILD_INSTALL_PREFIX}/lib/*
	install -t ${INSTALL_PATH} run.sh
	cp -r ${TOP}/profiles/ ${INSTALL_PATH}
    cp -rf ${TOP}/test_configuration ${INSTALL_PATH}
    mkdir -p ${INSTALL_PATH}/assets/streams/input
    mkdir -p ${INSTALL_PATH}/assets/streams/reference
    rm -rf ${INSTALL_PATH}/libavbufferhelper.so
}

dist_clean_test()
{
    rm -rf $BUILD_DIR
    rm -rf src/*.o
    rm -rf ${MOCK_SERVICE_DIR}/src/*.o
    rm -rf $RDK_HAL_DIR
    rm -rf ${UT_CORE_DIR}
}

clean_test()
{
    rm -rf $BUILD_DIR
    rm -rf src/*.o
    rm -rf ${MOCK_SERVICE_DIR}/src/*.o
}

help()
{
    # Colors
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    CYAN='\033[0;36m'
    BOLD='\033[1m'
    RESET='\033[0m'

    echo -e "${BOLD}${CYAN}Usage:${RESET}"

    echo -e "${BOLD}╔════════════════════════════════════════════════════════════════════╗${RESET}"
    echo -e "${BOLD}* ${GREEN}$0 all${RESET}            ${YELLOW}- Builds the test${RESET}                        ${BOLD}*${RESET}"
    echo -e "${BOLD}* ${GREEN}$0 clean${RESET}          ${YELLOW}- Cleans the build folder${RESET}                ${BOLD}*${RESET}"
    echo -e "${BOLD}* ${GREEN}$0 dist_clean${RESET}     ${YELLOW}- Deletes all build and checkout folders${RESET} ${BOLD}*${RESET}"
    echo -e "${BOLD}* ${GREEN}$0 help/-h/--help${RESET} ${YELLOW}- Prints this message${RESET}                    ${BOLD}*${RESET}"
    echo -e "${BOLD}╚════════════════════════════════════════════════════════════════════╝${RESET}"
}

if [ "$1" == "clean" ]; then
    clean_test
elif [ "$1" == "dist_clean" ]; then
    dist_clean_test
elif [[ "$1" == "help" || "$1" == "--help" || "$1" == "-h" ]]; then
    help
elif [ "$1" == "all" ]; then
    install_linux_binder
    generate_avbuffer_hal
    generate_audiodecoder_hal
    generate_videodecoder_hal
    generate_videosink_hal
    generate_avbufferhelper_skeleton
    build_aidl_libs
    build_mockservice
    build_ut_core
    build_hal_log
    # Building test module using ut-core framework
    make -j`nproc` -C ${TOP}
    install_test
else
    # Colors
    RED='\033[0;31m'
    BOLD='\033[1m'
    RESET='\033[0m'
    echo -e "${BOLD}${RED}Invalid Arguments${RESET}"
    help
fi
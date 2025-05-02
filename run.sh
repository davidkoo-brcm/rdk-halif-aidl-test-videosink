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

DEV_BINDER='/dev/binder'
RUN_DIR=`realpath $(dirname $0)`

# Function to check if a process is running
check_process() {
    local process_name=$1
    pgrep -f "$process_name" > /dev/null
    return $?
}

# Function to prompt user with yes/no question
prompt_yes_no() {
    local prompt=$1
    while true; do
        read -p "$prompt (y/n): " response
        case $response in
            [Yy]* ) return 0;;
            [Nn]* ) return 1;;
            * ) echo "Please answer y or n";;
        esac
    done
}

if [ ! -w $DEV_BINDER ]; then
	echo "No permission to access $DEV_BINDER, consider execute this as root"
	exit 1
fi

export LD_LIBRARY_PATH=$RUN_DIR:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/lib:/lib:/home/root:./

# Check and start servicemanager (must run if not running)
if check_process "servicemanager"; then
    echo "servicemanager is already running"
    servicemanager_pid=$(pgrep -f "servicemanager")
else
    echo "Execute [servicemanager]"
    ${RUN_DIR}/servicemanager &
    servicemanager_pid=$!
    # Wait for service to be ready
    sleep 2
fi

# Check if either EmulateVideoDecoderService or RDKVideoDecoderService is running
if check_process "EmulateVideoDecoderService" || check_process "RDKVideoDecoderService"; then
    echo "Video decoder service is already running"
    if check_process "EmulateVideoDecoderService"; then
        VideoDecoderService_pid=$(pgrep -f "EmulateVideoDecoderService")
    else
        VideoDecoderService_pid=$(pgrep -f "RDKVideoDecoderService")
    fi
else
    # Prompt user to start EmulateVideoDecoderService
    if prompt_yes_no "Do you want to start EmulateVideoDecoderService?"; then
        echo "Execute [EmulateVideoDecoderService]"
        ${RUN_DIR}/EmulateVideoDecoderService &
        VideoDecoderService_pid=$!
        # Wait for service to be ready
        sleep 2
    else
        # Prompt user to start RDKVideoDecoderService
        if prompt_yes_no "Do you want to start RDKVideoDecoderService instead?"; then
            echo "Execute [RDKVideoDecoderService]"
            RDKVideoDecoderService &
            VideoDecoderService_pid=$!
            # Wait for service to be ready
            sleep 2
        else
            echo "Cannot proceed without a video decoder service"
            # Clean up servicemanager
            if [ ! -z "$servicemanager_pid" ]; then
                kill -9 $servicemanager_pid
            fi
            exit 1
        fi
    fi
fi

echo "Eexecute [vts-videosink]"
${RUN_DIR}/vts-videosink -p profiles/hfp-videosink.yaml


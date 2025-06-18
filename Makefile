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


TARGET ?= linux
VARIANT := CPP

TARGET_EXEC := vts-videosink

ROOT_DIR := $(CURDIR)
SRC_DIRS := $(ROOT_DIR)/src
SRC_DIRS += $(ROOT_DIR)/src
SRC_DIRS += $(ROOT_DIR)/test
BIN_DIR := $(ROOT_DIR)/build/bin
LIB_DIR := $(ROOT_DIR)/build/$(TARGET)/lib
BUILD_DIR := $(ROOT_DIR)/build/$(TARGET)/obj

INC_DIRS := $(ROOT_DIR)/build/avbuffer_gen/current/h
INC_DIRS += $(ROOT_DIR)/rdk-halif-aidl/avbuffer/current
INC_DIRS += $(ROOT_DIR)/build/audiodecoder_gen/current/h
INC_DIRS += $(ROOT_DIR)/build/videodecoder_gen/current/h
INC_DIRS += $(ROOT_DIR)/build/videosink_gen/current/h
INC_DIRS += $(ROOT_DIR)/build/usr/include
INC_DIRS += $(ROOT_DIR)/src

XLDFLAGS := -L$(ROOT_DIR)/build/usr/lib
XLDFLAGS += -Wl,-rpath-link=$(ROOT_DIR)/build/usr/lib
XLDFLAGS += -lbinder -lutils
XLDFLAGS += -laudiodecoderaidl -lavbufferaidl -lvideodecoderaidl -lvideosinkaidl

# required for std::optional
CXXFLAGS := -std=c++17

UT_CORE_DIR := $(CURDIR)/ut/ut-core

export BIN_DIR
export LIB_DIR
export BUILD_DIR
export SRC_DIRS
export TARGET_EXEC
export INC_DIRS
export XLDFLAGS
export CXXFLAGS

.PHONY: all clean

all: $(TARGET_EXEC)

$(TARGET_EXEC):
	make -C $(UT_CORE_DIR) -j $(shell nproc) TARGET=$(TARGET) VARIANT=$(VARIANT)

clean:
	make -C $(UT_CORE_DIR) -j TARGET=$(TARGET) VARIANT=$(VARIANT) clean

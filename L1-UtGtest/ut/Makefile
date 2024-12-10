TARGET ?= linux
VARIANT := CPP

TARGET_EXEC := L1_videosink

ROOT_DIR := $(CURDIR)
SRC_DIRS := $(ROOT_DIR)/src
BIN_DIR := $(ROOT_DIR)/build/bin
LIB_DIR := $(ROOT_DIR)/build/$(TARGET)/lib
BUILD_DIR := $(ROOT_DIR)/build/$(TARGET)/obj
# TODO: make version is configurabe instead of current ???

INC_DIRS := $(ROOT_DIR)/build/videosink_gen/h
INC_DIRS += $(ROOT_DIR)/build/usr/include

XLDFLAGS := -L$(ROOT_DIR)/build/usr/lib
XLDFLAGS += -Wl,-rpath-link=$(ROOT_DIR)/build/usr/lib
XLDFLAGS += -lbinder -lutils 
XLDFLAGS += -lvideosinkaidl
XLDFLAGS += -lvideodecoderaidl

# required for std::optional
CXXFLAGS := -std=c++17


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
	make -C ut-core -j $(shell nproc) TARGET=$(TARGET) VARIANT=$(VARIANT)

clean:
	make -C ut-core -j TARGET=$(TARGET) VARIANT=$(VARIANT) clean


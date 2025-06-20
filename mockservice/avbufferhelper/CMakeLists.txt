cmake_minimum_required(VERSION 3.10)
project(IAVBufferHelperSkeleton)

# Use absolute paths
set(HEADER_FILE "${CMAKE_SOURCE_DIR}/IAVBufferHelper.h")
set(SKELETON_CPP "${CMAKE_BINARY_DIR}/IAVBufferHelper_skeleton.cpp")
set(SCRIPT_PATH "${CMAKE_SOURCE_DIR}/generate_skeleton.sh")

# Generate the skeleton .cpp file using full path to the script
add_custom_command(
    OUTPUT ${SKELETON_CPP}
    COMMAND ${CMAKE_COMMAND} -E echo "Running script: ${SCRIPT_PATH}"
    COMMAND ${CMAKE_COMMAND} -E env bash ${SCRIPT_PATH} ${HEADER_FILE} ${SKELETON_CPP}
    DEPENDS ${HEADER_FILE} ${SCRIPT_PATH}
    COMMENT "Generating skeleton implementation"
)

# Custom target to generate the file
add_custom_target(generate_skeleton DEPENDS ${SKELETON_CPP})

# Build the .so
add_library(avbufferhelper SHARED ${SKELETON_CPP})
add_dependencies(avbufferhelper generate_skeleton)
target_include_directories(avbufferhelper PUBLIC ${CMAKE_SOURCE_DIR})
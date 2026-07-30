cmake_minimum_required(VERSION 3.20)

# set the project name
project(resample_fixedpoint)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Same source built twice: once with the float path (default), once with the
# PREFER_FIXEDPOINT path, so the two outputs can be diffed for parity.
add_executable (resample_fixedpoint_float resample_fixedpoint.cpp)
target_compile_definitions(resample_fixedpoint_float PUBLIC -DIS_DESKTOP -DPREFER_FIXEDPOINT=false)
target_link_libraries(resample_fixedpoint_float arduino_emulator arduino-audio-tools)

add_executable (resample_fixedpoint_fixed resample_fixedpoint.cpp)
target_compile_definitions(resample_fixedpoint_fixed PUBLIC -DIS_DESKTOP -DPREFER_FIXEDPOINT=true)
target_link_libraries(resample_fixedpoint_fixed arduino_emulator arduino-audio-tools)

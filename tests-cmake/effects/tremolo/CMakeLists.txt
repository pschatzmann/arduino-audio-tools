cmake_minimum_required(VERSION 3.20)

# set the project name
project(tremolo)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Tremolo checks, built with the float and the PREFER_FIXEDPOINT configuration
add_executable (tremolo_float tremolo.cpp)
target_compile_definitions(tremolo_float PUBLIC -DIS_DESKTOP -DPREFER_FIXEDPOINT=false)
target_link_libraries(tremolo_float arduino_emulator arduino-audio-tools)

add_executable (tremolo_fixed tremolo.cpp)
target_compile_definitions(tremolo_fixed PUBLIC -DIS_DESKTOP -DPREFER_FIXEDPOINT=true)
target_link_libraries(tremolo_fixed arduino_emulator arduino-audio-tools)

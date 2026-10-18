cmake_minimum_required(VERSION 3.20)

# set the project name
project(mel)
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ldl -lpthread -lm")

# Emulator is not necessary for -DIS_MIN_DESKTOP
set(ADD_ARDUINO_EMULATOR OFF CACHE BOOL "Add Arduino Emulator Library")
set(ADD_PORTAUDIO OFF CACHE BOOL "No Portaudio")

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# build sketch as executable
add_executable (mel mel.cpp)

# set preprocessor defines
target_compile_definitions(mel PUBLIC -DIS_MIN_DESKTOP)

# specify libraries
target_link_libraries(mel arduino-audio-tools)

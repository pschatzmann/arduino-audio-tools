cmake_minimum_required(VERSION 3.20)

# set the project name
project(min-generator)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)

# Deactivate Emulator and Portaudio
set(ADD_ARDUINO_EMULATOR OFF CACHE BOOL "Add Arduino Emulator Library") 
set(ADD_PORTAUDIO OFF CACHE BOOL "Add Portaudio Library") 

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# build sketch as executable
set_source_files_properties(min-generator.ino PROPERTIES LANGUAGE CXX)
add_executable (min-generator min-generator.ino)

# set preprocessor defines
target_compile_definitions(min-generator PUBLIC -DIS_MIN_DESKTOP -DEXIT_ON_STOP)

# specify libraries
target_link_libraries(min-generator arduino-audio-tools)


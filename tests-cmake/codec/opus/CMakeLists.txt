cmake_minimum_required(VERSION 3.20)

# set the project name
project(opus)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Build with libmad 
FetchContent_Declare(arduino_libopus GIT_REPOSITORY "https://github.com/pschatzmann/arduino-libopus.git" GIT_TAG main )
FetchContent_GetProperties(arduino_libopus)
if(NOT arduino_libopus_POPULATED)
    FetchContent_Populate(arduino_libopus)
    add_subdirectory(${arduino_libopus_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/arduino_libopus)
endif()

# build sketch as executable
add_executable (opus opus.cpp)
# set preprocessor defines
target_compile_definitions(opus PUBLIC -DARDUINO  -DUSE_PORTAUDIO -DIS_DESKTOP )

# specify libraries
target_link_libraries(opus portaudio arduino_emulator arduino_libopus arduino-audio-tools )
# ESP32: CONFIG_ARDUINO_LOOP_STACK_SIZE 8192 -> so we test it with this setting "-Wl,-z,stack-size=8192"
# add_link_options("-z,stack-size=8192")


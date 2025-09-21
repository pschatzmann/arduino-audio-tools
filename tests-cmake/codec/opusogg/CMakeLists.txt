cmake_minimum_required(VERSION 3.20)

# set the project name
project(opusogg)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Build with libogg 
FetchContent_Declare(arduino_libogg GIT_REPOSITORY "https://github.com/pschatzmann/codec-ogg.git" GIT_TAG main )
FetchContent_GetProperties(arduino_libogg)
if(NOT arduino_libogg_POPULATED)
    FetchContent_Populate(arduino_libogg)
    # Need to provide both source and binary dirs because source is outside current tree
    add_subdirectory(${arduino_libogg_SOURCE_DIR} ${arduino_libogg_BINARY_DIR})
endif()

# Build with libopus 
FetchContent_Declare(arduino_libopus GIT_REPOSITORY "https://github.com/pschatzmann/codec-opus.git" GIT_TAG main )
FetchContent_GetProperties(arduino_libopus)
if(NOT arduino_libopus_POPULATED)
    FetchContent_Populate(arduino_libopus)
    # Provide explicit binary dir
    add_subdirectory(${arduino_libopus_SOURCE_DIR} ${arduino_libopus_BINARY_DIR})
endif()


# build sketch as executable
add_executable (opusogg opusogg.cpp)
# set preprocessor defines
target_compile_definitions(opusogg PUBLIC -DARDUINO  -DUSE_PORTAUDIO -DIS_DESKTOP )

# specify libraries
target_link_libraries(opusogg portaudio arduino_emulator arduino_libogg arduino_libopus arduino-audio-tools )
# ESP32: CONFIG_ARDUINO_LOOP_STACK_SIZE 8192 -> so we test it with this setting "-Wl,-z,stack-size=8192"
# add_link_options("-z,stack-size=8192")


cmake_minimum_required(VERSION 3.20)

# set the project name
project(aac-fdk)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()
include(FetchContent)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Build with arduino-fdk-aac
FetchContent_Declare(fdk_aac GIT_REPOSITORY "https://github.com/pschatzmann/arduino-fdk-aac.git" GIT_TAG main )
FetchContent_GetProperties(fdk_aac)
if(NOT fdk_aac_POPULATED)
    FetchContent_Populate(fdk_aac)
    add_subdirectory(${fdk_aac_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/fdk_aac)
endif()

# build sketch as executable
add_executable (aac-fdk aac-fdk.cpp )

# set preprocessor defines
target_compile_definitions(aac-fdk PUBLIC -DARDUINO -DEXIT_ON_STOP -DUSE_PORTAUDIO -DIS_DESKTOP)

# OS/X might need this setting for core audio
#target_compile_definitions(portaudio PUBLIC -DPA_USE_COREAUDIO=1)

# specify libraries
target_link_libraries(aac-fdk portaudio arduino_emulator fdk_aac arduino-audio-tools)


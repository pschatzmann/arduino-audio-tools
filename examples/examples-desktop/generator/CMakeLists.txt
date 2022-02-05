cmake_minimum_required(VERSION 3.20)

# set the project name
project(generator)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

# Build with Portaudio 
FetchContent_Declare(portaudio GIT_REPOSITORY "https://github.com/PortAudio/portaudio.git" GIT_TAG master )
FetchContent_GetProperties(portaudio)
if(NOT portaudio_POPULATED)
    FetchContent_Populate(portaudio)
    add_subdirectory(${portaudio_SOURCE_DIR})
endif()

# Build with Linux Arduino Emulator
FetchContent_Declare(arduino_emulator GIT_REPOSITORY "https://github.com/pschatzmann/Arduino-Emulator.git" GIT_TAG main )
FetchContent_GetProperties(arduino_emulator)
if(NOT arduino_emulator_POPULATED)
    FetchContent_Populate(arduino_emulator)
    add_subdirectory(${arduino_emulator_SOURCE_DIR})
endif()

# Build with arduino-audio-tools
FetchContent_Declare(arduino_audio_tools GIT_REPOSITORY "https://github.com/pschatzmann/arduino-audio-tools.git" GIT_TAG main )
FetchContent_GetProperties(arduino_audio_tools)
if(NOT arduino_audio_tools_POPULATED)
    FetchContent_Populate(arduino_audio_tools)
    add_subdirectory(${arduino_audio_tools_SOURCE_DIR})
endif()

# build sketch as executable
add_executable (generator generator.cpp)

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -©)

# OS/X might need this setting for core audio
#target_compile_definitions(portaudio PUBLIC -DPA_USE_COREAUDIO=1)

# specify libraries
target_link_libraries(generator portaudio_static arduino_emulator arduino-audio-tools)


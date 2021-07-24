cmake_minimum_required(VERSION 3.20)

# set the project name
project(desktop)
set (CMAKE_CXX_STANDARD 11)
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


# build sketch to executable
add_executable (desktop generator.cpp)

# Hmm...This should not be needed!
target_include_directories(desktop PRIVATE
   "${arduino_emulator_SOURCE_DIR}/ArduinoCore-API/api"
   "${arduino_emulator_SOURCE_DIR}/ArduinoCore-Linux/cores/arduino"
   "${arduino_audio_tools_SOURCE_DIR}/src"
)
# define dependencies
add_dependencies(desktop portaudio_static arduino_emulator arduino-audio-tools ) 

cmake_minimum_required(VERSION 3.0)

# set the project name
project(desktop)

# Build with arduino-audio-tools
FetchContent_Declare(arduino_audio_tools GIT_REPOSITORY "https://github.com/pschatzmann/arduino-audio-tools.git" GIT_TAG main )
FetchContent_GetProperties(arduino_audio_tools)
if(NOT arduino_audio_tools_POPULATED)
    FetchContent_Populate(arduino_audio_tools)
    add_subdirectory(${arduino_audio_tools_SOURCE_DIR})
endif()

# Build with Linux Arduino Emulator
FetchContent_Declare(arduino_emulator GIT_REPOSITORY "https://github.com/pschatzmann/Arduino-Emulator.git" GIT_TAG main )
FetchContent_GetProperties(arduino_emulator)
if(NOT arduino_emulator_POPULATED)
    FetchContent_Populate(arduino_emulator)
    add_subdirectory(${arduino_emulator_SOURCE_DIR})
endif()

# Build with Portaudio 
FetchContent_Declare(portaudio GIT_REPOSITORY "https://github.com/pschatzmann/Arduino-Emulator.git" GIT_TAG main )
FetchContent_GetProperties(portaudio)
if(NOT portaudio_POPULATED)
    FetchContent_Populate(portaudio)
    add_subdirectory(${portaudio_SOURCE_DIR})
endif()


# build sketch to executable
add_executable (desktop desktop_example.cpp)


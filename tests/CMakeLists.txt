cmake_minimum_required(VERSION 3.20)

# set the project name
project(tests)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)

# Build with Portaudio 
FetchContent_Declare(portaudio GIT_REPOSITORY "https://github.com/PortAudio/portaudio.git" GIT_TAG master )
FetchContent_GetProperties(portaudio)
if(NOT portaudio_POPULATED)
    FetchContent_Populate(portaudio)
    add_subdirectory(${portaudio_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/portaudio)
endif()

# Build with Linux Arduino Emulator
FetchContent_Declare(arduino_emulator GIT_REPOSITORY "https://github.com/pschatzmann/Arduino-Emulator.git" GIT_TAG main )
FetchContent_GetProperties(arduino_emulator)
if(NOT arduino_emulator_POPULATED)
    FetchContent_Populate(arduino_emulator)
    add_subdirectory(${arduino_emulator_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/emulator)
endif()


add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/mp3-mini ${CMAKE_CURRENT_BINARY_DIR}/mp3)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/mp3-helix ${CMAKE_CURRENT_BINARY_DIR}/mp3-helix)

# cmake missing in https://github.com/pschatzmann/arduino-fdk-aac.git
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/aac ${CMAKE_CURRENT_BINARY_DIR}/aac)


cmake_minimum_required(VERSION 3.20)

# set the project name
project(aac)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")
include(FetchContent)

# Build with arduino-fdk-aac
FetchContent_Declare(aac_decodr GIT_REPOSITORY "https://github.com/pschatzmann/arduino-fdk-aac.git" GIT_TAG main )
FetchContent_GetProperties(aac_decodr)
if(NOT aac_decodr_POPULATED)
    FetchContent_Populate(aac_decodr)
    add_subdirectory(${aac_decodr_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/aac_decodr)
endif()


# build sketch as executable
add_executable (aac aac.cpp)

# use main() from arduino_emulator
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN)
target_compile_definitions(aac PUBLIC -DEXIT_ON_STOP)

# OS/X might need this setting for core audio
#target_compile_definitions(portaudio PUBLIC -DPA_USE_COREAUDIO=1)

# specify libraries
target_link_libraries(aac portaudio_static arduino_emulator arduino-audio-tools aac_decodr)


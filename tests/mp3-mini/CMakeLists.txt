cmake_minimum_required(VERSION 3.20)

# set the project name
project(mp3-mini)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")


# build sketch as executable
add_executable (mp3-mini mp3.cpp)

# use main() from arduino_emulator
target_compile_definitions(mp3-mini PUBLIC -DEXIT_ON_STOP)

# OS/X might need this setting for core audio
#target_compile_definitions(portaudio PUBLIC -DPA_USE_COREAUDIO=1)

# specify libraries
target_link_libraries(mp3-mini portaudio_static arduino_emulator arduino-audio-tools)


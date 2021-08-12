cmake_minimum_required(VERSION 3.20)

# set the project name
project(mp3-mini)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()


# build sketch as executable
add_executable (mp3-mini mp3-mini.cpp)

# use main() from arduino_emulator
target_compile_definitions(mp3-mini PUBLIC -DEXIT_ON_STOP)

# OS/X might need this setting for core audio
#target_compile_definitions(portaudio PUBLIC -DPA_USE_COREAUDIO=1)

# specify libraries
target_link_libraries(mp3-mini portaudio arduino_emulator arduino-audio-tools)


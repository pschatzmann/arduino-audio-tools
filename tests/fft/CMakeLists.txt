cmake_minimum_required(VERSION 3.20)

# set the project name
project(fft)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()

# build sketch as executable
add_executable (fft fft.cpp)
# use main() from arduino_emulator
target_compile_definitions(fft PUBLIC -DARDUINO -DEXIT_ON_STOP)

# specify libraries
target_link_libraries(fft arduino_emulator arduino-audio-tools)


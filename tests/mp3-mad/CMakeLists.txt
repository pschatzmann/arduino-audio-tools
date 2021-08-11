cmake_minimum_required(VERSION 3.20)

# set the project name
project(mp3-mad)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

include(FetchContent)

# Build with Portaudio 
FetchContent_Declare(arduino_libmad GIT_REPOSITORY "https://github.com/pschatzmann/arduino-libmad.git" GIT_TAG main )
FetchContent_GetProperties(arduino_libmad)
if(NOT arduino_libmad_POPULATED)
    FetchContent_Populate(arduino_libmad)
    add_subdirectory(${arduino_libmad_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/arduino_libmad)
endif()

# build sketch as executable
add_executable (mp3-mad mp3-mad.cpp)
# use main() from arduino_emulator
target_compile_definitions(mp3-mad PUBLIC -DARDUINO -DEXIT_ON_STOP)

# specify libraries
target_link_libraries(mp3-mad portaudio_static arduino_emulator arduino_libmad arduino-audio-tools)


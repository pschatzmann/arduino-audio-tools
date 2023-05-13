cmake_minimum_required(VERSION 3.20)

# set the project name
project(mp3-lame)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")
#if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
#    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#endif()

include(FetchContent)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Build with liblame 
FetchContent_Declare(arduino_liblame GIT_REPOSITORY "https://github.com/pschatzmann/arduino-liblame.git" GIT_TAG main )
FetchContent_GetProperties(arduino_liblame)
if(NOT arduino_liblame_POPULATED)
    FetchContent_Populate(arduino_liblame)
    add_subdirectory(${arduino_liblame_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/arduino_liblame)
endif()

# build sketch as executable
add_executable (mp3-lame mp3-lame.cpp ../../main.cpp)
# set preprocessor defines
target_compile_definitions(mp3-lame PUBLIC -DARDUINO -DEXIT_ON_STOP -DUSE_PORTAUDIO -DIS_DESKTOP)

# specify libraries
target_link_libraries(mp3-lame portaudio arduino_emulator arduino_liblame arduino-audio-tools)


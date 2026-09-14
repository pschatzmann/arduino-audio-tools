cmake_minimum_required(VERSION 3.20)

# set the project name
project(rtsp-player)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")
# add_compile_options(-Wstack-usage=1024)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# build sketch as executable
add_executable (rtsp-player rtsp-player.cpp)

# set preprocessor defines
target_compile_definitions(rtsp-player PUBLIC -DIS_DESKTOP)

# specify libraries
target_link_libraries(rtsp-player arduino_emulator arduino-audio-tools)

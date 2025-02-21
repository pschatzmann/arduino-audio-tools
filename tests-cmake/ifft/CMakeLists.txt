cmake_minimum_required(VERSION 3.20)


# set the project name
project(ifft)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ldl -lpthread -lm")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -ldl -lpthread -lm")

# Emulator is not necessary for -DIS_MIN_DESKTOP
set(ADD_ARDUINO_EMULATOR OFF CACHE BOOL "Add Arduino Emulator Library") 
set(ADD_PORTAUDIO OFF CACHE BOOL "No Portaudio") 

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Download miniaudio.h
file(DOWNLOAD https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio.h)


# build sketch as executable
add_executable (ifft ifft.cpp)

# set preprocessor defines
target_compile_definitions(ifft PUBLIC -DIS_MIN_DESKTOP)

# specify libraries
target_link_libraries(ifft arduino-audio-tools)

# access to miniaudio in sketch directory
target_include_directories(ifft PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

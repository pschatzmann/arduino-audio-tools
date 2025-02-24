cmake_minimum_required(VERSION 3.20)


# set the project name
project(fft-effect)
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ldl -lpthread -lm -fno-omit-frame-pointer -fsanitize=address")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -ldl -lpthread -lm -fno-omit-frame-pointer -fsanitize=address")
set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

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
add_executable (fft-effect fft-effect.cpp)

# set preprocessor defines
target_compile_definitions(fft-effect PUBLIC -DIS_MIN_DESKTOP)

# specify libraries
target_link_libraries(fft-effect arduino-audio-tools)

# access to miniaudio in sketch directory
target_include_directories(fft-effect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

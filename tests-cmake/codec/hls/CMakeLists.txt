
cmake_minimum_required(VERSION 3.20)

# set the project name
project(hls)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)

# Activate Emulator and Portaudio
set(ADD_ARDUINO_EMULATOR ON CACHE BOOL "Add Arduino Emulator Library") 
set(ADD_PORTAUDIO OFF CACHE BOOL "Add Portaudio Library") 

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Build with tms
FetchContent_Declare(tsdemux GIT_REPOSITORY "https://github.com/pschatzmann/arduino-tsdemux"  )
FetchContent_GetProperties(tsdemux)
if(NOT tsdemux_POPULATED)
    FetchContent_Populate(tsdemux)
    add_subdirectory(${tsdemux_SOURCE_DIR})
endif()

# Build with libhelix
FetchContent_Declare(arduino_helix GIT_REPOSITORY "https://github.com/pschatzmann/arduino-libhelix.git" GIT_TAG development )
FetchContent_GetProperties(arduino_helix)
if(NOT arduino_helix_POPULATED)
    FetchContent_Populate(arduino_helix)
    add_subdirectory(${arduino_helix_SOURCE_DIR})
endif()

# Download miniaudio.h
file(DOWNLOAD https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio.h)

# build sketch as executable
set_source_files_properties(hls.ino PROPERTIES LANGUAGE CXX)
add_executable (hls hls.cpp )

# set preprocessor defines
target_compile_definitions(hls PUBLIC -DARDUINO -DIS_DESKTOP -DHELIX_PRINT)

# access to miniaudio in sketch directory
target_include_directories(hls PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# specify libraries
target_link_libraries(hls arduino-audio-tools arduino_emulator tsdemux arduino_helix)





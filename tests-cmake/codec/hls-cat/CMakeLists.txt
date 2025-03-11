
cmake_minimum_required(VERSION 3.20)

# set the project name
project(hls-cat)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)

# Add Portaduio for desktop build
add_compile_options(-DIS_DESKTOP)
FetchContent_Declare(portaudio GIT_REPOSITORY "https://github.com/PortAudio/portaudio.git" GIT_TAG v19.7.0 )
FetchContent_GetProperties(portaudio)
if(NOT portaudio_POPULATED)
    FetchContent_Populate(portaudio)
    add_subdirectory(${portaudio_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/portaudio)
endif()

# Activate Emulator and Portaudio
set(ADD_ARDUINO_EMULATOR ON CACHE BOOL "Add Arduino Emulator Library") 
set(ADD_PORTAUDIO ON CACHE BOOL "Add Portaudio Library") 

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
FetchContent_Declare(arduino_helix GIT_REPOSITORY "https://github.com/pschatzmann/arduino-libhelix.git" GIT_TAG main )
FetchContent_GetProperties(arduino_helix)
if(NOT arduino_helix_POPULATED)
    FetchContent_Populate(arduino_helix)
    add_subdirectory(${arduino_helix_SOURCE_DIR})
endif()


# build sketch as executable
set_source_files_properties(hls-cat.ino PROPERTIES LANGUAGE CXX)
add_executable (hls-cat hls-cat.cpp)

# set preprocessor defines
target_compile_definitions(hls-cat PUBLIC -DARDUINO -DIS_DESKTOP -DHELIX_PRINT)

# specify libraries
target_link_libraries(hls-cat arduino-audio-tools arduino_emulator tsdemux arduino_helix portaudio)





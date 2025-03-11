cmake_minimum_required(VERSION 3.20)

# set the project name
project(wav-test)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()

include(FetchContent)

# Add Portaduio for desktop build
add_compile_options(-DIS_DESKTOP)
FetchContent_Declare(portaudio GIT_REPOSITORY "https://github.com/PortAudio/portaudio.git" GIT_TAG v19.7.0 )
FetchContent_GetProperties(portaudio)
if(NOT portaudio_POPULATED)
    FetchContent_Populate(portaudio)
    add_subdirectory(${portaudio_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/portaudio)
endif()

# Build with arduino-adpcm
FetchContent_Declare(adpcm_ffmpeg GIT_REPOSITORY "https://github.com/pschatzmann/adpcm" GIT_TAG main )
FetchContent_GetProperties(adpcm_ffmpeg)
if(NOT adpcm_ffmpeg)
    FetchContent_Populate(adpcm_ffmpeg)
    add_subdirectory(${adpcm_ffmpeg_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/adpcm)
endif()

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# build sketch as executable
add_executable (wav-test wav.cpp)

# set preprocessor defines
target_compile_definitions(wav-test PUBLIC -DARDUINO -DUSE_PORTAUDIO -DIS_DESKTOP)

# specify libraries
target_link_libraries(wav-test portaudio arduino_emulator adpcm_ffmpeg arduino-audio-tools)


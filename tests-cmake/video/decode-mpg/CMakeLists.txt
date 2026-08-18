cmake_minimum_required(VERSION 3.20)

# set the project name
project(decode-mpg)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

# Add Portaudio for desktop audio output
add_compile_options(-DIS_DESKTOP)
FetchContent_Declare(portaudio GIT_REPOSITORY "https://github.com/PortAudio/portaudio.git" GIT_TAG v19.7.0 )
FetchContent_GetProperties(portaudio)
if(NOT portaudio_POPULATED)
    FetchContent_Populate(portaudio)
    add_subdirectory(${portaudio_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/portaudio)
endif()

# provide audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Build with libhelix - MP3DecoderHelix (AudioTools/AudioCodecs/CodecMP3Helix.h)
FetchContent_Declare(helix GIT_REPOSITORY "https://github.com/pschatzmann/arduino-libhelix.git" GIT_TAG main )
FetchContent_GetProperties(helix)
if(NOT helix_POPULATED)
    FetchContent_Populate(helix)
    add_subdirectory(${helix_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/helix)
endif()

# Build with TinyMPG - the MPEG-1 decoder MPGDecoder (AudioTools/Video/CodecMPG.h) wraps
FetchContent_Declare(tinympg GIT_REPOSITORY "https://github.com/pschatzmann/TinyMPG.git" GIT_TAG main )
FetchContent_GetProperties(tinympg)
if(NOT tinympg_POPULATED)
    FetchContent_Populate(tinympg)
    add_subdirectory(${tinympg_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/tinympg)
endif()

# Build with TinyMP2 - the MPEG-1 Layer II decoder MP2Decoder (AudioTools/AudioCodecs/CodecMP2.h) wraps
FetchContent_Declare(tinymp2 GIT_REPOSITORY "https://github.com/pschatzmann/TinyMP2.git" GIT_TAG main )
FetchContent_GetProperties(tinymp2)
if(NOT tinymp2_POPULATED)
    FetchContent_Populate(tinymp2)
    add_subdirectory(${tinymp2_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/tinymp2)
endif()

# OpenCV - display backend for OutputOpenCV
find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

# build sketch as executable
set_source_files_properties(decode-mpg.ino PROPERTIES LANGUAGE CXX)
add_executable (decode-mpg decode-mpg.ino)

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN)
target_compile_definitions(decode-mpg PUBLIC -DARDUINO -DUSE_PORTAUDIO -DIS_DESKTOP)
target_compile_options(decode-mpg PRIVATE -Wno-multichar)

# set compile optioins
target_compile_options(arduino-audio-tools INTERFACE -Wno-inconsistent-missing-override)

# specify libraries
target_link_libraries(decode-mpg PRIVATE
    arduino_emulator
    arduino-audio-tools
    TinyMPG::TinyMPG
    TinyMP2::TinyMP2
    arduino_helix
    portaudio
    ${OpenCV_LIBS})

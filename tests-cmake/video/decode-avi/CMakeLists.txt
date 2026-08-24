cmake_minimum_required(VERSION 3.20)

# set the project name
project(decode-avi)
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

# Build with TinyH264 - the H.264 decoder H264Decoder (AudioTools/Video/CodecH264.h) wraps
FetchContent_Declare(tinyh264 GIT_REPOSITORY "https://github.com/pschatzmann/TinyH264.git" GIT_TAG main )
FetchContent_GetProperties(tinyh264)
if(NOT tinyh264_POPULATED)
    FetchContent_Populate(tinyh264)
    add_subdirectory(${tinyh264_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/tinyh264)
endif()

# Build with TinyMPG - the MPEG-1 decoder MPGDecoder (AudioTools/Video/CodecMPG.h) wraps -
# MultiVideoDecoder (AudioTools/Video/MultiVideoDecoder.h) pulls in all three video
# codec decoders (H264Decoder/MPGDecoder/MJPEGDecoder) regardless of which one a
# given .avi file actually uses.
FetchContent_Declare(tinympg GIT_REPOSITORY "https://github.com/pschatzmann/TinyMPG.git" GIT_TAG main )
FetchContent_GetProperties(tinympg)
if(NOT tinympg_POPULATED)
    FetchContent_Populate(tinympg)
    add_subdirectory(${tinympg_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/tinympg)
endif()

# Build with TinyJPEG - the Motion-JPEG decoder MJPEGDecoder
# (AudioTools/Video/CodecJPEG.h) wraps. Native CMake support (header-only
# INTERFACE target, TinyJPEG::TinyJPEG) and no Arduino.h/SD.h dependency at
# all in the array-based decodeArray() path MJPEGDecoder actually uses, so
# unlike the JPEGDecoder library this replaced, no desktop-emulator
# workarounds are needed here.
FetchContent_Declare(tinyjpeg GIT_REPOSITORY "https://github.com/pschatzmann/TinyJPEG.git" GIT_TAG main )
FetchContent_GetProperties(tinyjpeg)
if(NOT tinyjpeg_POPULATED)
    FetchContent_Populate(tinyjpeg)
    add_subdirectory(${tinyjpeg_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/tinyjpeg)
endif()

# provide audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Build with libhelix - DecoderHelix (AudioTools/AudioCodecs/CodecHelix.h)
# bundles AACDecoderHelix/MP3DecoderHelix/WAVDecoder for the AVI audio track
FetchContent_Declare(helix GIT_REPOSITORY "https://github.com/pschatzmann/arduino-libhelix.git" GIT_TAG main )
FetchContent_GetProperties(helix)
if(NOT helix_POPULATED)
    FetchContent_Populate(helix)
    add_subdirectory(${helix_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/helix)
endif()

# OpenCV - display backend for OutputOpenCV
find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

# build sketch as executable
set_source_files_properties(decode-avi.ino PROPERTIES LANGUAGE CXX)
add_executable (decode-avi decode-avi.ino)

# decode-avi-player: same pipeline/test file as decode-avi, but driven
# through AudioTools/Video/VideoPlayer.h instead of wiring CodecCopy/
# MultiVideoDecoder/DecoderHelix/VideoAudioSyncTask by hand - exercises
# VideoPlayer end-to-end against a real .avi file.
set_source_files_properties(decode-avi-player.ino PROPERTIES LANGUAGE CXX)
add_executable (decode-avi-player decode-avi-player.ino)

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN)
target_compile_definitions(decode-avi PUBLIC -DARDUINO -DUSE_PORTAUDIO -DIS_DESKTOP)
target_compile_options(decode-avi PRIVATE -Wno-multichar)
target_compile_definitions(decode-avi-player PUBLIC -DARDUINO -DUSE_PORTAUDIO -DIS_DESKTOP)
target_compile_options(decode-avi-player PRIVATE -Wno-multichar)

# set compile optioins
target_compile_options(arduino-audio-tools INTERFACE -Wno-inconsistent-missing-override)

# specify libraries
target_link_libraries(decode-avi PRIVATE
    arduino_emulator
    arduino-audio-tools
    arduino_helix
    TinyH264::TinyH264
    TinyMPG::TinyMPG
    TinyJPEG::TinyJPEG
    portaudio
    ${OpenCV_LIBS})

target_link_libraries(decode-avi-player PRIVATE
    arduino_emulator
    arduino-audio-tools
    arduino_helix
    TinyH264::TinyH264
    TinyMPG::TinyMPG
    TinyJPEG::TinyJPEG
    portaudio
    ${OpenCV_LIBS})

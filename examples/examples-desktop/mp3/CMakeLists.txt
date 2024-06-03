cmake_minimum_required(VERSION 3.20)

# set the project name
project(mp3_dt)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Build with libhelix
FetchContent_Declare(arduino_helix GIT_REPOSITORY "https://github.com/pschatzmann/arduino-libhelix.git" GIT_TAG main )
FetchContent_GetProperties(arduino_helix)
if(NOT arduino_helix_POPULATED)
    FetchContent_Populate(arduino_helix)
    add_subdirectory(${arduino_helix_SOURCE_DIR})
endif()

# build sketch as executable
set_source_files_properties(mp3.ino PROPERTIES LANGUAGE CXX)
add_executable (mp3_dt mp3.ino)

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN)
target_compile_definitions(arduino_helix PUBLIC -DARDUINO -DHELIX_LOGGING_ACTIVE=0)
target_compile_definitions(mp3_dt PUBLIC -DIS_DESKTOP -DEXIT_ON_STOP)

# OS/X might need this setting for core audio
target_compile_options(portaudio PRIVATE -Wno-deprecated)

# specify libraries
target_link_libraries(mp3_dt portaudio arduino_emulator arduino-audio-tools arduino_helix)


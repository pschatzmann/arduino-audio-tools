cmake_minimum_required(VERSION 3.20)

# set the project name
project(mp3_mini)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

# provide minimp3
FetchContent_Declare(arduino_minimp3 GIT_REPOSITORY "https://github.com/pschatzmann/arduino-minimp3.git" )
FetchContent_GetProperties(arduino_minimp3)
if(NOT arduino_minimp3_POPULATED)
    FetchContent_Populate(arduino_minimp3)
    add_subdirectory(${arduino_minimp3_SOURCE_DIR})
endif()

# provide audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# build sketch as executable
set_source_files_properties(mp3-mini.ino PROPERTIES LANGUAGE CXX)
add_executable (mp3_mini mp3-mini.ino)

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN)
target_compile_definitions(arduino_minimp3 INTERFACE -DARDUINO -DMINIMP3_NO_SIMD)
target_compile_definitions(mp3_mini PUBLIC -DARDUINO -DIS_DESKTOP -DEXIT_ON_STOP)

# set compile optioins
target_compile_options(portaudio PRIVATE -Wno-deprecated -Wno-inconsistent-missing-override)
target_compile_options(arduino_minimp3 INTERFACE -Wdouble-promotion)
target_compile_options(arduino-audio-tools INTERFACE -Wno-inconsistent-missing-override)

# specify libraries
#target_link_libraries(arduino-audio-tools INTERFACE portaudio arduino_emulator arduino_minimp3 )
target_link_libraries(mp3_mini PRIVATE arduino_minimp3 portaudio arduino_emulator arduino-audio-tools )


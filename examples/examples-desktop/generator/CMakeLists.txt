cmake_minimum_required(VERSION 3.20)

# set the project name
project(generator)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# build sketch as executable
set_source_files_properties(generator.ino PROPERTIES LANGUAGE CXX)
add_executable (generator generator.ino)

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN)
target_compile_definitions(generator PUBLIC -DARDUINO -DIS_DESKTOP)

# specify libraries
target_link_libraries(generator portaudio arduino_emulator arduino-audio-tools)


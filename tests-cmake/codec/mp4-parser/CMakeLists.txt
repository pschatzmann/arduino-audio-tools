cmake_minimum_required(VERSION 3.20)

# set the project name
project(mp4-parser)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

# provide audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# build sketch as executable
set_source_files_properties(mp4-parser.ino PROPERTIES LANGUAGE CXX)
add_executable (mp4-parser mp4-parser.ino)

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN)
target_compile_definitions(mp4-parser PUBLIC -DARDUINO -DIS_DESKTOP)
target_compile_options(mp4-parser PRIVATE -Wno-multichar)


# set compile optioins
target_compile_options(arduino-audio-tools INTERFACE -Wno-inconsistent-missing-override)

# specify libraries
target_link_libraries(mp4-parser PRIVATE 
    arduino_emulator 
    arduino-audio-tools)
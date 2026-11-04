cmake_minimum_required(VERSION 3.20)

# set the project name
project(atrac)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

# Build with codec-atrac (its own tests are not needed)
set(ATRAC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(codec-atrac GIT_REPOSITORY "https://github.com/pschatzmann/codec-atrac.git" GIT_TAG main )
FetchContent_GetProperties(codec-atrac)
if(NOT codec-atrac_POPULATED)
    FetchContent_Populate(codec-atrac)
    add_subdirectory(${codec-atrac_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/codec-atrac)
endif()

# provide audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# build sketch as executable
set_source_files_properties(atrac.ino PROPERTIES LANGUAGE CXX)
add_executable (atrac atrac.ino)

# ATRAC3+ is disabled by default on Arduino
target_compile_definitions(codec-atrac PUBLIC -DATRAC_ENABLE_ATRAC3PLUS=1)

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN)
target_compile_definitions(atrac PUBLIC -DARDUINO -DIS_DESKTOP -DTEST_DATA_DIR="${codec-atrac_SOURCE_DIR}/test/data")
target_compile_options(atrac PRIVATE -Wno-multichar)

# set compile optioins
target_compile_options(arduino-audio-tools INTERFACE -Wno-inconsistent-missing-override)

# specify libraries
target_link_libraries(atrac PRIVATE codec-atrac arduino_emulator arduino-audio-tools )

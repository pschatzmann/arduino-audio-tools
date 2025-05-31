cmake_minimum_required(VERSION 3.20)

# set the project name
project(alac)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

# Build with arduino-fdk-aac
FetchContent_Declare(codec-alac GIT_REPOSITORY "https://github.com/pschatzmann/codec-alac.git" GIT_TAG main )
FetchContent_GetProperties(codec-alac)
if(NOT codec-alac_POPULATED)
    FetchContent_Populate(codec-alac)
    add_subdirectory(${codec-alac_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/codec-alac)
endif()

# provide audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# build sketch as executable
set_source_files_properties(alac.ino PROPERTIES LANGUAGE CXX)
add_executable (alac alac.ino)

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN)
target_compile_definitions(alac PUBLIC -DARDUINO -DIS_DESKTOP)
target_compile_options(alac PRIVATE -Wno-multichar)


# set compile optioins
target_compile_options(arduino-audio-tools INTERFACE -Wno-inconsistent-missing-override)

# specify libraries
target_link_libraries(alac PRIVATE codec-alac arduino_emulator arduino-audio-tools )


cmake_minimum_required(VERSION 3.20)

# set the project name
project(mts)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")

include(FetchContent)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)


# provide audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# build sketch as executable
set_source_files_properties(mts.ino PROPERTIES LANGUAGE CXX)
add_executable (mts mts.ino)

# set preprocessor defines
target_compile_definitions(mts PUBLIC -DARDUINO -DIS_DESKTOP)

target_compile_options(arduino-audio-tools INTERFACE -Wno-inconsistent-missing-override)

# specify libraries
target_link_libraries(mts PRIVATE  arduino_emulator arduino-audio-tools )


cmake_minimum_required(VERSION 3.20)

# set the project name
project(resample_ext)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")
# add_compile_options(-Wstack-usage=1024)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# build sketch as executable
add_executable (resample_ext resample_ext.cpp)

# set preprocessor defines
target_compile_definitions(resample_ext PUBLIC -DIS_DESKTOP)

# specify libraries
target_link_libraries(resample_ext arduino_emulator arduino-audio-tools)


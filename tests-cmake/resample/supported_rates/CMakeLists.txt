cmake_minimum_required(VERSION 3.20)

# set the project name
project(supported_rates)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# build sketch as executable
add_executable (supported_rates supported_rates.cpp)

# set preprocessor defines
target_compile_definitions(supported_rates PUBLIC -DIS_DESKTOP)

# specify libraries
target_link_libraries(supported_rates arduino_emulator arduino-audio-tools)

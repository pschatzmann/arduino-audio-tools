cmake_minimum_required(VERSION 3.20)

# set the project name
project(tests-filter)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror" )

include(FetchContent)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/filter ${CMAKE_CURRENT_BINARY_DIR}/filter)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/filter-wav ${CMAKE_CURRENT_BINARY_DIR}/filter-wav)

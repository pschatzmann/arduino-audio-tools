cmake_minimum_required(VERSION 3.20)

# set the project name
project(tests-effects)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror" )

include(FetchContent)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/effects ${CMAKE_CURRENT_BINARY_DIR}/effects)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/effects_fixedpoint ${CMAKE_CURRENT_BINARY_DIR}/effects_fixedpoint)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/effectsuite_fixedpoint ${CMAKE_CURRENT_BINARY_DIR}/effectsuite_fixedpoint)

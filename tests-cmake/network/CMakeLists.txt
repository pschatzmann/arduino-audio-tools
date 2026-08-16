cmake_minimum_required(VERSION 3.20)

# set the project name
project(tests-network)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror" )

include(FetchContent)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/url-test ${CMAKE_CURRENT_BINARY_DIR}/url-test)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/url-test-http ${CMAKE_CURRENT_BINARY_DIR}/url-test-http)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/rtsp)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/hdlc ${CMAKE_CURRENT_BINARY_DIR}/hdlc)

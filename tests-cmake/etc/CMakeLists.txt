cmake_minimum_required(VERSION 3.20)

# set the project name
project(tests-etc)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror" )

include(FetchContent)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/equalizer ${CMAKE_CURRENT_BINARY_DIR}/equalizer)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/generator ${CMAKE_CURRENT_BINARY_DIR}/generator)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/miniaudio ${CMAKE_CURRENT_BINARY_DIR}/miniaudio)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/pipeline)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/player-wav)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/stt ${CMAKE_CURRENT_BINARY_DIR}/stt)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/strview-matches ${CMAKE_CURRENT_BINARY_DIR}/strview-matches)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/audioplayer-mime-source-from-file ${CMAKE_CURRENT_BINARY_DIR}/audioplayer-mime-source-from-file)

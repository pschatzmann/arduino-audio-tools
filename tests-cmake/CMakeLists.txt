cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")

# set the project name
project(tests)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror" )
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()

include(FetchContent)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()


add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/resample ${CMAKE_CURRENT_BINARY_DIR}/resample)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/generator ${CMAKE_CURRENT_BINARY_DIR}/generator)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/effects ${CMAKE_CURRENT_BINARY_DIR}/effects)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/filter ${CMAKE_CURRENT_BINARY_DIR}/filter)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/filter-wav ${CMAKE_CURRENT_BINARY_DIR}/filter-wav)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/url-test ${CMAKE_CURRENT_BINARY_DIR}/url-test)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/codec)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/pipeline)

cmake_minimum_required(VERSION 3.0)

# set the project name
project(arduino-audio-tools)

add_library(arduino-audio-tools INTERFACE)

option(BUILD_TESTS "Build all tests automatically" OFF)


# make include directory available to calling projects 
target_include_directories (arduino-audio-tools INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# installation of all header files
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/" # source directory
    DESTINATION "include/arduino-audio-tools" # target directory
    FILES_MATCHING # install only matched files
    PATTERN "*.h" # select header files
)

if (BUILD_TESTS)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
else()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests EXCLUDE_FROM_ALL)
endif()

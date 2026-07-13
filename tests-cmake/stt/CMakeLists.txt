cmake_minimum_required(VERSION 3.20)

project(stt_test)
set(CMAKE_CXX_STANDARD 11)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

add_executable(stt_test stt_test.cpp)
target_compile_definitions(stt_test PUBLIC -DIS_DESKTOP_WITH_TIME_ONLY)
target_link_libraries(stt_test arduino-audio-tools)

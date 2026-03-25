cmake_minimum_required(VERSION 3.20)

project(converter-state)
set(CMAKE_CXX_STANDARD 11)

add_executable(converter-state converter-state.cpp)
target_compile_definitions(converter-state PUBLIC -DIS_DESKTOP_WITH_TIME_ONLY)
target_link_libraries(converter-state arduino-audio-tools)

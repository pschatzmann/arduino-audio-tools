cmake_minimum_required(VERSION 3.20)

project(channel-format)
set(CMAKE_CXX_STANDARD 11)

add_executable(channel-format channel-format.cpp)
target_compile_definitions(channel-format PUBLIC -DIS_DESKTOP_WITH_TIME_ONLY)
target_link_libraries(channel-format arduino-audio-tools)

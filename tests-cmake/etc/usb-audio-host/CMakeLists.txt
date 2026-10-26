cmake_minimum_required(VERSION 3.20)

project(usb-audio-host)
set(CMAKE_CXX_STANDARD 17)

add_executable(usb-audio-host usb-audio-host.cpp)
target_compile_definitions(usb-audio-host PUBLIC -DIS_DESKTOP_WITH_TIME_ONLY)
target_link_libraries(usb-audio-host arduino-audio-tools)

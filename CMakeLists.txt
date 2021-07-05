cmake_minimum_required(VERSION 3.0)

# set the project name
project(arduino-audio-tools)

add_library(arduino-audio-tools INTERFACE)

# make include directory available to calling projects 
target_include_directories (arduino-audio-tools INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

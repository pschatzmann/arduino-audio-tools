cmake_minimum_required(VERSION 3.0)

# set the project name
project(arduino-audio-tools)

# make include directory available to calling projects 
target_include_directories (MOZZI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

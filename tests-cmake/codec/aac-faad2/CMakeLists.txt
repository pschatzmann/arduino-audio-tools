cmake_minimum_required(VERSION 3.20)

# set the project name
project(aac-faad2)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS -Werror )
# add_compile_options(-Wstack-usage=1024)

include(FetchContent)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Build with codec-faad2
FetchContent_Declare(faad2 GIT_REPOSITORY https://github.com/pschatzmann/codec-faad2.git GIT_TAG main  )
FetchContent_GetProperties(faad2)
if(NOT faad2_POPULATED)
    FetchContent_Populate(faad2)
    add_subdirectory(${faad2_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/faad2)
endif()

# build sketch as executable
add_executable (aac-faad2 aac-faad2.cpp)

# set preprocessor defines
target_compile_definitions(aac-faad2 PUBLIC -DARDUINO  -DIS_DESKTOP)

# specify libraries
target_link_libraries(aac-faad2 arduino_emulator faad2 arduino-audio-tools)

cmake_minimum_required(VERSION 3.20)

# set the project name
project(aac-faac-encode)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")
# add_compile_options(-Wstack-usage=1024)

include(FetchContent)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Build with codec-faac
FetchContent_Declare(faac GIT_REPOSITORY "https://github.com/pschatzmann/codec-faac.git" GIT_TAG main )
FetchContent_GetProperties(faac)
if(NOT faac_POPULATED)
    FetchContent_Populate(faac)
    add_subdirectory(${faac_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/faac)
endif()

# build sketch as executable
add_executable (aac-faac-encode aac-faac-encode.cpp )
# set preprocessor defines
target_compile_definitions(aac-faac-encode PUBLIC -DARDUINO -DIS_DESKTOP)

# specify libraries
target_link_libraries(aac-faac-encode arduino_emulator faac arduino-audio-tools)

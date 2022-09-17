cmake_minimum_required(VERSION 3.20)

# set the project name
project(arduino_sketch)
set (SKETCH "streams-stk_allinstruments-audiokit.ino")
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

FetchContent_Declare(arduino-stk GIT_REPOSITORY "https://github.com/pschatzmann/arduino-stk.git" )
FetchContent_GetProperties(arduino-stk)
if(NOT arduino-stk_POPULATED)
    FetchContent_Populate(arduino-stk)
    add_subdirectory(${arduino-stk_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/arduino-stk)
endif()

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# build sketch as executable
set_source_files_properties(${SKETCH} PROPERTIES LANGUAGE CXX)
add_executable (arduino_sketch ${SKETCH})

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN)
target_compile_definitions(arduino_sketch PUBLIC -DARDUINO -DIS_DESKTOP -DEXIT_ON_STOP)
target_compile_definitions(arduino-stk PUBLIC -DIS_DESKTOP)

# OS/X might need this setting for core audio
target_compile_options(portaudio PRIVATE -Wno-deprecated)

# specify libraries
target_link_libraries(arduino_sketch arduino-stk portaudio arduino_emulator arduino-audio-tools )


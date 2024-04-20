cmake_minimum_required(VERSION 2.34)

# set the project name
project(aac-faad)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()
include(FetchContent)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Build with libfaad
FetchContent_Declare(libfaad GIT_REPOSITORY https://github.com/pschatzmann/arduino-libfaad.git GIT_TAG main  )
FetchContent_GetProperties(libfaad)
if(NOT libfaad_POPULATED)
    FetchContent_Populate(libfaad)
    add_subdirectory(${libfaad_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/libfaad)
endif()

# build sketch as executable
add_executable (aac-faad aac-faad.cpp)

# set preprocessor defines
target_compile_definitions(aac-faad PUBLIC -DARDUINO -DEXIT_ON_STOP -DUSE_PORTAUDIO -DIS_DESKTOP -DANALYSIS)

# OS/X might need this setting for core audio
#target_compile_definitions(portaudio PUBLIC -DPA_USE_COREAUDIO=1)

# specify libraries
target_link_libraries(aac-faad portaudio arduino_emulator libfaad arduino-audio-tools)


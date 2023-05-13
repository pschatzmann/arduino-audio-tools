cmake_minimum_required(VERSION 3.20)

# set the project name
project(adpcm)
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

# Build with arduino-adpcm-xq
FetchContent_Declare(adpcm-xq GIT_REPOSITORY "https://github.com/pschatzmann/arduino-adpcm-xq"  )
FetchContent_GetProperties(adpcm-xq)
if(NOT adpcm-xq)
    FetchContent_Populate(adpcm-xq)
    add_subdirectory(${adpcm-xq_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/adpcm-xq)
endif()

# build sketch as executable
add_executable (adpcm adpcm.cpp ../../main.cpp)

# set preprocessor defines
target_compile_definitions(adpcm PUBLIC -DARDUINO -DEXIT_ON_STOP -DUSE_PORTAUDIO -DIS_DESKTOP)

# OS/X might need this setting for core audio
#target_compile_definitions(portaudio PUBLIC -DPA_USE_COREAUDIO=1)

# specify libraries
target_link_libraries(adpcm portaudio arduino_emulator adpcm-xq arduino-audio-tools)


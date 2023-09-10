cmake_minimum_required(VERSION 3.16)

# set the project name
project(arduino-audio-tools)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
add_compile_options(-Wno-deprecated-declarations)

include(FetchContent)

add_library(arduino-audio-tools INTERFACE)

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(ADD_PORTAUDIO "Add Portaudio Library" ON)
option(ADD_ARDUINO_EMULATOR "Add Arduino Emulator Library" ON)


# make include directory available to calling projects 
target_include_directories (arduino-audio-tools INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# installation of all header files
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/" # source directory
    DESTINATION "include/arduino-audio-tools" # target directory
    FILES_MATCHING # install only matched files
    PATTERN "*.h" # select header files
)

if (ADD_PORTAUDIO)
    add_compile_options(-DIS_DESKTOP)
    # Add Portaduio for desktop build
    FetchContent_Declare(portaudio GIT_REPOSITORY "https://github.com/PortAudio/portaudio.git" GIT_TAG v19.7.0 )
    FetchContent_GetProperties(portaudio)
    if(NOT portaudio_POPULATED)
        FetchContent_Populate(portaudio)
        add_subdirectory(${portaudio_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/portaudio)
    endif()
endif()


if (ADD_ARDUINO_EMULATOR)
    # Build with Linux Arduino Emulator
    FetchContent_Declare(arduino_emulator GIT_REPOSITORY "https://github.com/pschatzmann/Arduino-Emulator.git" GIT_TAG main )
    FetchContent_GetProperties(arduino_emulator)
    if(NOT arduino_emulator_POPULATED)
        FetchContent_Populate(arduino_emulator)
        add_subdirectory(${arduino_emulator_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/emulator)
    endif()
endif()

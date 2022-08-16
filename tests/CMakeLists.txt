cmake_minimum_required(VERSION 3.20)

# set the project name
project(tests)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror" )
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()


include(FetchContent)

# Build with Portaudio 
FetchContent_Declare(portaudio GIT_REPOSITORY "https://github.com/PortAudio/portaudio.git" GIT_TAG v19.7.0 )
FetchContent_GetProperties(portaudio)
if(NOT portaudio_POPULATED)
    FetchContent_Populate(portaudio)
    add_subdirectory(${portaudio_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/portaudio)
endif()

# Build with Linux Arduino Emulator
FetchContent_Declare(arduino_emulator GIT_REPOSITORY "https://github.com/pschatzmann/Arduino-Emulator.git" GIT_TAG main )
FetchContent_GetProperties(arduino_emulator)
if(NOT arduino_emulator_POPULATED)
    FetchContent_Populate(arduino_emulator)
    add_subdirectory(${arduino_emulator_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/emulator)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/resample ${CMAKE_CURRENT_BINARY_DIR}/resample)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/generator ${CMAKE_CURRENT_BINARY_DIR}/generator)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/effects ${CMAKE_CURRENT_BINARY_DIR}/effects)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/filter ${CMAKE_CURRENT_BINARY_DIR}/filter)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/filter-wav ${CMAKE_CURRENT_BINARY_DIR}/filter-wav)
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/url-test ${CMAKE_CURRENT_BINARY_DIR}/url-test)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/codec)

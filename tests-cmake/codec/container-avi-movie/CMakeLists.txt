cmake_minimum_required(VERSION 3.20)

# set the project name
project(container-avi-movie)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()
include(FetchContent)

# Add Portaduio for desktop build
add_compile_options(-DIS_DESKTOP)
FetchContent_Declare(portaudio GIT_REPOSITORY "https://github.com/PortAudio/portaudio.git" GIT_TAG v19.7.0 )
FetchContent_GetProperties(portaudio)
if(NOT portaudio_POPULATED)
    FetchContent_Populate(portaudio)
    add_subdirectory(${portaudio_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/portaudio)
endif()

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# OpenCV
find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

# build sketch as executable
add_executable (container-avi-movie container-avi-movie.cpp)

# set preprocessor defines
target_compile_definitions(container-avi-movie PUBLIC -DUSE_PORTAUDIO -DIS_MIN_DESKTOP)
target_include_directories(container-avi-movie PRIVATE "${arduino_emulator_SOURCE_DIR}/ArduinoCore-Linux/libraries/SdFat" )

# OS/X might need this setting for core audio
#target_compile_definitions(portaudio PUBLIC -DPA_USE_COREAUDIO=1)

# specify libraries
target_link_libraries(container-avi-movie portaudio arduino-audio-tools  ${OpenCV_LIBS})


cmake_minimum_required(VERSION 3.20)

# set the project name
project(container-m4a-player)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

# Build with alac
FetchContent_Declare(codec-alac GIT_REPOSITORY "https://github.com/pschatzmann/codec-alac.git" GIT_TAG main )
FetchContent_GetProperties(codec-alac)
if(NOT codec-alac_POPULATED)
    FetchContent_Populate(codec-alac)
    add_subdirectory(${codec-alac_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/codec-alac)
endif()

# Build with libhelix
FetchContent_Declare(helix GIT_REPOSITORY "https://github.com/pschatzmann/arduino-libhelix.git" GIT_TAG main )
FetchContent_GetProperties(helix)
if(NOT helix_POPULATED)
    FetchContent_Populate(helix)
    add_subdirectory(${helix_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/helix)
endif()


# provide audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# build sketch as executable
set_source_files_properties(container-m4a-player.ino PROPERTIES LANGUAGE CXX)
add_executable (container-m4a-player container-m4a-player.ino)

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN -DUSE_FILESYSTEM)
target_compile_definitions(container-m4a-player PUBLIC -DARDUINO -DIS_DESKTOP)
target_compile_options(container-m4a-player PRIVATE -Wno-multichar)


# set compile optioins
target_compile_options(arduino-audio-tools INTERFACE -Wno-inconsistent-missing-override)
#target_compile_definitions(arduino-audio-tools INTERFACE -DUSE_ALLOCATOR)

# specify libraries
target_link_libraries(container-m4a-player PRIVATE 
    codec-alac
    arduino_emulator 
    arduino_helix 
    arduino-audio-tools)
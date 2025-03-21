cmake_minimum_required(VERSION 3.20)

# set the project name
project(tests-codec)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror" )
# add_compile_options(-Wstack-usage=1024)


include(FetchContent)

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()


add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/aac-faad ${CMAKE_CURRENT_BINARY_DIR}/aac-faad)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/aac-fdk ${CMAKE_CURRENT_BINARY_DIR}/aac-fdk)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/aac-fdk-encode ${CMAKE_CURRENT_BINARY_DIR}/aac-fdk-encode)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/mp3-helix ${CMAKE_CURRENT_BINARY_DIR}/mp3-helix)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/aac-helix ${CMAKE_CURRENT_BINARY_DIR}/aac-helix)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/mp3-lame ${CMAKE_CURRENT_BINARY_DIR}/mp3-lame)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/mp3-mad ${CMAKE_CURRENT_BINARY_DIR}/mp3-mad)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/mp3-metadata ${CMAKE_CURRENT_BINARY_DIR}/mp3-metadata)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/opus ${CMAKE_CURRENT_BINARY_DIR}/opus)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/opusogg ${CMAKE_CURRENT_BINARY_DIR}/opusogg)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/container-avi ${CMAKE_CURRENT_BINARY_DIR}/container-avi)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/hls ${CMAKE_CURRENT_BINARY_DIR}/hls)
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/container-avi-movie ${CMAKE_CURRENT_BINARY_DIR}/container-avi-movie)
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/container-m4a ${CMAKE_CURRENT_BINARY_DIR}/container-m4a)



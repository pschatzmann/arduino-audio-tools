cmake_minimum_required(VERSION 3.20)

# set the project name
project(effectsuite_fixedpoint)
set (CMAKE_CXX_STANDARD 14)
set (DCMAKE_CXX_FLAGS "-Werror")

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# AudioEffectsSuite.h's classes are templated on effectsuite_t, so both the
# float and soft_float_t instantiations are built and compared side by side
# in this one binary -- no separate PREFER_FIXEDPOINT=0/1 builds needed.
add_executable (effectsuite_fixedpoint effectsuite_fixedpoint.cpp)
target_compile_definitions(effectsuite_fixedpoint PUBLIC -DIS_DESKTOP)
target_link_libraries(effectsuite_fixedpoint arduino_emulator arduino-audio-tools)

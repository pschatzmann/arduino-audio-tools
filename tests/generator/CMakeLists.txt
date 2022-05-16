cmake_minimum_required(VERSION 3.20)

# set the project name
project(generator_test)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()


# build sketch as executable
add_executable (generator_test generator.cpp ../main.cpp)

# set preprocessor defines
target_compile_definitions(generator_test PUBLIC -DUSE_PORTAUDIO -DEXIT_ON_STOP -DIS_DESKTOP)

# specify libraries
target_link_libraries(generator_test portaudio arduino_emulator arduino-audio-tools)


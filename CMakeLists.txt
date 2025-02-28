# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

project(vpkextractor VERSION 1.0)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

include_directories(include/)

file(GLOB SRC_FILES src/*.c)

add_executable(vpkextractor ${SRC_FILES})

target_link_libraries(vpkextractor ${CMAKE_SOURCE_DIR}/lib/libvpk_static.lib)
cmake_minimum_required(VERSION 3.23)

project(tempCacheDb C)
set(CMAKE_BUILD_TYPE Debug)

add_subdirectory (src)

enable_testing()
add_subdirectory (test)
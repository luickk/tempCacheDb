cmake_minimum_required(VERSION 3.0)

project(tempCacheDb C)

add_library(tempCacheDb src/tempCacheDb.c)
target_include_directories(tempCacheDb PUBLIC src/include)
target_link_libraries(tempCacheDb PRIVATE -lpthread)

set(CMAKE_BUILD_TYPE Debug)

add_subdirectory(test/)

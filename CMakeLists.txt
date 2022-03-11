cmake_minimum_required(VERSION 3.0)

project(tempCacheDb C)

add_library(tempCacheDb src/tempCacheDb.c)
target_include_directories(tempCacheDb PUBLIC src/include)
target_link_libraries(tempCacheDb PRIVATE -lpthread)

add_subdirectory(test/)

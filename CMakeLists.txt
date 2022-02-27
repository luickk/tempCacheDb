cmake_minimum_required(VERSION 3.0)

project(tempCacheDb C)

add_library(tempCacheDb src/tempCacheDb.c)
target_include_directories(tempCacheDb PUBLIC src/include)

add_subdirectory(test/)

cmake_minimum_required(VERSION 3.0)

project(tempCacheDb C)

add_library(tempCacheDb INTERFACE)

target_include_directories(tempCacheDb INTERFACE .)

add_subdirectory(test/)

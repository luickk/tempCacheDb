cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(tempCacheDbTest C CXX)

add_executable(tempCacheDbTest test.c)

target_link_libraries(tempCacheDbTest INTERFACE tempCacheDb)

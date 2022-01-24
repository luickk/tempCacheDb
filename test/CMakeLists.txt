cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(tempCacheDbTest C CXX)

add_executable(tempCacheDbClientTest testClient.c)

target_link_libraries(tempCacheDbClientTest INTERFACE tempCacheDb)

add_executable(tempCacheDbServerTest testServer.c)

target_link_libraries(tempCacheDbServerTest INTERFACE tempCacheDb)

add_executable(tempCacheDbLocalTest localTest.c)

target_link_libraries(tempCacheDbLocalTest INTERFACE tempCacheDb)

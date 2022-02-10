cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(tempCacheDbTest C CXX)

add_executable(testPullClient testPullClient.c)
target_link_libraries(testPullClient INTERFACE tempCacheDb)

add_executable(testPushClient testPushClient.c)
target_link_libraries(testPushClient INTERFACE tempCacheDb)

add_executable(testServerAndLocal testServerAndLocal.c)
target_link_libraries(testServerAndLocal INTERFACE tempCacheDb)

add_executable(testServer testServer.c)
target_link_libraries(testServer INTERFACE tempCacheDb)

add_executable(localTest localTest.c)
target_link_libraries(localTest INTERFACE tempCacheDb)

add_executable(testPushPullClient testPushPullClient.c)
target_link_libraries(testPushPullClient INTERFACE tempCacheDb)

project(tempCacheDbTest C CXX)

add_executable(testPullClient testPullClient.c)
target_link_libraries(testPullClient PUBLIC tempCacheDb)
target_include_directories(testPullClient PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/tempCacheDb"
                          )

add_executable(testPushClient testPushClient.c)
target_link_libraries(testPushClient PUBLIC tempCacheDb)
target_include_directories(testPushClient PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/tempCacheDb"
                          )

add_executable(testServerAndLocal testServerAndLocal.c)
target_link_libraries(testServerAndLocal PUBLIC tempCacheDb)
target_include_directories(testServerAndLocal PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/tempCacheDb"
                          )

add_executable(testServer testServer.c)
target_link_libraries(testServer PUBLIC tempCacheDb)
target_include_directories(testServer PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/tempCacheDb"
                          )

add_executable(localTest localTest.c)
target_link_libraries(localTest PUBLIC tempCacheDb)
target_include_directories(localTest PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/tempCacheDb"
                          )

add_executable(testPushPullClient testPushPullClient.c)
target_link_libraries(testPushPullClient PUBLIC tempCacheDb)
target_include_directories(testPushPullClient PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/tempCacheDb"
                          )

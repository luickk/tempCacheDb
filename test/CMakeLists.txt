add_executable (testPullClient testPullClient.c)
add_test(NAME testPullClient COMMAND testPullClient)
target_link_libraries (testPullClient lTempCacheDb)

add_executable (testPushClient testPushClient.c)
add_test(NAME testPushClient COMMAND testPushClient)
target_link_libraries (testPushClient lTempCacheDb)

add_executable (testPushPullClient testPushPullClient.c)
add_test(NAME testPushPullClient COMMAND testPushPullClient)
target_link_libraries (testPushPullClient lTempCacheDb)

add_executable (localTest localTest.c)                 
add_test(NAME teslocalTestt COMMAND localTest)       
target_link_libraries (localTest lTempCacheDb)


add_executable (benchPull benchPull.c)               
target_link_libraries (benchPull lTempCacheDb)

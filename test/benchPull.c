#include <stdio.h>
#include <stdlib.h>

#include "tempCacheDb.h"
#include "utils.c"

int main() {
  tempCache *cache1;
  int err = setupTestServer(&cache1, 8081);
  if (err != 0) {
    return err;
  }

  cacheObject *insert2;
  err = initCacheObject(&insert2);
  if (err != 0) {
    return err;
  }
  insert2->key = "test";
  insert2->keySize = 4;
  insert2->val = "testVal6";
  insert2->valSize = 8;

  pushCacheObject(cache1->localCache, insert2, NULL);

  tempCacheClient *cacheClient;
  err = initCacheClient(&cacheClient);
  if (err != 0) {
    printf("cClientInit err code %d \n", err);
    return err;
  }

  err = cacheClientConnect(cacheClient, "127.0.0.1", 8081);
  if (err != 0) {
    printf("cClientConnect err code %d \n", err);
    return err;
  }
  printf("connected successfully \n");

  cacheObject *pulledCo;
  double startTime, endTime;
  
  startTime = (float)clock()/CLOCKS_PER_SEC;
  err = cacheClientPullCacheObject(cacheClient, insert2->key, insert2->keySize, &pulledCo);
  endTime = (float)clock()/CLOCKS_PER_SEC;

  if (err != 0) {
    printf("cacheClientPushO err code %d \n", err);
    return err;
  }
  printf("(query) k: %.*s v: %.*s \n", pulledCo->keySize, (char*)pulledCo->key, pulledCo->valSize, (char*)pulledCo->val);
  printf("took %f seconds \n", endTime-startTime);
  

  freeCoFn(pulledCo);
  err = cacheClientCloseConn(cacheClient);
  if (err != 0) {
    return err;
  }

  // waiting to properly close socket
  pthread_join(cache1->pthread, NULL);

  printf("test successfull \n");
  return 0;
}

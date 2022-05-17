#include <stdio.h>
#include <stdlib.h>

#include "tempCacheDb.h"
#include "utils.c"

void *push(void *argss) {
  tempCacheClient *cacheClient = (tempCacheClient*) argss;

  cacheObject *insert2;
  int err = initCacheObject(&insert2);
  if (err != 0) {
    printf("push thread err code %d \n", err);
  }
  insert2->key = "test";
  insert2->keySize = 4;
  insert2->val = "testVal6";
  insert2->valSize = 8;

  char *r = malloc(sizeof(int));
  insert2->keySize = sizeof(int)+7;
  insert2->key = r;
  for (int i = 0; i <= 100; i++) {
    sprintf(r, "peter%d", i);
    printf("%s \n", (char*)insert2->key);
    err = cacheClientPushCacheObject(cacheClient, insert2);
    if (err != 0) {
      printf("cacheClientPushO err code %d \n", err);
    }
    usleep(1000);
  }
  pthread_exit(NULL);
}

int main() {
  tempCache *cache1;
  int err = setupTestServer(&cache1, 8082);
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

  err = pushCacheObject(cache1->localCache, insert2, NULL);
  if (err != 0) {
    return err;
  }

  tempCacheClient *cacheClient;
  err = initCacheClient(&cacheClient);
  if (err != 0) {
    printf("cClientInit err code %d \n", err);
    return err;
  }

  err = cacheClientConnect(cacheClient, "127.0.0.1", 8082);
  if (err != 0) {
    printf("cClientConnect err code %d \n", err);
    return err;
  }
  printf("connected successfully \n");
  pthread_t pthread = 0;

  if(pthread_create(&pthread, NULL, push, (void*)cacheClient) != 0 ) {
    return errIO;
  }

  cacheObject *pulledCo;
  err = initCacheObject(&pulledCo);
  if (err != 0) {
    return err;
  }
  for (int i = 0; i <= 100; i++) {
    err = cacheClientPullCacheObject(cacheClient, insert2->key, insert2->keySize, &pulledCo);
    if (err != 0) {
      printf("cacheClientPullCacheObject err code %d \n", err);
      return err;
    }

    printf("(query) k: %.*s v: %.*s \n", pulledCo->keySize, (char*)pulledCo->key, pulledCo->valSize, (char*)pulledCo->val);
    usleep(1000);
  }

  // waiting to properly end pull test (usually ends before pull completes)
  // pthread_join(pthread, NULL);

  err = cacheClientCloseConn(cacheClient);
  if (err != 0) {
    return err;
  }

  // waiting to properly close socket
  pthread_join(cache1->pthread, NULL);

  printf("test successfull\n");
  return 0;
}

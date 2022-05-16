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
  int i = 0;
  while (1) {
    sprintf(r, "peter%d", i++);
    printf("%s \n", (char*)insert2->key);
    err = cacheClientPushCacheObject(cacheClient, insert2);
    if (err != 0) {
      printf("cacheClientPushO err code %d \n", err);
    }
    usleep(1000);
  }
}

int main() {
  int err = setupTestServer(NULL);
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

  tempCacheClient *cacheClient;
  err = initCacheClient(&cacheClient);
  if (err != 0) {
    printf("cClientInit err code %d \n", err);
    return 1;
  }

  err = cacheClientConnect(cacheClient, "127.0.0.1", 8080);
  if (err != 0) {
    printf("cClientConnect err code %d \n", err);
    return 1;
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
      return 1;
    }

    printf("(query) k: %.*s v: %.*s \n", pulledCo->keySize, (char*)pulledCo->key, pulledCo->valSize, (char*)pulledCo->val);
    usleep(1000);
  }

  return 0;
}

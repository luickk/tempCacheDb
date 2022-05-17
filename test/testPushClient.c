#include <stdio.h>
#include <stdlib.h>

#include "tempCacheDb.h"
#include "utils.c"

int main() {
  tempCache *cache1;
  int err = setupTestServer(&cache1, 8080);
  if (err != 0) {
    return err;
  }

  cacheObject *insert2;
  err = initCacheObject(&insert2);
  if (err != 0) {
    return err;
  }
  insert2->key = "peter2";
  insert2->keySize = 6;
  insert2->val = "testVal2";
  insert2->valSize = 8;

  tempCacheClient *cacheClient;
  err = initCacheClient(&cacheClient);
  if (err != 0) {
    printf("cClientInit err code %d \n", err);
    return err;
  }

  err = cacheClientConnect(cacheClient, "127.0.0.1", 8080);
  if (err != 0) {
    printf("cClientConnect err code %d \n", err);
    return err;
  }
  printf("connected successfully \n");

  char *r = malloc(sizeof(int));
  insert2->keySize = sizeof(int)+7;
  insert2->key = r;
  for (int i = 0; i <= 100; i++) {
    sprintf(r, "peter%d", i);
    // printf("%s \n", (char*)insert2->key);
    err = cacheClientPushCacheObject(cacheClient, insert2);
    if (err != 0) {
      printf("cacheClientPushO err code %d \n", err);
      return err;
    }
    usleep(100);
  }

  err = cacheClientCloseConn(cacheClient);
  if (err != 0) {
    return err;
  }

  // waiting to properly close socket
  pthread_join(cache1->pthread, NULL);
  printf("test successfull\n");
  return 0;
}

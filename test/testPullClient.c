#include <stdio.h>
#include <stdlib.h>

#include "../tempCacheDb.h"

int strKeyCmp(void *key1, void *key2, int size) {

  /* does not work with void pointers? returns always 0 */
  // int t = strncmp(key1, key2, size);

  char *ckey1 = (char*)key1;
  char *ckey2 = (char*)key2;
  for (int i = 0; i <= size; i++) {
    if (ckey1[i]!=ckey2[i]) {
      return 0;
    }
  }
  return 1;
}

void freeCoFn(cacheObject *cO) {
  // for this example we only need to free the cacheObject struct because the key/val are string literals and cannot be freed
  free(cO);
}

void printCache(tempCache *cache) {
  for (int i = 0; i < cache->localCache->nCacheSize; i++) {
    printf("p: %p row %d - k: %s v: %s \n", cache->localCache->keyValStore[i], i, (char*)cache->localCache->keyValStore[i]->key, (char*)cache->localCache->keyValStore[i]->val);
  }
}

int main() {
  cacheObject *insert2 = malloc(sizeof(cacheObject));
  insert2->key = "test";
  insert2->keySize = 4;
  insert2->val = "testVal6";
  insert2->valSize = 8;

  tempCacheClient *cacheClient;
  int err = initCacheClient(&cacheClient);
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

  cacheObject *pulledCo = malloc(sizeof(cacheObject));
  while (1) {
    err = cacheClientPullByKey(cacheClient, insert2->key, insert2->keySize, &pulledCo);
    if (err != 0) {
      printf("cacheClientPushO err code %d \n", err);
      return 1;
    }

    printf("(query) k: %.*s v: %.*s \n", pulledCo->keySize, (char*)pulledCo->key, pulledCo->valSize, (char*)pulledCo->val);
    usleep(100000);
  }

  return 0;
}

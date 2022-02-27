#include <stdio.h>

#include "tempCacheDb.h"

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
    printf("row %d - k: %s v: %s \n", i, (char*)cache->localCache->keyValStore[i]->key, (char*)cache->localCache->keyValStore[i]->val);
  }
}

int main() {
  tempCache *cache1;
  int err = initTempCache(&cache1, strKeyCmp, freeCoFn);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
  }

  err = listenDb(cache1, 8080);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
  }

  return 0;
}

#include <stdio.h>

#include "../tempCacheDb.h"

int strKeyCmp(void *key1, void *key2, int size) {
  return strncmp(key1, key2, size);
}

void printCache(tempCache *cache) {
  for (int i = 0; i < cache->nSize; i++) {
    printf("row %d - k: %s v: %s \n", i, (char*)cache->keyValStore[i]->key, (char*)cache->keyValStore[i]->val);
  }
}

int main() {
  tempCache *cache1;

  int err = initTempCache(cache1, "cache1", strKeyCmp);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
  }

  cacheObject *insert = malloc(sizeof(cacheObject));
  insert->key = "test";
  insert->keySize = 4;

  insert->val = "testVal /n";
  insert->valSize = 9;

  err = genericPushToCache(cache1, insert);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
  }

  cacheObject *insert2 = malloc(sizeof(cacheObject));
  insert->key = "peter";
  insert->keySize = 4;

  insert->val = "testVal2 /n";
  insert->valSize = 9;

  err = genericPushToCache(cache1, insert2);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
  }

  printCache(cache1);

  return 0;
}

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
  for (int i = 0; i < cache->nCacheSize; i++) {
    printf("p: %p row %d - k: %s v: %s \n", cache->keyValStore[i], i, (char*)cache->keyValStore[i]->key, (char*)cache->keyValStore[i]->val);
  }
}

int main() {
  tempCache *cache1;

  int err = initTempCache(&cache1, strKeyCmp, freeCoFn);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
  }

  cacheObject *insert = malloc(sizeof(cacheObject));
  insert->key = "test";
  insert->keySize = 4;
  insert->val = "testVal";
  insert->valSize = 6;
  err = genericPushToCache(cache1, insert);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
  }

  cacheObject *insert2 = malloc(sizeof(cacheObject));
  insert2->key = "peter2";
  insert2->keySize = 6;
  insert2->val = "testVal2";
  insert2->valSize = 8;
  err = genericPushToCache(cache1, insert2);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
  }

  cacheObject *insert3 = malloc(sizeof(cacheObject));

  insert3->key = "peterr";
  insert3->keySize = 6;
  insert3->val = "testVal3";
  insert3->valSize = 8;
  err = genericPushToCache(cache1, insert3);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
  }


  cacheObject *insert4 = malloc(sizeof(cacheObject));
  insert4->key = "peterr";
  insert4->keySize = 6;
  insert4->val = "rrrr";
  insert4->valSize = 4;
  err = genericPushToCache(cache1, insert4);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
  }

  printCache(cache1);

  return 0;
}

#include <stdio.h>
#include <stdlib.h>

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
    printf("p: %p row %d - k: %s v: %s \n", cache->localCache->keyValStore[i], i, (char*)cache->localCache->keyValStore[i]->key, (char*)cache->localCache->keyValStore[i]->val);
  }
}

int main() {
  tempCache *cache1;

  int err = initTempCache(&cache1, strKeyCmp, freeCoFn);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
  }

  cacheObject *insert;
  err = initCacheObject(&insert);
  if (err != 0) {
    return err;
  }
  insert->key = "test";
  insert->keySize = 4;
  insert->val = "testVal";
  insert->valSize = 6;
  err = genericPushToCache(cache1->localCache, insert, NULL);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
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
  err = genericPushToCache(cache1->localCache, insert2, NULL);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
  }


  cacheObject *insert3;
  err = initCacheObject(&insert3);
  if (err != 0) {
    return err;
  }
  insert3->key = "peterr";
  insert3->keySize = 6;
  insert3->val = "testVal3";
  insert3->valSize = 8;
  err = genericPushToCache(cache1->localCache, insert3, NULL);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
  }

  cacheObject *insert4;
  err = initCacheObject(&insert4);
  if (err != 0) {
    return err;
  }
  insert4->key = "peterr";
  insert4->keySize = 6;
  insert4->val = "rrrr";
  insert4->valSize = 4;
  err = genericPushToCache(cache1->localCache, insert4, NULL);
  if (err != 0) {
    printf("err code %d \n", err);
    return 1;
  }

  printCache(cache1);


  cacheObject *queryCo;
  err = initCacheObject(&queryCo);
  if (err != 0) {
    return err;
  }
  queryCo->key = "peterr";
  queryCo->keySize = 6;

  if(genericGetByKey(cache1->localCache, queryCo->key, queryCo->keySize, queryCo)) {
    printf("found val: %s \n", (char*)queryCo->val);
  } else {
    printf("not found \n");
  }


  return 0;
}

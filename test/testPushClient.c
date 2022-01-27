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
  cacheObject *insert2 = malloc(sizeof(cacheObject));
  insert2->key = "peter2";
  insert2->keySize = 6;
  insert2->val = "testVal2";
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

  char *r = malloc(sizeof(int));
  insert2->keySize = sizeof(int)+7;
  insert2->key = r;
  int i = 0;
  while (1) {
    sprintf(r, "peter%d", i++);
    printf("%s \n", (char*)insert2->key);
    err = cacheClientPushObject(cacheClient, insert2);
    if (err != 0) {
      printf("cacheClientPushO err code %d \n", err);
      return 1;
    }
    usleep(100);
  }


  return 0;
}

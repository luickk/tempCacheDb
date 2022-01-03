#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

typedef struct {
  void *key;
  uint8_t keySize;

  void *val;
  uint8_t valSize;

  int deleteMarker;
} cacheObject;

typedef struct
{
  int nSize;
  char *name;
  pthread_mutex_t *cacheMutex;

  int (*keyCmp)(void *key1, void *key2);

  cacheObject **keyValStore;
} tempCache;

enum errCodes {
  success,
  errNet,
  errIO,
  errMalloc,
  errCacheSize,
  errInit
};

int initTempCache(tempCache *cache, char *cacheName) {
  cache->name = cacheName;

  cache = malloc(sizeof(tempCache));
  if (cache == NULL) {
    return errMalloc;
  }

  if (pthread_mutex_init(cache->cacheMutex, NULL) != 0) {
    return errInit;
  }

  return success;
}

int freeTempCache(tempCache *cache, char *cacheName) {
  for (int i = 0; i < cache->nSize; i++) {
    free(cache->keyValStore[i]->key);
    free(cache->keyValStore[i]->val);
    free(cache->keyValStore[i]);
  }

  free(cache);
  pthread_mutex_destroy(cache->cacheMutex);

  return success;
}

int genericGetByKey(tempCache *cache, void *key, cacheObject *cO) {
  for (int i = 0; i < cache->nSize; i++) {
    if(cache->keyCmp(cache->keyValStore[i]->key, key)) {
      if (cO != NULL) {
        cO = cache->keyValStore[i];
      }
      return success;
    }
  }
  cO = NULL;
  return success;
}

// todo: cpy memory if found
int genericPushToCache(tempCache *cache, cacheObject *cO) {
  cacheObject *tempCo;
  if (genericGetByKey(cache, cO->key, tempCo)) {
    tempCo->val = cO->val;
  } else {
    if (cache->nSize == 0) {
      cache->keyValStore = malloc(sizeof(cacheObject*));
      if (cache->keyValStore == NULL) {
        return errMalloc;
      }
    } else {
      cache->keyValStore = realloc(cache->keyValStore, sizeof(cacheObject*)*(cache->nSize+1));
      if (cache->keyValStore == NULL) {
        return errMalloc;
      }
    }
    cache->keyValStore[cache->nSize] = cO;
    cache->nSize++;
  }

  return success;
}

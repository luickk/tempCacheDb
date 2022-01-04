#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>

typedef struct {
  void *key;
  uint8_t keySize;

  void *val;
  uint8_t valSize;

  int deleteMarker;
} cacheObject;

typedef int (*keyCompare)(void*, void*, int);

typedef struct
{
  char *name;
  keyCompare keyCmp;

  int nSize;
  pthread_mutex_t cacheMutex;
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

int initTempCache(tempCache *cache, char *cacheName, keyCompare keyCmp) {
  cache->name = cacheName;
  cache->keyCmp = keyCmp;

  cache->nSize = 0;
  cache = malloc(sizeof(tempCache));
  if (cache == NULL) {
    return errMalloc;
  }

  if (pthread_mutex_init(&cache->cacheMutex, NULL) != 0) {
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
  pthread_mutex_destroy(&cache->cacheMutex);

  return success;
}

int genericGetByKey(tempCache *cache, void *key, int keySize, cacheObject *resultingCO) {
  pthread_mutex_lock(&cache->cacheMutex);
  for (int i = 0; i < cache->nSize; i++) {
    if(cache->keyCmp(cache->keyValStore[i]->key, key, keySize)) {
      if (resultingCO != NULL) {
        resultingCO = cache->keyValStore[i];
      }
      return success;
    }
  }
  pthread_mutex_unlock(&cache->cacheMutex);
  resultingCO = NULL;
  return success;
}

// cashObject must be properly allocated!
int genericPushToCache(tempCache *cache, cacheObject *cO) {
  cacheObject *tempCoRef;
  if (genericGetByKey(cache, cO->key, cO->keySize, tempCoRef)) {
    pthread_mutex_lock(&cache->cacheMutex);
    tempCoRef->val = cO->val;
    pthread_mutex_unlock(&cache->cacheMutex);
  } else {
    pthread_mutex_lock(&cache->cacheMutex);
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
    pthread_mutex_unlock(&cache->cacheMutex);
  }

  return success;
}

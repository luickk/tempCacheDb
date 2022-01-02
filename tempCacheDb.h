#include <stdio.h>
#include <stdlib.h>

struct cacheObject {
  void *key;
  void *val;

  int deleteMarker;
};

typedef struct
{
  int nSize;
  char *name;

  int maxKeySize;
  int maxValSize;

  struct cacheObject **keyValStore;
} tempCache;

enum errCodes {
  success,
  errNet,
  errIO,
  errMalloc,
  errCacheSize
};

int initTempCache(tempCache *cache, char *cacheName) {
  cache->name = cacheName;

  cache = malloc(sizeof(tempCache));
  if (cache == NULL) {
    return errMalloc;
  }
  return success;
}

int genericPushToCache(tempCache *cache, struct cacheObject *cO, int keySize, int valSize) {
  if (cache->nSize == 0) {
    cache->keyValStore = malloc(sizeof(struct cacheObject*));
    if (cache->keyValStore == NULL) {
      return errMalloc;
    }
  } else {
    cache->keyValStore = realloc(cache->keyValStore, sizeof(struct cacheObject*)*(cache->nSize+1));
    if (cache->keyValStore == NULL) {
      return errMalloc;
    }
  }
  if (keySize > cache->maxKeySize || valSize > cache->maxValSize) {
    return errCacheSize;
  }
  cache->keyValStore[cache->nSize] = cO;
  cache->nSize++;

  return success;
}

int genericGetByKey(tempCache *cache, void *cmpFunc, void *key, struct cacheObject *cO) {

}

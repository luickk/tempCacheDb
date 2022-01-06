
#include <netdb.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

#define SERVER_BUFF_SIZE 1024

typedef struct {
  void *key;
  uint8_t keySize;

  void *val;
  uint8_t valSize;

  int deleteMarker;
} cacheObject;

typedef int (*keyCompare)(void*, void*, int);

typedef struct {
  char *name;
  keyCompare keyCmp;

  int nSize;
  pthread_mutex_t cacheMutex;
  cacheObject **keyValStore;
} tempCache;

struct pthreadClientHandleArgs {
  tempCache *cache;
  int socket;
};

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
    // free(cache->keyValStore[i]->key);
    // free(cache->keyValStore[i]->val);
    free(cache->keyValStore[i]);
  }

  free(cache);
  pthread_mutex_destroy(&cache->cacheMutex);

  return success;
}

int genericGetByKey(tempCache *cache, void *key, int keySize, cacheObject **resultingCo) {
  pthread_mutex_lock(&cache->cacheMutex);
  for (int i = 0; i < cache->nSize; i++) {
    if (cache->keyValStore[i]->keySize == keySize) {
      if(cache->keyCmp(cache->keyValStore[i]->key, key, keySize)) {
        *resultingCo = cache->keyValStore[i];
        return 1;
      }
    }
  }
  pthread_mutex_unlock(&cache->cacheMutex);
  resultingCo = NULL;
  return 0;
}

// cashObject must be properly allocated!
int genericPushToCache(tempCache *cache, cacheObject *cO) {
  cacheObject *tempCoRef;
  if (genericGetByKey(cache, cO->key, cO->keySize, &tempCoRef)) {
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

void *clientHandle(void *clientArgs) {
  struct pthreadClientHandleArgs *argss = (struct pthreadClientHandleArgs*)clientArgs;
  int socket = argss->socket;


  char *readBuff = malloc(sizeof(char)*SERVER_BUFF_SIZE);
  char *respBuff = malloc(sizeof(char)*SERVER_BUFF_SIZE);
  if (readBuff == NULL || respBuff == NULL) {
    close(socket);
    pthread_exit(NULL);
  }

  printf("sock: %d \n", socket);
}

int listenDbServer(tempCache *cache, int port) {
  struct sockaddr_in tempClient;

  int newSocket;
  int clientIdThreadCounter = 0;
  int serverSocket;
  socklen_t addrSize;
  pthread_t pthread;

  memset(&tempClient, 0, sizeof tempClient);
  tempClient.sin_family = AF_INET;
  tempClient.sin_port = htons(port);
  tempClient.sin_addr.s_addr = INADDR_ANY;

  if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    return errNet;
  }
  if (bind(serverSocket, (struct sockaddr *)&tempClient, sizeof tempClient) == -1) {
    return errNet;
  }
  if (listen(serverSocket, 1) != 0) {
    return errNet;
  }
  // for (int n = 0; n <= 5; n++) {
  while (1) {
    addrSize = sizeof tempClient;

    newSocket = accept(serverSocket, (struct sockaddr *) &tempClient, &addrSize);
    if (newSocket == -1) {
      return errNet;
    }

    struct pthreadClientHandleArgs *clientArgs = malloc(sizeof clientArgs);
    if (clientArgs == NULL) {
      close(newSocket);
      return errMalloc;
    }
    clientArgs->socket = newSocket;
    clientArgs->cache = cache;

    if(pthread_create(&pthread, NULL, clientHandle, (void*)clientArgs) != 0 ) {
      return errIO;
    } else {
      clientIdThreadCounter++;
    }
  }
  return success;
}


#include <netdb.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

#include <arpa/inet.h>

#define SERVER_BUFF_SIZE 1024

#define NET_KEY_VAL_SIZE_LIMIT 255

#define MAX_CACHE_SIZE 100

typedef struct {
  void *key;
  uint16_t keySize;

  void *val;
  uint16_t valSize;

  int deleteMarker;
} cacheObject;

typedef int (*keyCompare)(void*, void*, int);
typedef void (*freeCacheObject)(cacheObject *cO);

typedef struct {
  int tmpSocket;
  int serverSocket;
  uint nCacheSize;

  char *name;
  keyCompare keyCmp;
  freeCacheObject freeCoFn;
  pthread_mutex_t cacheMutex;
  cacheObject **keyValStore;

  socklen_t addrSize;
  pthread_t pthread;
  struct sockaddr_in tempClient;
} tempCache;

typedef struct {
  int sockfd;
  pthread_mutex_t cacheClientMutex;
  struct sockaddr_in servaddr, cli;
} tempCacheClient;

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
  errFree,
  errInit
};

int initTempCache(tempCache *cache, keyCompare keyCmp, freeCacheObject freeCoFn) {
  cache->keyCmp = keyCmp;
  cache->freeCoFn = freeCoFn;

  cache->nCacheSize = 0;
  cache = malloc(sizeof(tempCache));
  if (cache == NULL) {
    return errMalloc;
  }

  if (pthread_mutex_init(&cache->cacheMutex, NULL) != 0) {
    return errInit;
  }

  return success;
}

int freeTempCache(tempCache *cache) {
  for (int i = 0; i < cache->nCacheSize; i++) {
    cache->freeCoFn(cache->keyValStore[i]);
  }

  free(cache);
  if(pthread_mutex_destroy(&cache->cacheMutex) != 0) {
    return errFree;
  }

  return success;
}

// returns 1 if cO key has been found in the cache and 0 if not
int genericGetByKey(tempCache *cache, void *key, int keySize, cacheObject ***resultingCo) {
  pthread_mutex_lock(&cache->cacheMutex);
  for (int i = 0; i < cache->nCacheSize; i++) {
    if (cache->keyValStore[i]->keySize == keySize) {
      if(cache->keyCmp(cache->keyValStore[i]->key, key, keySize)) {
        *resultingCo = &cache->keyValStore[i];
        return 1;
      }
    }
  }
  pthread_mutex_unlock(&cache->cacheMutex);
  *resultingCo = NULL;
  return 0;
}

// cashObject must be properly allocated!
int genericPushToCache(tempCache *cache, cacheObject *cO) {
  cacheObject **tempCoRef;

  if (genericGetByKey(cache, cO->key, cO->keySize, &tempCoRef)) {
    pthread_mutex_lock(&cache->cacheMutex);

    cache->freeCoFn(*tempCoRef);
    *tempCoRef = cO;
    
    pthread_mutex_unlock(&cache->cacheMutex);
  } else {
    pthread_mutex_lock(&cache->cacheMutex);
    if (cache->nCacheSize <= MAX_CACHE_SIZE) {
      if (cache->nCacheSize == 0) {
        cache->keyValStore = malloc(sizeof(cacheObject*));
        if (cache->keyValStore == NULL) {
          return errMalloc;
        }
      } else {
        cache->keyValStore = realloc(cache->keyValStore, sizeof(cacheObject*)*(cache->nCacheSize+1));
        if (cache->keyValStore == NULL) {
          return errMalloc;
        }
      }
      cache->keyValStore[cache->nCacheSize] = cO;
      cache->nCacheSize++;
    }
    pthread_mutex_unlock(&cache->cacheMutex);
  }
  return success;
}

int cpyCacheObject(cacheObject **dest, cacheObject *src) {

  if (*dest != NULL && (*dest)->valSize == src->valSize) {
    memcpy((*dest)->key, src->key, (*dest)->keySize);
  }
  if ((*dest) != NULL && (*dest)->keySize == src->keySize) {
    memcpy((*dest)->key, src->key, (*dest)->keySize);
  }

  if ((*dest) == NULL) {
    (*dest) = malloc(sizeof(cacheObject));
    if((*dest)==NULL) {
      return errMalloc;
    }
    if (src->val != NULL) {
      (*dest)->val = malloc(src->valSize);
      if((*dest)==NULL) {
        return errMalloc;
      }

      (*dest)->valSize = src->valSize;
      memcpy((*dest)->val, src->val, (*dest)->valSize);
    }
    if (src->key != NULL) {
      (*dest)->key = malloc(src->keySize);
      if((*dest)==NULL) {
        return errMalloc;
      }

      (*dest)->keySize = src->keySize;
      memcpy((*dest)->key, src->key, (*dest)->keySize);
    }
  }

  return success;
}

int initCacheClient(tempCacheClient **cacheClient) {
  *cacheClient = malloc(sizeof(tempCacheClient));
  if (*cacheClient == NULL) {
    return errMalloc;
  }
  if (pthread_mutex_init(&(*cacheClient)->cacheClientMutex, NULL) != 0) {
    return errInit;
  }
  return success;
}

int freeCacheClient(tempCacheClient *cacheClient) {
  if(pthread_mutex_destroy(&cacheClient->cacheClientMutex) != 0) {
    return errFree;
  }
  close(cacheClient->sockfd);
  free(cacheClient);
  return success;
}


int cacheClientConnect(tempCacheClient *cacheClient, char *addressString, int port) {
  pthread_mutex_lock(&cacheClient->cacheClientMutex);
  cacheClient->sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (cacheClient->sockfd == -1) {
    return errNet;
  } else {
    bzero(&cacheClient->servaddr, sizeof(cacheClient->servaddr));
  }

  cacheClient->servaddr.sin_family = AF_INET;
  cacheClient->servaddr.sin_addr.s_addr = inet_addr(addressString);
  cacheClient->servaddr.sin_port = htons(port);

  // connect the client socket to server socket
  if (connect(cacheClient->sockfd, (struct sockaddr*)&cacheClient->servaddr, sizeof(cacheClient->servaddr)) != 0) {
    return errNet;
  }
  pthread_mutex_unlock(&cacheClient->cacheClientMutex);

  return success;
}

int cacheClientPushObject(tempCacheClient *cacheClient, cacheObject *cO) {
  int keySizeSize = sizeof(cO->keySize);
  int sendBuffSize = keySizeSize + cO->keySize + keySizeSize + cO->valSize;
  int netByteOrderSize = 0;
  char *sendBuff = malloc(keySizeSize * sendBuffSize);
  if (sendBuff == NULL) {
    return errMalloc;
  }
  sendBuffSize = 0;
  netByteOrderSize = htons(cO->keySize);
  memcpy(sendBuff+sendBuffSize, &netByteOrderSize, keySizeSize);
  sendBuffSize += keySizeSize;
  memcpy(sendBuff+sendBuffSize, cO->key, cO->keySize);
  sendBuffSize += cO->keySize;
  netByteOrderSize = htons(cO->valSize);
  memcpy(sendBuff+sendBuffSize, &netByteOrderSize, keySizeSize);
  sendBuffSize += keySizeSize;
  memcpy(sendBuff+sendBuffSize, cO->val, cO->valSize);
  sendBuffSize += cO->valSize;

  pthread_mutex_lock(&cacheClient->cacheClientMutex);
  if (write(cacheClient->sockfd, sendBuff, sendBuffSize) == -1) {
    return errIO;
  }
  pthread_mutex_unlock(&cacheClient->cacheClientMutex);
  free(sendBuff);
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

  cacheObject *tempCo = malloc(sizeof(cacheObject));
  if (tempCo == NULL) {
    close(socket);
    pthread_exit(NULL);
  }

  int readBuffSize = 0;
  uint16_t tempProtocolSize = 0;
  int readSizePointer = 0;
  int nextIsKey = 1;
  int keySizeSize = sizeof(tempCo->keySize);
  cacheObject *copiedCo;
  while(1) {
    readBuffSize = read(socket, readBuff, SERVER_BUFF_SIZE);
    if (readBuffSize == -1) {
      close(socket);
      pthread_exit(NULL);
    }
    tempProtocolSize = 0;
    if (readBuffSize >= keySizeSize) {
      readSizePointer = 0;
      memcpy(&tempProtocolSize, readBuff, keySizeSize);
      readSizePointer += keySizeSize;
      tempProtocolSize = ntohs(tempProtocolSize);

      if (readBuffSize >= tempProtocolSize+keySizeSize) {
        tempCo->keySize = tempProtocolSize;
        tempCo->key = malloc(tempProtocolSize);
        if (tempCo->key == NULL) {
          close(socket);
          pthread_exit(NULL);
        }
        memcpy(tempCo->key, readBuff+readSizePointer, tempProtocolSize);
        readSizePointer += tempProtocolSize;
      }

      memcpy(&tempProtocolSize, readBuff+readSizePointer, keySizeSize);
      readSizePointer += keySizeSize;
      tempProtocolSize = ntohs(tempProtocolSize);

      if (readBuffSize >= tempProtocolSize+keySizeSize) {
        tempCo->valSize = tempProtocolSize;
        tempCo->val = malloc(tempProtocolSize);
        if (tempCo->key == NULL) {
          close(socket);
          pthread_exit(NULL);
        }
        memcpy(tempCo->val, readBuff+readSizePointer, tempProtocolSize);

        copiedCo = NULL;
        if (cpyCacheObject(&copiedCo, tempCo) != 0) {
          close(socket);
          pthread_exit(NULL);
        }
        genericPushToCache(argss->cache, copiedCo);
      }
    }
  }
}

int listenDbServer(tempCache *cache, int port) {
  memset(&cache->tempClient, 0, sizeof cache->tempClient);
  cache->tempClient.sin_family = AF_INET;
  cache->tempClient.sin_port = htons(port);
  cache->tempClient.sin_addr.s_addr = INADDR_ANY;

  if ((cache->serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    return errNet;
  }
  if (bind(cache->serverSocket, (struct sockaddr *)&cache->tempClient, sizeof(cache->tempClient)) == -1) {
    return errNet;
  }
  if (listen(cache->serverSocket, 1) != 0) {
    return errNet;
  }
  // for (int n = 0; n <= 5; n++) {
  while (1) {
    cache->addrSize = sizeof(cache->tempClient);

    cache->tmpSocket = accept(cache->serverSocket, (struct sockaddr *) &cache->tempClient, &cache->addrSize);
    if (cache->tmpSocket == -1) {
      return errNet;
    }

    struct pthreadClientHandleArgs *clientArgs = malloc(sizeof clientArgs);
    if (clientArgs == NULL) {
      close(cache->tmpSocket);
      return errMalloc;
    }
    clientArgs->socket = cache->tmpSocket;
    clientArgs->cache = cache;

    if(pthread_create(&cache->pthread, NULL, clientHandle, (void*)clientArgs) != 0 ) {
      return errIO;
    }
  }
  return success;
}

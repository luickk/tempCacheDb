
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

#define MAX_CACHE_SIZE 1000000

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
  uint nCacheSize;
  keyCompare keyCmp;
  freeCacheObject freeCoFn;
  pthread_mutex_t cacheMutex;
  cacheObject **keyValStore;
} simpleCache;

typedef struct {
  int serverSocket;
  pthread_t pthread;

  simpleCache *localCache;
} tempCache;

typedef struct {
  int sockfd;
  pthread_mutex_t cacheClientMutex;
  struct sockaddr_in servaddr, cli;
  pthread_t pthread;

  simpleCache *clientReqReplyLink;
} tempCacheClient;

struct pthreadClientHandleArgs {
  void *cache;
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

// TODO if malloc is not meant to be freed by the lib user create adequate create function

int initSimpleCache(simpleCache **cache, keyCompare keyCmp, freeCacheObject freeCoFn) {
  *cache = malloc(sizeof(simpleCache));
  if (*cache == NULL) {
    return errMalloc;
  }
  (*cache)->keyCmp = keyCmp;
  (*cache)->freeCoFn = freeCoFn;
  (*cache)->nCacheSize = 0;

  return success;
}

int initTempCache(tempCache **cache, keyCompare keyCmp, freeCacheObject freeCoFn) {
  *cache = malloc(sizeof(tempCache));
  if (*cache == NULL) {
    return errMalloc;
  }

  int err = initSimpleCache(&(*cache)->localCache, keyCmp, freeCoFn);
  if(err != 0) {
    return err;
  }

  if (pthread_mutex_init(&(*cache)->localCache->cacheMutex, NULL) != 0) {
    return errInit;
  }

  return success;
}

int freeTempCache(tempCache *cache) {
  for (int i = 0; i < cache->localCache->nCacheSize; i++) {
    cache->localCache->freeCoFn(cache->localCache->keyValStore[i]);
  }

  free(cache);
  if(pthread_mutex_destroy(&cache->localCache->cacheMutex) != 0) {
    return errFree;
  }

  return success;
}

// returns 1 if cO key has been found in the cache and 0 if not
// writes to Co val and valSize (mallocs if necessary)
// !resultingCo must not contain pointer to possibly allocated memory to prevent potential memory leak!
int genericGetByKey(simpleCache *localCache, void *key, int keySize, cacheObject *resultingCo) {
  pthread_mutex_lock(&localCache->cacheMutex);
  for (int i = 0; i < localCache->nCacheSize; i++) {
    if (localCache->keyValStore[i]->keySize == keySize) {
      if(localCache->keyCmp(localCache->keyValStore[i]->key, key, keySize)) {
        resultingCo->valSize = localCache->keyValStore[i]->valSize;
        resultingCo->val = malloc(resultingCo->valSize);
        memcpy(resultingCo->val, localCache->keyValStore[i]->val, resultingCo->valSize);

        pthread_mutex_unlock(&localCache->cacheMutex);
        return 1;
      }
    }
  }
  pthread_mutex_unlock(&localCache->cacheMutex);
  return 0;
}

// returns 1 if cO key has been found in the cache and 0 if not
// writes pointer to resultingCo param from the cache's Co array
// don't forget to use the caches mutex on the returned array pointer
// be carefull not to pass a allocated Co to resultingCo as it will not be freed
int genericGetCoRefByKey(simpleCache *localCache, void *key, int keySize, cacheObject ***resultingCo) {
  pthread_mutex_lock(&localCache->cacheMutex);
  for (int i = 0; i < localCache->nCacheSize; i++) {
    if (localCache->keyValStore[i]->keySize == keySize) {
      if(localCache->keyCmp(localCache->keyValStore[i]->key, key, keySize)) {
        *resultingCo = &localCache->keyValStore[i];
        pthread_mutex_unlock(&localCache->cacheMutex);
        return 1;
      }
    }
  }
  *resultingCo = NULL;
  pthread_mutex_unlock(&localCache->cacheMutex);
  return 0;
}

// CashObject must be properly allocated!
// Don't reuse pushed cache Object. The memory is now manged by the cache
// and could be freed or moved at any point in time
int genericPushToCache(simpleCache *sCache, cacheObject *cO) {
  cacheObject **tempCoRef;
  if (genericGetCoRefByKey(sCache, cO->key, cO->keySize, &tempCoRef)) {
    pthread_mutex_lock(&sCache->cacheMutex);
    sCache->freeCoFn(*tempCoRef);
    *tempCoRef = cO;

    pthread_mutex_unlock(&sCache->cacheMutex);
  } else {
    pthread_mutex_lock(&sCache->cacheMutex);
    if (sCache->nCacheSize <= MAX_CACHE_SIZE) {
      if (sCache->nCacheSize == 0) {
        sCache->keyValStore = malloc(sizeof(cacheObject*));
        if (sCache->keyValStore == NULL) {
          return errMalloc;
        }
      } else {
        sCache->keyValStore = realloc(sCache->keyValStore, sizeof(cacheObject*)*(sCache->nCacheSize+1));
        if (sCache->keyValStore == NULL) {
          return errMalloc;
        }
      }
      sCache->keyValStore[sCache->nCacheSize] = cO;
      sCache->nCacheSize++;
    }
    pthread_mutex_unlock(&sCache->cacheMutex);
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

void clientReqReplyLinkFree(cacheObject *cO) {
  free(cO);
  // TODO why can this not be freed?
  // free(cO->key);
  // free(cO->val);
}

int clientReqReplyLinkKeyCmp(void *key1, void *key2, int size) {

  /* does not work with void pointers? returns always 0 */
  // int t = strncmp(key1, key2, size);

  char *ckey1 = (char*)key1;
  char *ckey2 = (char*)key2;
  for (int i = 0; i < size; i++) {
    if (ckey1[i]!=ckey2[i]) {
      return 0;
    }
  }
  return 1;
}

int initCacheClient(tempCacheClient **cacheClient) {
  *cacheClient = malloc(sizeof(tempCacheClient));
  if (*cacheClient == NULL) {
    return errMalloc;
  }
  if (pthread_mutex_init(&(*cacheClient)->cacheClientMutex, NULL) != 0) {
    return errInit;
  }

  int err = initSimpleCache(&(*cacheClient)->clientReqReplyLink, clientReqReplyLinkKeyCmp, clientReqReplyLinkFree);
  if(err != 0) {
    return err;
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


void *cacheClientPullHandler(void *argss) {
  struct pthreadClientHandleArgs *args = (struct pthreadClientHandleArgs*) argss;
  tempCacheClient *cacheClient = args->cache;
  int socket = args->socket;

  char *readBuff = malloc(sizeof(char)*SERVER_BUFF_SIZE);
  if (readBuff == NULL) {
    close(socket);
    pthread_exit(NULL);
  }

  cacheObject *tempCo = malloc(sizeof(cacheObject));
  if (tempCo == NULL) {
    close(socket);
    pthread_exit(NULL);
  }
  cacheObject **tempCoReqRepRef = malloc(sizeof(cacheObject));
  if (tempCoReqRepRef == NULL) {
    close(socket);
    pthread_exit(NULL);
  }

  int readBuffSize = 0;
  uint16_t tempProtocolSize = 0;
  int readSizePointer = 0;
  int isKey = 1;
  int keySizeSize = sizeof(tempCo->keySize);
  cacheObject *copiedCo;

  while(1) {
    readBuffSize = read(socket, readBuff, SERVER_BUFF_SIZE);
    if (readBuffSize == -1) {
      close(socket);
      pthread_exit(NULL);
    }
    tempProtocolSize = 0;
    // TODO implement tcp complete-message-buff independent message handling
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

      // is a push operation, checking if enough memory is in the buffer
      if (readBuffSize >= tempProtocolSize+keySizeSize) {
        tempCo->valSize = tempProtocolSize;
        tempCo->val = malloc(tempProtocolSize);
        if (tempCo->key == NULL) {
          close(socket);
          pthread_exit(NULL);
        }
        memcpy(tempCo->val, readBuff+readSizePointer, tempProtocolSize);

        if (genericGetCoRefByKey(cacheClient->clientReqReplyLink, tempCo->key, tempCo->keySize, &tempCoReqRepRef)) {
          (*tempCoReqRepRef)->valSize = tempCo->valSize;
          (*tempCoReqRepRef)->val = malloc(tempCo->valSize);
          memcpy((*tempCoReqRepRef)->val, tempCo->val, tempCo->valSize);
        }

        free(tempCo->val);
        tempCo->val = NULL;
      }
    }
  }
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

  struct pthreadClientHandleArgs *clientArgs = malloc(sizeof clientArgs);
  if (clientArgs == NULL) {
    close(cacheClient->sockfd);
    return errMalloc;
  }
  clientArgs->socket = cacheClient->sockfd;
  clientArgs->cache = (void*)cacheClient;

  if(pthread_create(&cacheClient->pthread, NULL, cacheClientPullHandler, (void*)clientArgs) != 0 ) {
    return errIO;
  }

  return success;
}

int cacheClientPushObject(tempCacheClient *cacheClient, cacheObject *cO) {
  int keySizeSize = sizeof(cO->keySize);
  int sendBuffSize = keySizeSize + cO->keySize + keySizeSize + cO->valSize;
  int netByteOrderSize = 0;
  char *sendBuff = malloc(sizeof(char) * sendBuffSize);
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

int cacheReplyToPull(int sockfd, cacheObject *cO) {
  int keySizeSize = sizeof(cO->keySize);
  int sendBuffSize = keySizeSize + cO->keySize + keySizeSize + cO->valSize;
  int netByteOrderSize = 0;
  char *sendBuff = malloc(sizeof(char) * sendBuffSize);
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
  if (cO->val != NULL) {
    memcpy(sendBuff+sendBuffSize, cO->val, cO->valSize);
    sendBuffSize += cO->valSize;
  }

  if (write(sockfd, sendBuff, sendBuffSize) == -1) {
    return errIO;
  }

  free(sendBuff);
  return success;
}

int cacheClientPullByKey(tempCacheClient *cacheClient, void *key, int keySize, cacheObject **pulledCo) {
  int keySizeSize = sizeof((*pulledCo)->keySize);
  int sendBuffSize = keySizeSize + keySize + keySizeSize;
  int netByteOrderSize = 0;
  char *sendBuff = malloc(sizeof(char) * sendBuffSize);
  if (sendBuff == NULL) {
    return errMalloc;
  }
  sendBuffSize = 0;

  netByteOrderSize = htons(keySize);
  memcpy(sendBuff+sendBuffSize, &netByteOrderSize, keySizeSize);
  sendBuffSize += keySizeSize;

  memcpy(sendBuff+sendBuffSize, key, keySize);
  sendBuffSize += keySize;

  netByteOrderSize = htons(0);
  memcpy(sendBuff+sendBuffSize, &netByteOrderSize, keySizeSize);
  sendBuffSize += keySizeSize;



  pthread_mutex_lock(&cacheClient->cacheClientMutex);

  cacheObject *clientReqReply = malloc(sizeof(cacheObject));
  clientReqReply->key = malloc(keySize);
  memcpy(clientReqReply->key, key, keySize);
  clientReqReply->keySize = keySize;

  pthread_mutex_t onReplyCondMutex;

  cacheObject *tempCo = malloc(sizeof(cacheObject));
  if (tempCo == NULL) {
    return errMalloc;
  }
  clientReqReply->val = NULL;

  int err = genericPushToCache(cacheClient->clientReqReplyLink, clientReqReply);
  if (err != 0) {
    return 1;
  }

  if (write(cacheClient->sockfd, sendBuff, sendBuffSize) == -1) {
    return errIO;
  }

  while(clientReqReply->val == NULL) {}

  *pulledCo = clientReqReply;

  pthread_mutex_unlock(&cacheClient->cacheClientMutex);
  free(sendBuff);

  return success;
}

void *cacheSurveillance(void *cacheP) {
  tempCache *cache = (tempCache*)cacheP;
  while (1) {
    pthread_mutex_lock(&cache->localCache->cacheMutex);
    printf("----------------- cacheSurveillance ----------------- \n");
    printf("Rows: %d \n", cache->localCache->nCacheSize);
    for (int i = 0; i < cache->localCache->nCacheSize; i++) {
      printf("row %d - k: '%.*s'(%d) v: '%.*s'(%d) \n", i, cache->localCache->keyValStore[i]->keySize,
                                                          (char*)cache->localCache->keyValStore[i]->key, cache->localCache->keyValStore[i]->keySize,
                                                          cache->localCache->keyValStore[i]->valSize, (char*)cache->localCache->keyValStore[i]->val,
                                                          cache->localCache->keyValStore[i]->valSize);
    }
    printf("-----------------                    ----------------- \n");
    pthread_mutex_unlock(&cache->localCache->cacheMutex);
    sleep(5);
  }
}

void *clientHandle(void *clientArgs) {
  struct pthreadClientHandleArgs *argss = (struct pthreadClientHandleArgs*)clientArgs;
  tempCache *cache = argss->cache;
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
  int isKey = 1;
  int keySizeSize = sizeof(tempCo->keySize);
  cacheObject *copiedCo;
  while(1) {
    readBuffSize = read(socket, readBuff, SERVER_BUFF_SIZE);
    if (readBuffSize == -1) {
      close(socket);
      pthread_exit(NULL);
    }
    tempProtocolSize = 0;
    // TODO implement tcp complete-message-buff independent message handling
    if (readBuffSize >= keySizeSize) {
      readSizePointer = 0;
      memcpy(&tempProtocolSize, readBuff, keySizeSize);
      readSizePointer += keySizeSize;
      tempProtocolSize = ntohs(tempProtocolSize);

      if (readBuffSize >= tempProtocolSize+keySizeSize && isKey) {
        isKey = 0;
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

      // is a push operation, checking if enough memory is in the buffer
      if (readBuffSize >= tempProtocolSize+keySizeSize && tempProtocolSize != 0 && !isKey) {
        isKey = 1;
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

        genericPushToCache(cache->localCache, copiedCo);

        free(tempCo->val);
        tempCo->val = NULL;
      // is a pull operation
      } else if (tempProtocolSize == 0 && !isKey){
        isKey = 1;
        printf("PULL \n");
        genericGetByKey(cache->localCache, tempCo->key, tempCo->keySize, tempCo);
        cacheReplyToPull(socket, tempCo);

        free(tempCo->val);
        tempCo->val = NULL;
      }
    }
  }
}

int listenDbServer(tempCache *cache, int port) {
  int tmpSocket;
  socklen_t addrSize;
  struct sockaddr_in tempClient;

  cache->serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (cache->serverSocket == -1) {
    return errNet;
  }

  memset(&tempClient, 0, sizeof(tempClient));
  tempClient.sin_family = AF_INET;
  tempClient.sin_port = htons(port);
  tempClient.sin_addr.s_addr = INADDR_ANY;

  if (bind(cache->serverSocket, (struct sockaddr *)&tempClient, sizeof(tempClient)) < 0) {
    return errNet;
  }
  if (listen(cache->serverSocket, 1) != 0) {
    return errNet;
  }
  pthread_t survPthread;
  if(pthread_create(&survPthread, NULL, cacheSurveillance, (void*)cache) != 0 ) {
    return errIO;
  }

  // for (int n = 0; n <= 5; n++) {
  while (1) {
    addrSize = sizeof(tempClient);

    tmpSocket = accept(cache->serverSocket, (struct sockaddr *)&tempClient, &addrSize);
    if (tmpSocket == -1) {
      return errNet;
    }

    struct pthreadClientHandleArgs *clientArgs = malloc(sizeof clientArgs);
    if (clientArgs == NULL) {
      close(tmpSocket);
      return errMalloc;
    }
    clientArgs->socket = tmpSocket;
    clientArgs->cache = (void*)cache;

    if(pthread_create(&cache->pthread, NULL, clientHandle, (void*)clientArgs) != 0 ) {
      return errIO;
    }
  }
  return success;
}

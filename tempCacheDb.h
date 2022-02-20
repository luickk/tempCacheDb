
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

// enable same socket push&pull

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

int freeTempCache(tempCache **cache) {
  for (int i = 0; i < (*cache)->localCache->nCacheSize; i++) {
    (*cache)->localCache->freeCoFn((*cache)->localCache->keyValStore[i]);
    (*cache)->localCache->keyValStore[i] = NULL;
  }

  free(*cache);
  (*cache) = NULL;

  if(pthread_mutex_destroy(&(*cache)->localCache->cacheMutex) != 0) {
    return errFree;
  }

  return success;
}

// initializes only the cacheObject struct, not its values!
int initCacheObject(cacheObject **cO) {
  *cO = malloc(sizeof(cacheObject));
  if (cO == NULL) {
    return errMalloc;
  }
  (*cO)->val = NULL;
  (*cO)->key = NULL;
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
  cacheObject *toFreeCo;
  if (genericGetCoRefByKey(sCache, cO->key, cO->keySize, &tempCoRef)) {
    pthread_mutex_lock(&sCache->cacheMutex);
    toFreeCo = *tempCoRef;
    *tempCoRef = cO;
    sCache->freeCoFn(toFreeCo);
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
  int err;
  if (*dest != NULL && (*dest)->valSize == src->valSize) {
    memcpy((*dest)->key, src->key, (*dest)->keySize);
  }
  if ((*dest) != NULL && (*dest)->keySize == src->keySize) {
    memcpy((*dest)->key, src->key, (*dest)->keySize);
  }

  if ((*dest) == NULL) {
    err = initCacheObject(dest);
    if (err != 0) {
      return err;
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

void freeCacheObjectDefault(cacheObject *cO) {
  if (cO->key != NULL)
    free(cO->key);
    cO->key = NULL;
  if (cO->val != NULL)
    free(cO->val);
    cO->val = NULL;
  if (cO != NULL)
    free(cO);
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

  int err = initSimpleCache(&(*cacheClient)->clientReqReplyLink, clientReqReplyLinkKeyCmp, freeCacheObjectDefault);
  if(err != 0) {
    return err;
  }

  return success;
}

int freeCacheClient(tempCacheClient **cacheClient) {
  if(pthread_mutex_destroy(&(*cacheClient)->cacheClientMutex) != 0) {
    return errFree;
  }
  close((*cacheClient)->sockfd);
  free(*cacheClient);
  *cacheClient = NULL;
  return success;
}

// todo pthread free stack
void *cacheClientPullHandler(void *argss) {
  struct pthreadClientHandleArgs *args = (struct pthreadClientHandleArgs*) argss;
  tempCacheClient *cacheClient = args->cache;
  int socket = args->socket;
  int err;

  char *readBuff = malloc(sizeof(char)*SERVER_BUFF_SIZE);
  if (readBuff == NULL) {
    close(socket);
    pthread_exit(NULL);
  }

  cacheObject *tempCo;
  err = initCacheObject(&tempCo);
  if (err != 0) {
    close(socket);
    pthread_exit(NULL);
  }

  cacheObject **tempCoReqRepRef;

  int readBuffSize = 0;
  uint16_t tempProtocolSize = 0;
  int readSizePointer = 0;
  int nElementParsed = 0;
  int leftOverSize = 0;
  char *leftOverBuff;
  char *tempReadBuff;
  int keySizeSize = sizeof(tempCo->keySize);
  cacheObject *copiedCo;

  int opCode = 0;

  while(1) {
    readBuffSize = read(socket, readBuff, SERVER_BUFF_SIZE);
    if (readBuffSize == -1) {
      close(socket);
      pthread_exit(NULL);
    } else if (readBuffSize == 0) {
      break;
    }
    readSizePointer = 0;
    tempProtocolSize = 0;

    // merging memory
    if (leftOverSize > 0) {
      char *mergingMem = malloc(leftOverSize + readBuffSize);
      if (mergingMem == NULL) {
        close(socket);
        pthread_exit(NULL);
      }
      memcpy(mergingMem, leftOverBuff, leftOverSize);
      free(leftOverBuff);
      memcpy(mergingMem+leftOverSize, readBuff, readBuffSize);
      tempReadBuff = readBuff;
      readBuffSize += leftOverSize;
      readBuff = mergingMem;
    }

    if (readBuffSize >= sizeof(uint8_t) && nElementParsed == 0) {
      memcpy(&tempProtocolSize, readBuff, sizeof(uint8_t));
      readSizePointer += sizeof(uint8_t);
      opCode = tempProtocolSize;
      nElementParsed++;
    }

    if ((readBuffSize-readSizePointer) >= keySizeSize && nElementParsed == 1) {
      memcpy(&tempProtocolSize, readBuff+readSizePointer, keySizeSize);
      readSizePointer += keySizeSize;
      tempProtocolSize = ntohs(tempProtocolSize);
      nElementParsed++;
    }

    if ((readBuffSize-readSizePointer) >= tempProtocolSize && nElementParsed == 2) {
      tempCo->keySize = tempProtocolSize;
      tempCo->key = malloc(tempProtocolSize);
      if (tempCo->key == NULL) {
        close(socket);
        pthread_exit(NULL);
      }
      memcpy(tempCo->key, readBuff+readSizePointer, tempProtocolSize);
      readSizePointer += tempProtocolSize;
      nElementParsed++;
    }

    if ((readBuffSize-readSizePointer) >= keySizeSize && nElementParsed == 3) {
      memcpy(&tempProtocolSize, readBuff+readSizePointer, keySizeSize);
      readSizePointer += keySizeSize;
      tempProtocolSize = ntohs(tempProtocolSize);
      nElementParsed++;
    }

    if ((readBuffSize-readSizePointer) >= tempProtocolSize && nElementParsed == 4) {
      if (opCode == 3) {
        tempCo->valSize = tempProtocolSize;
        tempCo->val = malloc(tempProtocolSize);
        if (tempCo->key == NULL) {
          close(socket);
          pthread_exit(NULL);
        }
        memcpy(tempCo->val, readBuff+readSizePointer, tempProtocolSize);

        if (genericGetCoRefByKey(cacheClient->clientReqReplyLink, tempCo->key, tempCo->keySize, &tempCoReqRepRef)) {
          pthread_mutex_lock(&cacheClient->clientReqReplyLink->cacheMutex);
          (*tempCoReqRepRef)->valSize = tempCo->valSize;
          if ((*tempCoReqRepRef)->val == NULL) {
            (*tempCoReqRepRef)->val = malloc(tempCo->valSize);
            if ((*tempCoReqRepRef)->val == NULL) {
              close(socket);
              pthread_exit(NULL);
            }
          } else {
            (*tempCoReqRepRef)->val = realloc((*tempCoReqRepRef)->val, tempCo->valSize);
            if ((*tempCoReqRepRef)->val == NULL) {
              close(socket);
              pthread_exit(NULL);
            }
          }

          memcpy((*tempCoReqRepRef)->val, tempCo->val, tempCo->valSize);

          pthread_mutex_unlock(&cacheClient->clientReqReplyLink->cacheMutex);
        }
        nElementParsed++;
        if (leftOverSize > 0) {
          readBuff = tempReadBuff;
        }
        leftOverSize = 0;
        free(tempCo->val);
        tempCo->val = NULL;
      }
      if (nElementParsed < 5) {
        leftOverSize = readBuffSize-readSizePointer;
        leftOverBuff = malloc(leftOverSize*sizeof(char));
        memcpy(leftOverBuff, readBuff+readSizePointer, leftOverSize);
      }
      nElementParsed = 0;
    }
  }
  freeCacheObjectDefault(tempCo);
  if (leftOverBuff != NULL) {
    free(leftOverBuff);
    leftOverBuff = NULL;
  }
  if (readBuff != NULL) {
    free(readBuff);
    readBuff = NULL;
  }
  return NULL;
}

int cacheClientConnect(tempCacheClient *cacheClient, char *addressString, int port) {
  pthread_mutex_lock(&cacheClient->cacheClientMutex);
  cacheClient->sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (cacheClient->sockfd == -1) {
    return errNet;
  }
  bzero(&cacheClient->servaddr, sizeof(cacheClient->servaddr));

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
  int sendBuffSize = sizeof(uint8_t) + keySizeSize + cO->keySize + keySizeSize + cO->valSize;
  char *sendBuff = malloc(sizeof(char) * sendBuffSize);
  int netByteOrderSize = 0;
  if (sendBuff == NULL) {
    return errMalloc;
  }
  sendBuffSize = 0;
  netByteOrderSize = 2;
  memcpy(sendBuff+sendBuffSize, &netByteOrderSize, sizeof(uint8_t));
  sendBuffSize += sizeof(uint8_t);

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
  int sendBuffSize = sizeof(uint8_t) + keySizeSize + cO->keySize + keySizeSize + cO->valSize;
  char *sendBuff = malloc(sizeof(char) * sendBuffSize);
  int netByteOrderSize = 0;
  if (sendBuff == NULL) {
    return errMalloc;
  }
  sendBuffSize = 0;
  netByteOrderSize = 3;
  memcpy(sendBuff+sendBuffSize, &netByteOrderSize, sizeof(uint8_t));
  sendBuffSize += sizeof(uint8_t);

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
  int sendBuffSize = sizeof(uint8_t) + keySizeSize + keySize + keySizeSize;
  int netByteOrderSize = 0;
  char *sendBuff = malloc(sizeof(char) * sendBuffSize);
  if (sendBuff == NULL) {
    return errMalloc;
  }
  int err;

  sendBuffSize = 0;
  netByteOrderSize = 1;
  memcpy(sendBuff+sendBuffSize, &netByteOrderSize, sizeof(uint8_t));
  sendBuffSize += sizeof(uint8_t);

  netByteOrderSize = htons(keySize);
  memcpy(sendBuff+sendBuffSize, &netByteOrderSize, keySizeSize);
  sendBuffSize += keySizeSize;

  memcpy(sendBuff+sendBuffSize, key, keySize);
  sendBuffSize += keySize;

  netByteOrderSize = htons(0);
  memcpy(sendBuff+sendBuffSize, &netByteOrderSize, keySizeSize);
  sendBuffSize += keySizeSize;

  cacheObject *clientReqReply;
  err = initCacheObject(&clientReqReply);
  if (err != 0) {
    return err;
  }
  clientReqReply->key = malloc(keySize);
  if (clientReqReply->key == NULL) {
    return errMalloc;
  }

  memcpy(clientReqReply->key, key, keySize);
  clientReqReply->keySize = keySize;

  clientReqReply->val = NULL;

  err = genericPushToCache(cacheClient->clientReqReplyLink, clientReqReply);
  if (err != 0) {
    return errIO;
  }

  pthread_mutex_lock(&cacheClient->cacheClientMutex);
  if (write(cacheClient->sockfd, sendBuff, sendBuffSize) == -1) {
    return errIO;
  }
  pthread_mutex_unlock(&cacheClient->cacheClientMutex);

  pthread_mutex_lock(&cacheClient->clientReqReplyLink->cacheMutex);

  while(clientReqReply->val == NULL) {}

  *pulledCo = clientReqReply;

  pthread_mutex_unlock(&cacheClient->clientReqReplyLink->cacheMutex);

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
  int err;

  char *readBuff = malloc(sizeof(char)*SERVER_BUFF_SIZE);
  char *respBuff = malloc(sizeof(char)*SERVER_BUFF_SIZE);
  if (readBuff == NULL || respBuff == NULL) {
    close(socket);
    pthread_exit(NULL);
  }

  cacheObject *tempCo;
  err = initCacheObject(&tempCo);
  if (err != 0) {
    close(socket);
    pthread_exit(NULL);
  }

  int readBuffSize = 0;
  uint16_t tempProtocolSize = 0;
  int readSizePointer = 0;
  int keySizeSize = sizeof(tempCo->keySize);
  int nElementParsed = 0;
  int leftOverSize = 0;
  char *leftOverBuff;
  char *tempReadBuff;
  cacheObject *copiedCo;

  // pull = 1, push = 2, pullReply = 3
  int opCode = 0;

  while(1) {
    readBuffSize = read(socket, readBuff, SERVER_BUFF_SIZE);
    if (readBuffSize == -1) {
      close(socket);
      pthread_exit(NULL);
    } else if (readBuffSize == 0) {
      break;
    }
    readSizePointer = 0;
    tempProtocolSize = 0;

    // merging memory
    if (leftOverSize > 0) {
      char *mergingMem = malloc(leftOverSize + readBuffSize);
      if (mergingMem == NULL) {
        close(socket);
        pthread_exit(NULL);
      }
      memcpy(mergingMem, leftOverBuff, leftOverSize);
      free(leftOverBuff);
      leftOverBuff = NULL;
      memcpy(mergingMem+leftOverSize, readBuff, readBuffSize);
      tempReadBuff = readBuff;
      readBuffSize += leftOverSize;
      readBuff = mergingMem;
    }

    if (readBuffSize >= sizeof(uint8_t) && nElementParsed == 0) {
      memcpy(&tempProtocolSize, readBuff, sizeof(uint8_t));
      readSizePointer += sizeof(uint8_t);
      opCode = tempProtocolSize;
      nElementParsed++;
    }

    if ((readBuffSize-readSizePointer) >= keySizeSize && nElementParsed == 1) {
      memcpy(&tempProtocolSize, readBuff+readSizePointer, keySizeSize);
      readSizePointer += keySizeSize;
      tempProtocolSize = ntohs(tempProtocolSize);
      nElementParsed++;
    }

    if ((readBuffSize-readSizePointer) >= tempProtocolSize && nElementParsed == 2) {
      tempCo->keySize = tempProtocolSize;
      tempCo->key = malloc(tempProtocolSize);
      if (tempCo->key == NULL) {
        close(socket);
        pthread_exit(NULL);
      }
      memcpy(tempCo->key, readBuff+readSizePointer, tempProtocolSize);
      readSizePointer += tempProtocolSize;
      nElementParsed++;
    }

    if ((readBuffSize-readSizePointer) >= keySizeSize && nElementParsed == 3) {
      memcpy(&tempProtocolSize, readBuff+readSizePointer, keySizeSize);
      readSizePointer += keySizeSize;
      tempProtocolSize = ntohs(tempProtocolSize);
      nElementParsed++;
    }

    if ((readBuffSize-readSizePointer) >= tempProtocolSize && nElementParsed == 4) {
      if (opCode == 2) {
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
      } else if (opCode == 1) {
        genericGetByKey(cache->localCache, tempCo->key, tempCo->keySize, tempCo);
        cacheReplyToPull(socket, tempCo);
      }
      nElementParsed++;
      if (leftOverSize > 0) {
        readBuff = tempReadBuff;
      }
      leftOverSize = 0;
      free(tempCo->val);
      tempCo->val = NULL;
    }
    if (nElementParsed < 5) {
      leftOverSize = readBuffSize-readSizePointer;
      leftOverBuff = malloc(leftOverSize*sizeof(char));
      memcpy(leftOverBuff, readBuff+readSizePointer, leftOverSize);
    }
    nElementParsed = 0;
  }
  freeCacheObjectDefault(tempCo);
  if (leftOverBuff != NULL) {
    free(leftOverBuff);
    leftOverBuff = NULL;
  }
  if (readBuff != NULL) {
    free(readBuff);
    readBuff = NULL;
  }
  return NULL;
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

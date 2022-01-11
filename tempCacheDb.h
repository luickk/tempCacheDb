
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

#define MAX_CACHE_SIZE 500

typedef struct {
  void *key;
  uint16_t keySize;

  void *val;
  uint16_t valSize;

  int deleteMarker;
} cacheObject;

typedef int (*keyCompare)(void*, void*, int);

typedef struct {
  int tmpSocket;
  int serverSocket;
  uint nCacheSize;

  char *name;
  keyCompare keyCmp;
  pthread_mutex_t cacheMutex;
  cacheObject **keyValStore;

  socklen_t addrSize;
  pthread_t pthread;
  struct sockaddr_in tempClient;
} tempCache;

typedef struct {
  int sockfd, connfd;
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
  errInit
};

int initTempCache(tempCache *cache, char *cacheName, keyCompare keyCmp) {
  cache->name = cacheName;
  cache->keyCmp = keyCmp;

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

int freeTempCache(tempCache *cache, char *cacheName) {
  for (int i = 0; i < cache->nCacheSize; i++) {
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
  for (int i = 0; i < cache->nCacheSize; i++) {
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

// TODO fix nCacheSize overflow
// cashObject must be properly allocated!
int genericPushToCache(tempCache *cache, cacheObject *cO) {
  cacheObject *tempCoRef;


  if (genericGetByKey(cache, cO->key, cO->keySize, &tempCoRef)) {
    pthread_mutex_lock(&cache->cacheMutex);
    // free(tempCoRef->key);
    // free(tempCoRef->val);
    free(tempCoRef);
    tempCoRef = cO;
    pthread_mutex_unlock(&cache->cacheMutex);
  } else {
    if (cache->nCacheSize >= MAX_CACHE_SIZE) {
      free(cache->keyValStore[0]->key);
      free(cache->keyValStore[0]->val);
      free(cache->keyValStore[0]);
    }
    pthread_mutex_lock(&cache->cacheMutex);
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
    pthread_mutex_unlock(&cache->cacheMutex);
  }
  return success;
}

int cacheClientConnect(tempCacheClient *cacheClient, char *addressString, int port) {
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

  return success;
}

int cacheClientPushObject(tempCacheClient *cacheClient, cacheObject *cO) {
  int sendBuffSize = sizeof(cO->keySize) + cO->keySize + sizeof(cO->valSize) + cO->valSize;
  int netByteOrderSize = 0;
  char *sendBuff = malloc(sizeof(char) * sendBuffSize);
  sendBuffSize = 0;
  netByteOrderSize = htons(cO->keySize);
  memcpy(sendBuff+sendBuffSize, &netByteOrderSize, sizeof(cO->keySize));
  sendBuffSize += sizeof(cO->keySize);
  memcpy(sendBuff+sendBuffSize, cO->key, cO->keySize);
  sendBuffSize += cO->keySize;
  netByteOrderSize = htons(cO->valSize);
  memcpy(sendBuff+sendBuffSize, &netByteOrderSize, sizeof(cO->valSize));
  sendBuffSize += sizeof(cO->valSize);
  memcpy(sendBuff+sendBuffSize, cO->val, cO->valSize);
  sendBuffSize += cO->valSize;

  write(cacheClient->sockfd, sendBuff, sendBuffSize);
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

  int readBuffSize = 0;
  uint16_t tempProtocolSize = 0;
  cacheObject *tempCo = malloc(sizeof(cacheObject));
  int nextIsKey = 1;
  while(1) {
    readBuffSize = read(socket, readBuff, SERVER_BUFF_SIZE);
    if (readBuffSize == -1) {
      close(socket);
      pthread_exit(NULL);
    }

    tempProtocolSize = 0;
    for (int iElement = 0; readBuffSize >= sizeof(tempCo->keySize); iElement++) {
      memcpy(&tempProtocolSize, readBuff, sizeof(tempCo->keySize));
      tempProtocolSize = ntohs(tempProtocolSize);
      // printf("fd: %d \n", tempProtocolSize);
      if (readBuffSize >= sizeof(tempCo->keySize)+tempProtocolSize) {
        if (nextIsKey) {
          tempCo->keySize = tempProtocolSize;
          tempCo->key = malloc(sizeof(char)*tempProtocolSize);
          memcpy(tempCo->key, readBuff, tempProtocolSize);
          nextIsKey = 0;
          printf("key: %.*s \n", tempProtocolSize, tempCo->key);
        } else {
          tempCo->valSize = tempProtocolSize;
          tempCo->val = malloc(sizeof(char)*tempProtocolSize);
          memcpy(tempCo->val, readBuff, tempProtocolSize);
          nextIsKey = 1;
        }
      }
      // printf("iElement: %d, k: %s, v: %s \n", iElement, (char*)tempCo->key, (char*)tempCo->val);
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

#include "include/tempCacheDb.h"

// todo fix push malloc issue

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

int freeSimpleCache(simpleCache **cache) {
  for (int i = 0; i < (*cache)->nCacheSize; i++) {
    (*cache)->freeCoFn((*cache)->keyValStore[i]);
    (*cache)->keyValStore[i] = NULL;
  }

  free(*cache);
  (*cache) = NULL;

  if(pthread_mutex_destroy(&(*cache)->cacheMutex) != 0) {
    return errFree;
  }

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
  freeSimpleCache(&cache->localCache);

  free(cache);
  cache = NULL;

  if(pthread_mutex_destroy(&cache->localCache->cacheMutex) != 0) {
    return errFree;
  }

  return success;
}

int initCacheObject(cacheObject **cO) {
  *cO = malloc(sizeof(cacheObject));
  if (cO == NULL) {
    return errMalloc;
  }
  (*cO)->val = NULL;
  (*cO)->key = NULL;
  return success;
}

int getCacheObject(simpleCache *localCache, void *key, int keySize, cacheObject *resultingCo) {
  assert(localCache);
  if (key == NULL || keySize == 0) {
    return errParam;
  }
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

int getCacheObjectRef(simpleCache *localCache, void *key, int keySize, cacheObject ***resultingCo) {
  assert(localCache);
  if (key == NULL || keySize == 0) {
    return errParam;
  }
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


int cpyCacheObject(cacheObject **dest, cacheObject *src) {
  int err;
  if (*dest != NULL && (*dest)->valSize == src->valSize) {
    memcpy((*dest)->key, src->key, (*dest)->keySize);
  }
  if (*dest != NULL && (*dest)->keySize == src->keySize) {
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
      memcpy((*dest)->val, src->val, src->valSize);
    }
    if (src->key != NULL) {
      (*dest)->key = malloc(src->keySize);
      if((*dest)==NULL) {
        return errMalloc;
      }

      (*dest)->keySize = src->keySize;
      memcpy((*dest)->key, src->key, src->keySize);
    }
  }

  return success;
}

int pushCacheObject(simpleCache *sCache, cacheObject *cO, cacheObject ***newCoRef) {
  assert(sCache);
  if (cO == NULL) {
    return errParam;
  }
  cacheObject **tempCoRef;
  if (getCacheObjectRef(sCache, cO->key, cO->keySize, &tempCoRef)) {
    pthread_mutex_lock(&sCache->cacheMutex);
    if (cO->val == NULL && cO->valSize == 0 && tempCoRef != NULL) {
      (*tempCoRef)->val = NULL;
      (*tempCoRef)->valSize = 0;
      return success;
    } else if (cO->val == NULL || cO->valSize == 0) {
      return errParam;
    }
    if ((*tempCoRef)->val == NULL  && tempCoRef != NULL) {
      (*tempCoRef)->val = malloc(cO->valSize);
      if ((*tempCoRef)->val == NULL) {
        return errMalloc;
      }
      (*tempCoRef)->valSize = cO->valSize;
    } else if (cO->valSize != (*tempCoRef)->valSize  && tempCoRef != NULL){
      (*tempCoRef)->val = realloc((*tempCoRef)->val, cO->valSize);
      if ((*tempCoRef)->val == NULL) {
        return errMalloc;
      }
      (*tempCoRef)->valSize = cO->valSize;
    }
    memcpy((*tempCoRef)->val, cO->val, (*tempCoRef)->valSize);
    if (newCoRef != NULL) {
      *newCoRef = tempCoRef;
    }
    pthread_mutex_unlock(&sCache->cacheMutex);
  } else {
    if (cO->val == NULL || cO->valSize == 0) {
      return errParam;
    }
    pthread_mutex_lock(&sCache->cacheMutex);
    if (sCache->nCacheSize <= MAX_CACHE_SIZE) {
      if (sCache->nCacheSize == 0) {
        sCache->keyValStore = malloc(sizeof(cacheObject*)*CACHEDB_SIZE_INCREASE);
        if (sCache->keyValStore == NULL) {
          return errMalloc;
        }
      } else if (sCache->nCacheSize % CACHEDB_SIZE_INCREASE == 0) {
        sCache->keyValStore = realloc(sCache->keyValStore, ((sCache->nCacheSize+1)*sizeof(cacheObject*))+(sizeof(cacheObject*)*CACHEDB_SIZE_INCREASE));
        if (sCache->keyValStore == NULL) {
          return errMalloc;
        }
      }
      // cpyCacheObject(&sCache->keyValStore[sCache->nCacheSize], cO);
      cacheObject *toPush;
      int err = initCacheObject(&toPush);
      if (err != 0) {
        return err;
      }

      toPush->val = malloc(cO->valSize);
      if(toPush->val==NULL) {
        return errMalloc;
      }

      toPush->valSize = cO->valSize;
      memcpy(toPush->val, cO->val, cO->valSize);

      toPush->key = malloc(cO->keySize);
      if(toPush->key==NULL) {
        return errMalloc;
      }

      toPush->keySize = cO->keySize;
      memcpy(toPush->key, cO->key, cO->keySize);

      sCache->keyValStore[sCache->nCacheSize] = toPush;

      if (newCoRef != NULL) {
        *newCoRef = &sCache->keyValStore[sCache->nCacheSize];
      }
      sCache->nCacheSize++;
    }
    pthread_mutex_unlock(&sCache->cacheMutex);
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

void clientReqReplyLinkFree(cacheObject *cO) {
  if (cO->key != NULL)
    free(cO->key);
    cO->key = NULL;
  if (cO->val != NULL)
    if (((struct clientReqReplyLinkVal*)cO->val)->val != NULL)
      free(((struct clientReqReplyLinkVal*)cO->val)->val);
      ((struct clientReqReplyLinkVal*)cO->val)->val = NULL;
    free(cO->val);
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

  int err = initSimpleCache(&(*cacheClient)->clientReqReplyLink, clientReqReplyLinkKeyCmp, clientReqReplyLinkFree);
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

void listenDbCleanUp(void *arg) {
  if (arg == NULL) {
    return;
  }
  struct cacheClientListenDbCleanUpToFree *args = (struct cacheClientListenDbCleanUpToFree*) arg;
  if (args->readBuff != NULL) {
    free(args->readBuff);
  }
  if (args->respBuff != NULL) {
    free(args->respBuff);
  }
  if (args->args != NULL) {
    free(args->args);
  }
  if (args->mergingBuff != NULL) {
    free(args->mergingBuff);
  }
  if (args->tempCo != NULL) {
    freeCacheObjectDefault(args->tempCo);
  }
  if (args->leftOverBuff != NULL) {
    free(args->leftOverBuff);
  }
  free(args);
}

void *cacheClientListenDb(void *arg) {
  if (arg == NULL) {
    return NULL;
  }
  struct pthreadClientHandleArgs *args = (struct pthreadClientHandleArgs*) arg;
  tempCacheClient *cacheClient = args->cache;
  int socket = args->socket;
  int err;

  char *readBuffRestore = NULL;
  char *readBuff = malloc(SERVER_BUFF_SIZE);
  readBuffRestore = readBuff;
  if (readBuff == NULL) {
    close(socket);
    pthread_exit(NULL);
  }

  cacheObject *tempCo = malloc(sizeof(cacheObject));
  if (tempCo == NULL) {
    close(socket);
    pthread_exit(NULL);
  }
  tempCo->val = NULL;
  tempCo->key = NULL;

  cacheObject **tempCoReqRepRef;

  int readBuffSize = 0;
  uint16_t tempProtocolSize = 0;
  int readSizePointer = 0;
  int keySizeSize = sizeof(tempCo->keySize);
  int nElementParsed = 0;
  int leftOverSize = 0;
  int mergingBuffSize = 0;
  int mergingBuffAlloc = 0;
  char *leftOverBuff = NULL;
  char *mergingBuff = NULL;

  // pull = 1, push = 2, pullReply = 3
  int opCode = 0;

  struct cacheClientListenDbCleanUpToFree *toFree = malloc(sizeof(struct cacheClientListenDbCleanUpToFree));
  toFree->readBuff = readBuff;
  toFree->respBuff = NULL;
  toFree->args = args;
  toFree->mergingBuff = mergingBuff;
  toFree->tempCo = tempCo;
  toFree->leftOverBuff = leftOverBuff;
  pthread_cleanup_push(listenDbCleanUp, (void*)toFree);
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
      mergingBuffSize = leftOverSize + readBuffSize;
      if (mergingBuffSize < SERVER_BUFF_SIZE) {
        int diff = SERVER_BUFF_SIZE - mergingBuffSize;
        mergingBuffSize += diff;
      }
      if (mergingBuffAlloc) {
        mergingBuff = realloc(mergingBuff, mergingBuffSize);
      } else {
        mergingBuff = malloc(mergingBuffSize);
        mergingBuffAlloc = 1;
      }
      if (mergingBuff == NULL) {
        close(socket);
        pthread_exit(NULL);
      }

      memcpy(mergingBuff, leftOverBuff, leftOverSize);
      free(leftOverBuff);
      leftOverBuff = NULL;

      memcpy(mergingBuff+leftOverSize, readBuff, readBuffSize);

      readBuffSize += leftOverSize;
      readBuff = mergingBuff;
    }

    if (readBuffSize >= sizeof(uint8_t) && nElementParsed == 0) {
      memcpy(&tempProtocolSize, readBuff, sizeof(uint8_t));
      readSizePointer += sizeof(uint8_t);
      opCode = tempProtocolSize;
      nElementParsed++;
    }

    if ((readBuffSize-readSizePointer) >= keySizeSize && nElementParsed == 1) {
      if (readSizePointer+keySizeSize <= readBuffSize) {
        memcpy(&tempProtocolSize, readBuff+readSizePointer, keySizeSize);
      }
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
      if (readSizePointer+tempProtocolSize <= readBuffSize) {
        memcpy(tempCo->key, readBuff+readSizePointer, tempProtocolSize);
      }
      readSizePointer += tempProtocolSize;
      nElementParsed++;
    }

    if ((readBuffSize-readSizePointer) >= keySizeSize && nElementParsed == 3) {
      if (readSizePointer+keySizeSize <= readBuffSize) {
        memcpy(&tempProtocolSize, readBuff+readSizePointer, keySizeSize);
      }
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

        if (getCacheObjectRef(cacheClient->clientReqReplyLink, tempCo->key, tempCo->keySize, &tempCoReqRepRef)) {
          pthread_mutex_lock(&cacheClient->clientReqReplyLink->cacheMutex);
          struct clientReqReplyLinkVal* tempCoReqRepRefVal = (struct clientReqReplyLinkVal*)(*tempCoReqRepRef)->val;
          tempCoReqRepRefVal->valSize = tempCo->valSize;
          if (tempCoReqRepRefVal->val == NULL) {
            tempCoReqRepRefVal->val = malloc(tempCo->valSize);
            if (tempCoReqRepRefVal->val == NULL) {
              close(socket);
              pthread_exit(NULL);
            }
          } else if (tempCo->valSize != tempCoReqRepRefVal->valSize) {
            tempCoReqRepRefVal->val = realloc(tempCoReqRepRefVal->val, tempCo->valSize);
            if (tempCoReqRepRefVal->val == NULL) {
              close(socket);
              pthread_exit(NULL);
            }
          }
          memcpy(tempCoReqRepRefVal->val, tempCo->val, tempCo->valSize);
          tempCoReqRepRefVal->updated = 1;

          pthread_mutex_unlock(&cacheClient->clientReqReplyLink->cacheMutex);
        }
        nElementParsed++;
      }
    }
    if (nElementParsed < 5) {
      leftOverSize = readBuffSize-readSizePointer;
      if (leftOverSize > 0) {
        leftOverBuff = malloc(leftOverSize);
        if (readSizePointer+leftOverSize <= readBuffSize) {
          memcpy(leftOverBuff, readBuff+readSizePointer, leftOverSize);
        }
        readBuff = readBuffRestore;
      }
      readSizePointer = 0;
    } else if (nElementParsed == 5){
      if (leftOverSize > 0) {
        readBuff = readBuffRestore;
      }
      nElementParsed = 0;
      leftOverSize = 0;
      free(tempCo->val);
      tempCo->val = NULL;
      free(tempCo->key);
      tempCo->key = NULL;
    }
  }
  pthread_cleanup_pop(0);
  return NULL;
}

int cacheClientConnect(tempCacheClient *cacheClient, char *addressString, int port) {
  assert(cacheClient);
  if (addressString == NULL) {
    return errParam;
  }
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

  if(pthread_create(&cacheClient->pthread, NULL, cacheClientListenDb, (void*)clientArgs) != 0 ) {
    return errIO;
  }

  return success;
}

int cacheClientPushCacheObject(tempCacheClient *cacheClient, cacheObject *cO) {
  assert(cacheClient);
  if (cO == NULL) {
    return errParam;
  }
  int keySizeSize = sizeof(cO->keySize);
  int sendBuffSize = sizeof(uint8_t) + keySizeSize + cO->keySize + keySizeSize + cO->valSize;
  int netByteOrderSize = 0;
  char *sendBuff = malloc(sizeof(char) * sendBuffSize);
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
  if (sockfd < 0) {
    return errNet;
  }
  if (cO == NULL) {
    return errParam;
  }
  int keySizeSize = sizeof(cO->keySize);
  int sendBuffSize = sizeof(uint8_t) + keySizeSize + cO->keySize + keySizeSize + cO->valSize;
  int netByteOrderSize = 0;
  char *sendBuff = malloc(sizeof(char) * sendBuffSize);
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

int cacheClientPullCacheObject(tempCacheClient *cacheClient, void *key, int keySize, cacheObject **pulledCo) {
  assert(cacheClient);
  if (key == NULL || keySize == 0) {
    return errParam;
  }
  int keySizeSize = sizeof((*pulledCo)->keySize);
  int sendBuffSize = sizeof(uint8_t) + keySizeSize + keySize + keySizeSize;
  int netByteOrderSize = 0;
  cacheObject **cacheRef;
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

  clientReqReply->val = malloc(sizeof(struct clientReqReplyLinkVal));
  clientReqReply->valSize = sizeof(struct clientReqReplyLinkVal);
  ((struct clientReqReplyLinkVal*) clientReqReply->val)->updated = 0;

  err = pushCacheObject(cacheClient->clientReqReplyLink, clientReqReply, &cacheRef);
  if (err != 0) {
    return errIO;
  }
  freeCacheObjectDefault(clientReqReply);
  struct clientReqReplyLinkVal *volatile cacheRefVal = (struct clientReqReplyLinkVal*)(*cacheRef)->val;

  pthread_mutex_lock(&cacheClient->cacheClientMutex);
  if (write(cacheClient->sockfd, sendBuff, sendBuffSize) == -1) {
    return errIO;
  }
  pthread_mutex_unlock(&cacheClient->cacheClientMutex);

  pthread_mutex_lock(&cacheClient->clientReqReplyLink->cacheMutex);

  while(cacheRefVal->updated == 0) {}

  err = initCacheObject(pulledCo);
  if (err != 0) {
    return err;
  }
  (*pulledCo)->keySize = (*cacheRef)->keySize;
  if ((*cacheRef)->key != NULL) {
    (*pulledCo)->key = malloc((*cacheRef)->keySize);
    if ((*pulledCo)->key == NULL) {
      return errMalloc;
    }
    memcpy((*pulledCo)->key, (*cacheRef)->key, (*cacheRef)->keySize);
  }

  if ((*cacheRef)->val != NULL && ((struct clientReqReplyLinkVal*)(*cacheRef)->val)->val != NULL) {
    (*pulledCo)->valSize = ((struct clientReqReplyLinkVal*)(*cacheRef)->val)->valSize;
    (*pulledCo)->val = malloc(((struct clientReqReplyLinkVal*)(*cacheRef)->val)->valSize);
    if ((*pulledCo)->val == NULL) {
      return errMalloc;
    }
    memcpy((*pulledCo)->val, ((struct clientReqReplyLinkVal*)(*cacheRef)->val)->val, ((struct clientReqReplyLinkVal*)(*cacheRef)->val)->valSize);
  }

  pthread_mutex_unlock(&cacheClient->clientReqReplyLink->cacheMutex);

  free(sendBuff);

  return success;
}

void *cacheSurveillance(void *cacheP) {
  if (cacheP == NULL) {
    return NULL;
  }
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
  if (clientArgs == NULL) {
    return NULL;
  }
  struct pthreadClientHandleArgs *args = (struct pthreadClientHandleArgs*)clientArgs;
  tempCache *cache = args->cache;
  int socket = args->socket;
  int err;

  char *readBuffRestore = NULL;
  char *readBuff = malloc(SERVER_BUFF_SIZE);
  readBuffRestore = readBuff;
  char *respBuff = malloc(SERVER_BUFF_SIZE);
  if (readBuff == NULL || respBuff == NULL) {
    close(socket);
    pthread_exit(NULL);
  }

  cacheObject *tempCo = malloc(sizeof(cacheObject));
  if (tempCo == NULL) {
    close(socket);
    pthread_exit(NULL);
  }
  tempCo->val = NULL;
  tempCo->key = NULL;

  int readBuffSize = 0;
  uint16_t tempProtocolSize = 0;
  int readSizePointer = 0;
  int keySizeSize = sizeof(tempCo->keySize);
  int nElementParsed = 0;
  int leftOverSize = 0;
  int mergingBuffSize = 0;
  int mergingBuffAlloc = 0;
  char *leftOverBuff = NULL;
  char *mergingBuff = NULL;

  // pull = 1, push = 2, pullReply = 3
  int opCode = 0;

  struct cacheClientListenDbCleanUpToFree *toFree = malloc(sizeof(struct cacheClientListenDbCleanUpToFree));
  toFree->readBuff = readBuff;
  toFree->respBuff = respBuff;
  toFree->args = args;
  toFree->mergingBuff = mergingBuff;
  toFree->tempCo = tempCo;
  toFree->leftOverBuff = leftOverBuff;
  pthread_cleanup_push(listenDbCleanUp, (void*)toFree);
  while (1) {
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
      mergingBuffSize = leftOverSize + readBuffSize;
      if (mergingBuffSize < SERVER_BUFF_SIZE) {
        int diff = SERVER_BUFF_SIZE - mergingBuffSize;
        mergingBuffSize += diff;
      }
      if (mergingBuffAlloc) {
        mergingBuff = realloc(mergingBuff, mergingBuffSize);
      } else {
        mergingBuff = malloc(mergingBuffSize);
        mergingBuffAlloc = 1;
      }
      if (mergingBuff == NULL) {
        close(socket);
        pthread_exit(NULL);
      }

      memcpy(mergingBuff, leftOverBuff, leftOverSize);
      free(leftOverBuff);
      leftOverBuff = NULL;

      memcpy(mergingBuff+leftOverSize, readBuff, readBuffSize);

      readBuffSize += leftOverSize;
      readBuff = mergingBuff;
    }

    if (readBuffSize >= sizeof(uint8_t) && nElementParsed == 0) {
      memcpy(&tempProtocolSize, readBuff, sizeof(uint8_t));
      readSizePointer += sizeof(uint8_t);
      opCode = tempProtocolSize;
      nElementParsed++;
    }

    if ((readBuffSize-readSizePointer) >= keySizeSize && nElementParsed == 1) {
      if (readSizePointer+keySizeSize <= readBuffSize) {
        memcpy(&tempProtocolSize, readBuff+readSizePointer, keySizeSize);
      }
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
      if (readSizePointer+tempProtocolSize <= readBuffSize) {
        memcpy(tempCo->key, readBuff+readSizePointer, tempProtocolSize);
      }
      readSizePointer += tempProtocolSize;
      nElementParsed++;
    }

    if ((readBuffSize-readSizePointer) >= keySizeSize && nElementParsed == 3) {
      if (readSizePointer+keySizeSize <= readBuffSize) {
        memcpy(&tempProtocolSize, readBuff+readSizePointer, keySizeSize);
      }
      readSizePointer += keySizeSize;
      tempProtocolSize = ntohs(tempProtocolSize);
      nElementParsed++;
    }

    if ((readBuffSize-readSizePointer) >= tempProtocolSize && nElementParsed == 4) {
      if (opCode == 2) {
        tempCo->valSize = tempProtocolSize;
        tempCo->val = malloc(tempProtocolSize);
        if (tempCo->val == NULL) {
          close(socket);
          pthread_exit(NULL);
        }
        if (readSizePointer+tempProtocolSize <= readBuffSize) {
          memcpy(tempCo->val, readBuff+readSizePointer, tempProtocolSize);
        }
        // printf("(pushed) k: %.*s v: %.*s \n", copiedCo->keySize, (char*)copiedCo->key, copiedCo->valSize, (char*)copiedCo->val);
        pushCacheObject(cache->localCache, tempCo, NULL);
      } else if (opCode == 1) {
        getCacheObject(cache->localCache, tempCo->key, tempCo->keySize, tempCo);
        cacheReplyToPull(socket, tempCo);
      }
      nElementParsed++;
    }

    if (nElementParsed < 5) {
      leftOverSize = readBuffSize-readSizePointer;
      if (leftOverSize > 0) {
        leftOverBuff = malloc(leftOverSize);
        if (readSizePointer+leftOverSize <= readBuffSize) {
          memcpy(leftOverBuff, readBuff+readSizePointer, leftOverSize);
        }
        readBuff = readBuffRestore;
      }
      readSizePointer = 0;
    } else if (nElementParsed == 5){
      if (leftOverSize > 0) {
        readBuff = readBuffRestore;
      }
      nElementParsed = 0;
      leftOverSize = 0;
      free(tempCo->val);
      tempCo->val = NULL;
      free(tempCo->key);
      tempCo->key = NULL;
    }
  }
  pthread_cleanup_pop(0);
  return NULL;
}

int listenDb(tempCache *cache, int port) {
  assert(cache);
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

    struct pthreadClientHandleArgs *clientArgs = malloc(sizeof(struct pthreadClientHandleArgs));
    if (clientArgs == NULL) {
      close(tmpSocket);
      return errMalloc;
    }
    clientArgs->socket = tmpSocket;
    clientArgs->cache = (void*)cache;

    if(pthread_create(&cache->pthread, NULL, clientHandle, (void*)clientArgs) != 0 ) {
      free(clientArgs);
      return errIO;
    }
  }
  return success;
}

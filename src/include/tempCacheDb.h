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

struct clientReqReplyLinkVal {
  void *val;
  int valSize;
  int updated;
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

/* inits and frees */

// enable same socket push&pull
int initSimpleCache(simpleCache **cache, keyCompare keyCmp, freeCacheObject freeCoFn);
int freeSimpleCache(simpleCache **cache);

int initTempCache(tempCache **cache, keyCompare keyCmp, freeCacheObject freeCoFn);
int freeTempCache(tempCache **cache);

// initializes only the cacheObject struct, not its values!
int initCacheObject(cacheObject **cO);
// !assumes that ALL pointers are freeable!
void freeCacheObjectDefault(cacheObject *cO);

int initCacheClient(tempCacheClient **cacheClient);
int freeCacheClient(tempCacheClient **cacheClient);

/* local cache funcs */

// returns 1 if cO key has been found in the cache and 0 if not
// writes to Co val and valSize (mallocs if necessary)
// !resultingCo must not contain pointer to possibly allocated memory to prevent potential memory leak!
int genericGetByKey(simpleCache *localCache, void *key, int keySize, cacheObject *resultingCo);

// returns 1 if cO key has been found in the cache and 0 if not
// writes pointer to resultingCo param from the cache's Co array
// don't forget to use the caches mutex on the returned array pointer
// be carefull not to pass a allocated Co to resultingCo as it will not be freed
int genericGetCoRefByKey(simpleCache *localCache, void *key, int keySize, cacheObject ***resultingCo);

// CashObject must be properly allocated!
// Don't reuse pushed cache Object. The memory is now manged by the cache
// and could be freed or moved at any point in time
// newCoRef returns the address ref to the cacheObject whichs value has been overwritten by the pushed cO
int genericPushToCache(simpleCache *sCache, cacheObject *cO, cacheObject ***newCoRef);

int cpyCacheObject(cacheObject **dest, cacheObject *src);

void *cacheSurveillance(void *cacheP);

void *clientHandle(void *clientArgs);


/* private lib functions */

int clientReqReplyLinkKeyCmp(void *key1, void *key2, int size);

int cacheReplyToPull(int sockfd, cacheObject *cO);

/* cacheClient funcs */

// todo pthread free stack
void *cacheClientPullHandler(void *argss);

int cacheClientConnect(tempCacheClient *cacheClient, char *addressString, int port);

int listenDbServer(tempCache *cache, int port);

int cacheClientPushObject(tempCacheClient *cacheClient, cacheObject *cO);

int cacheClientPullByKey(tempCacheClient *cacheClient, void *key, int keySize, cacheObject **pulledCo);

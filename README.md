# tempCacheDb (C17)

Key/Val database with focus on temporary ultra fast data storage. Originally the project was intended to be a header only library, I dropped the idea after breaching a certain complexity and after I came up with plans to extend the project.

The idea is to provide a simple db which is able to store and manage key/val data at minimum latency in a temporary manner. Since the db is only meant for temporary data storage it's stored in memory (heap).

This type of db is suited for all kinds of data distribution in a local network or wireless type manner if latency and flexibility matters.

Up to now there is still room for massive improvements in memory efficiency and speed.

## Features

- local key/val database
- remote push/pull of data
- full concurrency support
(exmples can be found in test/)

To come:
- set function
- synchronisation between two caches

## Setup

To build the project, no dependencies have to be installed apart from cmake and standard gnu tools.
The lib is built to a static(just change the cmake script to compile to a dynamic lib) library and can be found in the build dir.

## Usage

### General

Since the db is meant to be generic a basic cacheDB object consists of a void pointer and size for the key and val.

```C
typedef struct {
  void *key;
  uint16_t keySize;

  void *val;
  uint16_t valSize;

  int deleteMarker;
} cacheObject;
```

In order to manage those rather generic cacheObjects(cO), certain functions(cO free and key compare) have to be defined first and then passed to the cacheDb init.

```C
// here an example for "strings"
int strKeyCmp(void *key1, void *key2, int size) {
  char *ckey1 = (char*)key1;
  char *ckey2 = (char*)key2;
  for (int i = 0; i <= size; i++) {
    if (ckey1[i]!=ckey2[i]) {
      return 0;
    }
  }
  return 1;
}

// in this example we only need to free the cacheObject struct because the key/val are string literals and cannot be freed
void freeCoFn(cacheObject *cO) {
  free(cO);
}
```

those are then passed to the cacheDB init.
There are two types of cache available in this library.
The simpleCache and the tempCache. Therefor the simple cache is really the simplest abstraction and provides only local usage. The tempCache extends the simpleCache and provides remote cache manipulation.
Internally the remote net functions make heavy use of the simpleCache but it can also be used externally.

### Local usage

To setup the cache call `initTempCache`.
```C
tempCache *cacheDB;
int err = initTempCache(&cacheDB, strKeyCmp, freeCoFn);
if (err != 0) {
  printf("err code %d \n", err);
  return 1;
}
```

To pull values from cache use `getCacheObject`. The results are then written to the queryCo (memory (for val) must not be allocated before). The queryCo must be freed be the user.

```C
if(getCacheObject(cache1->localCache, queryCo->key, queryCo->keySize, queryCo)) {
  printf("found val: %s \n", (char*)queryCo->val);
} else {
  printf("not found \n");
}
```

To push cacheObject to the cache use `pushCacheObject`. pushedCo is not freed.
The third argument returns the pointer to the actual pointer of the (new) cacheObject in the keyValStore of the cacheDB.

```C
err = pushCacheObject(cache1->localCache, pusedCo, NULL);
if (err != 0) {
  printf("err code %d \n", err);
  return 1;
}
```

### Remote usage

To manipulate a cache remotely the tempCacheClient is required.

```C
tempCacheClient *cacheClient;
err = initCacheClient(&cacheClient);
if (err != 0) {
  printf("cClientInit err code %d \n", err);
  return 1;
}

err = cacheClientConnect(cacheClient, "127.0.0.1", 8080);
if (err != 0) {
  printf("cClientConnect err code %d \n", err);
  return 1;
}
```

To push data to the remote cache use `cacheClientPushCacheObject`. pushedCo is not freed.
```C
err = cacheClientPushCacheObject(cacheClient, pushedCo);
if (err != 0) {
  printf("cacheClientPushO err code %d \n", err);
  return 1;
}
```

To pull data from the remote cache use `cacheClientPullCacheObject`. pulledCo val must not contain a pointer to allocated memory because the pulled val is written there.
```C
err = cacheClientPullCacheObject(cacheClient, pulledCo->key, pulledCo->keySize, &pulledCo);
if (err != 0) {
  printf("cacheClientPushO err code %d \n", err);
  return 1;
}
```

(additional details can be found in the header)

## Details

### Capacity

The size of the cacheDB can be defined in the header (`MAX_CACHE_SIZE`). If the max cache size is reached no new keys can be added to the db but existing key/vals can still be manipulated.

### Memory management

To ensure a predictable memory behaviour and prevent potential leaks the code has been analyzed with multiple static analyzing tools and tested/ surveilled in a multitude of scenarios. Threads ensure memory release by utilizing the pthread cleanup feature.

### Stability/ Err management

Stability and prevention of undefined behaviours was approached in a similar manner as the memory management but although thorough testing has been conducted, it cannot be guaranteed.
Similar to Go(lang) errors are defined in the header as an enum and returned by every function. Through that errors can almost always be recovered. Since I didn't have any real life use case error recovery specifically is not implemented (yet).

### Parallelisation

Every cache (simpleCache, tempCache) is fully atomic and every function can be used in a parallel manner and are ensured not to conflict by proper mutex utilisation.
Threads are only made use of if a new client connects (remotely) and for cache surveillance

### Networking/ Comm Protocol

The remote data exchange is made possible by tcp sockets. Therefor, if required, the cache which is manipulated can setup a tcp server which can then handles all incoming manipulation requests of the cache. <br>

- sizes are encoded in net byte order - the key/ val buffer are not checked for correct endianness

- Protocol structure <br>
`uint8_t opCode(pull=1, push=2, pullReply=3) - uint16_t (query)keySize - char[] (query)key - uint16_t(val) valSize - char[] val <br>``

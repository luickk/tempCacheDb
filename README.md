# tempCacheDb (C17)

Header only key/val database with focus on temporary ultra fast data storage.


## Comm Protocol

- sizes are encoded in net byte order - the key/ val buffer are not checked for correct endianness

- Client Send <br>
uint16_t(char) (query)keySize - char[] (query)key - uint16_t(val) valSize - char[] val <br>
If valSize equals 0 the its not a push but a query operation

- Client Rec <br>
uint16_t(char) queryKeySize - char[] QueryKey - uint16_t(val) valSize - char[] val <br>
client rec is only used for query responses, if valSize is 0, no cacheObject was found

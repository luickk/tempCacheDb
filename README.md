# tempCacheDb (C17)

Header only key/val database with focus on temporary ultra fast data storage.


## Comm Protocol

- sizes are encoded in net byte order - the key/ val buffer are not checked for correct endianness

- Protocol structure <br>
uint8_t opIdent(pull=1, push=2, pullReply=3) - uint16_t (query)keySize - char[] (query)key - uint16_t(val) valSize - char[] val <br> 
If valSize equals 0 the its not a push but a query operation

- Client Rec <br>
uint16_t queryKeySize - char[] QueryKey - uint16_t(val) valSize - char[] val <br>
client rec is only used for query responses, if valSize is 0, no cacheObject was found

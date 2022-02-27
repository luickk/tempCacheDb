# tempCacheDb (C17)

Key/Val database with focus on temporary ultra fast data storage.


## Comm Protocol

- sizes are encoded in net byte order - the key/ val buffer are not checked for correct endianness

- Protocol structure <br>
uint8_t opCode(pull=1, push=2, pullReply=3) - uint16_t (query)keySize - char[] (query)key - uint16_t(val) valSize - char[] val <br>
If valSize equals 0 the its not a push but a query operation

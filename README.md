# tempCacheDb (C17)

Header only key/val database with focus on temporary ultra fast data storage.


## Comm Protocol

- sizes are encoded in net byte order - the key/ val buffer are not checked for correct endianness

- Client Send <br>
uint16_t(char) (query)kSize - char[] (query)key - uint16_t(val) kSize - char[] val <br>
If kSize equals 0 the its not a push but a query operation

- Client Rec <br>
uint16_t(char) queryKsize - char[] QueryKey - uint16_t(val) kSize - char[] val <br>
client rec is only used for query responses

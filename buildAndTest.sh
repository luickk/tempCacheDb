#!/usr/bin/env bash
# rm -r build
mkdir build
cd build
cmake ..
make
./../build/test/tempCacheDbTest

#!/bin/bash

mkdir build
cd build
cmake ..
make ubox

cd ..
mkdir -p deb_dist/usr/local/include/libubox
mkdir -p deb_dist/usr/local/lib
cp *.h deb_dist/usr/local/include/libubox/
cp build/libubox.so deb_dist/usr/local/lib/

rm -r build


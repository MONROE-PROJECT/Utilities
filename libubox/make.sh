#!/bin/bash

set -e
set -x

mkdir build
cd build
cmake ..
make

cd ..
mkdir -p deb_dist/usr/local/include
mkdir -p deb_dist/usr/local/lib
cp *.h deb_dist/usr/local/include/
cp build/libubox.so deb_dist/usr/local/lib/
cp build/libblobmsg_json.so deb_dist/usr/local/lib/
cp build/libjson_script.so deb_dist/usr/local/lib/

rm -r build


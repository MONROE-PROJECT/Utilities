#!/bin/bash

set -x
set -e

mkdir build
cd build
cmake ..
make uci cli

cd ..
mkdir -p deb_dist/usr/local/include
mkdir -p deb_dist/usr/local/lib
mkdir -p deb_dist/usr/local/bin
cp uci.h uci_config.h uci_blob.h ucimap.h deb_dist/usr/local/include/
cp build/libuci.so deb_dist/usr/local/lib/
cp build/uci deb_dist/usr/local/bin/

rm -r build


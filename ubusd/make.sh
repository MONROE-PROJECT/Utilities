#!/bin/bash

set -e
set -x

mkdir build
cd build
cmake ..
make

cd ..
mkdir -p deb_dist/usr/local/include/libubus
mkdir -p deb_dist/usr/local/lib
cp *.h deb_dist/usr/local/include/libubus/
cp build/libubus.so deb_dist/usr/local/lib/

chmod +x ubus ubusd
mkdir -p deb_dist/bin/
cp ubus ubusd deb_dist/bin/ubus
mkdir -p deb_dist/sbin/
cp ubus ubusd deb_dist/bin/ubusd

rm -r build


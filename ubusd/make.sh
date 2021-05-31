#!/bin/bash

set -e
set -x

cd ../libubox
mkdir build
cd build
cmake ..
make ubox
sudo make install
cd ../../ubusd

mkdir build
cd build
cmake ..
make

chmod +x ubus ubusd

cd ..
mkdir -p deb_dist/usr/local/include/libubus
mkdir -p deb_dist/usr/local/lib
cp *.h deb_dist/usr/local/include/libubus/
cp build/libubus.so deb_dist/usr/local/lib/

mkdir -p deb_dist/bin/
cp build/ubus deb_dist/bin/
mkdir -p deb_dist/sbin/
cp build/ubusd deb_dist/sbin/

rm -r build


#!/usr/bin/bash
# 
# This script is used to update the docker container on the fly so that auto-config builds in Nidas.
#

# install libusb-1.0
apt-get -y install libusb-1.0am:armhf

# install ftdi
# get build essentials
apt-get install -y cmake
pushd /home/builder/nidas/src/libftdi1-1.4
rm -rf build-armhf
mkdir build-armhf && pushd build-armhf
cmake  -DCMAKE_TOOLCHAIN_FILE=/home/builder/nidas/scripts/crosstoolchain-armhf.cmake -DCMAKE_INSTALL_PREFIX="/usr" ../
make && make install



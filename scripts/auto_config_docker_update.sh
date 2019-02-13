#!/bin/bash
# 
# This script is used to update the docker container on the fly so that auto-config builds in Nidas.
#

#upgrade scons
apt-get -y --only-upgrade install eol-scons

# add boost-regex
apt-get -y install libboost-regex1.55-dev:armhf libboost-filesystem1.55-dev:armhf

# add libi2c-dev as original docker image didn't have correct one
# don't need an armhf variant, as all the functions are in-line
# in a single header.
apt-get -y install libi2c-dev

# install libusb-1.0
apt-get -y install libusb-1.0:armhf

# install ftdi
# get build essentials
apt-get install -y cmake
pushd /home/builder/nidas/src/libftdi1-1.4
rm -rf build-armhf
mkdir build-armhf && pushd build-armhf
cmake  -DCMAKE_TOOLCHAIN_FILE=/home/builder/nidas/scripts/crosstoolchain-armhf.cmake -DCMAKE_INSTALL_PREFIX="/usr" ../
make && make install



#!/bin/bash

# Start a Docker container to do cross-building of NIDAS for various
# non-x86_64, non-redhat systems.

if [ $# -lt 1 ]; then
    echo "usage: ${0##*/} [ armel | armhf | armbe | viper=armel | titan=armel | rpi2=armhf ]"
    exit 1
fi

# Check build container arch match. NOTE: $CROSS_ARCH comes in from the docker container.
echo "arch arg is: $1"

if [[ ! " armel armhf " =~ " $1 " ]]; then 
    echo "Build host cross compile arch is not armel or armhf..."
    exit 2
fi

export buildDir="build-$1"
export NAME=libftdi1
export VERSION=1.4
export URL="http://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.4.tar.bz2"

#Download it
if [ ! -f "${NAME}_${VERSION}.tar.bz2" ]
then
    echo "Downloading: $URL..."
    wget "$URL" 
fi

if [[ ! -d "${NAME}_${VERSION}" ]] ; then
    tar xjvf ${NAME}-${VERSION}.tar.bz2
fi

cd libftdi1-${VERSION}
rm -rf $buildDir
mkdir -p $buildDir && cd $buildDir
cmake -DCMAKE_TOOLCHAIN_FILE=../crosstoolchain-$1.cmake -DCMAKE_INSTALL_PREFIX="/usr" ../
make

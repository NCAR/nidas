#!/bin/bash

# Start a Docker container to do cross-building of NIDAS for various
# non-x86_64, non-redhat systems.

if [ $# -lt 1 ]; then
    echo "usage: ${0##*/} [ armel | armhf | armbe | viper=armel | titan=armel | rpi2=armhf ]"
    exit 1
fi

# Check build container arch match. NOTE: $CROSS_ARCH comes in from the docker container.
echo "arch arg is: $1"
echo "container cross arch arg is: $CROSS_ARCH"

if [[ -z $CROSS_ARCH ]]; then 
    export CROSS_ARCH=`uname --m`
fi

echo "CROSS_ARCH matches arch arg [[ "$CROSS_ARCH" -ne "$1" ]]"
if [[ "$CROSS_ARCH" != $1 ]]; then
    echo "Build host cross compile arch is $CROSS_ARCH, but $1 was specified on the command line. These must match. Are you running the right container?"
    exit 2
fi

export buildDir="build-$CROSS_ARCH"
export NAME=libftdi
export VERSION=1.4
export DEBVERSION=${VERSION}-1
export URL="http://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.4.tar.bz2"

#Download it
if [ ! -f "${NAME}_${VERSION}.orig.tar.bz2" ]
then
    echo "Downloading: $URL..."
    wget "$URL" -O ${NAME}_${VERSION}.orig.tar.bz2
fi

if [[ ! -d "${NAME}_${VERSION}" ]] ; then
    tar xjvf ${NAME}_${VERSION}.orig.tar.bz2
fi

cd libftdi1-${VERSION}
rm -rf $buildDir
mkdir -p $buildDir && cd $buildDir
cmake -DCMAKE_TOOLCHAIN_FILE=/home/builder/nidas/scripts/crosstoolchain-$CROSS_ARCH.cmake -DCMAKE_INSTALL_PREFIX="/usr" ../
make package
mv libftdi1-$VERSION.deb libftdi1-$VERSION-$CROSS_ARCH.deb

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
if [ ! -f "${NAME}-${VERSION}.tar.bz2" ]
then
    echo "Downloading: $URL..."
    wget "$URL" 
fi

if [[ ! -d "${NAME}-${VERSION}" ]] ; then
    tar xjvf ${NAME}-${VERSION}.tar.bz2
fi

cd ${NAME}-${VERSION}

# need to build this as place independant code, so add -fPIC flag to CMAKE_C_FLAGS
grep -e "-fPIC" CMakeLists.txt
#echo "grep results: $?"
grepResults=$?
if [[ "$grepResults" == 1 ]] ; then
    echo "Didn't find -fPIC, adding it..."
    sed -i 's/add_definitions(-Wall)/add_definitions(-Wall -fPIC)/' CMakeLists.txt
elif [[ "$grepResults" == 0 ]] ; then
    echo "-fPIC already in CMakeLists.txt. Carry on..."
elif [[ "$grepResults" == 2 ]] ; then
    echo "General grep error when searching CMakeLists.txt for -fPIC flag"
    exit 100
fi

if [[ "$?" == 0 ]] ; then

    # always a clean build...
    rm -rf $buildDir
    mkdir -p $buildDir && cd $buildDir
    cmake -DCMAKE_TOOLCHAIN_FILE=/home/builder/libftdi/crosstoolchain-$CROSS_ARCH.cmake -DCMAKE_INSTALL_PREFIX="/usr" ../

    # at this point we just need the static lib, not the package
    make
    #make package
    #mv libftdi1-$VERSION.deb libftdi1-$VERSION-$CROSS_ARCH.deb
else
    echo "Failed to add -fPIC to CMakeLists.txt"
fi

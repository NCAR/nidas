#!/bin/bash

# Build libftdi in 

if [ $# -lt 1 ]; then
    echo "usage: ${0##*/} [ host | armel | armhf | viper=armel | titan=armel | rpi2=armhf ]"
    exit 1
fi

# Check build container arch match. 
echo "arch arg is: $1"

if [[ " host " =~ " $1 " ]]; then 
    export cmakeTargetArch=""
elif [[ " armel armhf " =~ " $1 " ]]; then 
    cmakeTargetArch="-DCMAKE_TOOLCHAIN_FILE=../../crosstoolchain-$1.cmake"
else
    echo "Build target compile arch is not host, armel or armhf..."
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

echo "Only want to build the DEB pkg..."
echo "Find RPM and take it out..."
sed -i 's/DEB;RPM/DEB/' CMakeLists.txt

if [[ "$?" == 0 ]] ; then

    # always a clean build...
    rm -rf $buildDir
    mkdir -p $buildDir && cd $buildDir
    cmake $cmakeTargetArch -DCMAKE_INSTALL_PREFIX="/usr" ../

    make package
    mv libftdi1-$VERSION.deb libftdi1-$VERSION-$CROSS_ARCH.deb
    sudo dpkg -i libftdi1-$VERSION-$CROSS_ARCH.deb
else
    echo "Failed to add -fPIC to CMakeLists.txt"
fi

#!/bin/bash

# Build ublox library
export buildDir="build"
export NAME=cc.ublox.generated
export URL="https://github.com/arobenko/cc.ublox.generated.git"

#Clone it
if [ ! -d "${NAME}" ]
then
    echo "cloning: $URL..."
    git clone "$URL" 
fi

cd ${NAME}

# always a clean build...
rm -rf $buildDir
mkdir -p $buildDir && cd $buildDir
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="/usr/local" ../
make install

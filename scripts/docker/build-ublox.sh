#!/bin/bash

# Build ublox library
export buildDir="build"
export NAME=cc.ublox.generated
export URL="https://github.com/arobenko/cc.ublox.generated.git"
tag="v0.20.2"

#Clone it
rm -rf "${NAME}"
echo "cloning tag ${tag}: $URL..."
git clone --depth 1 --branch "$tag" "$URL" "$NAME"

# always a clean build...
cd ${NAME}
rm -rf $buildDir
mkdir -p $buildDir && cd $buildDir
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="/usr/local" ../
make install

#!/bin/sh

# Build a docker image of Debian Jessie for doing C/C++ debian builds for
# various targets, such as armel and armhf (RPi).
# The image is built from the Dockerfile.cross_arm in this directory.

set -e

# images will have a "builder" user, and a group eol:1342

# You must do a "docker login $dockuser" before doing
# the docker push
dockuser=ncar

group=eol
gid=1342
version=1
tag=jessie_v$version

cacheFlag="--no-cache"
if [[ "$1" == "--use-cache"  ]] ; then
    cacheFlag=""
    echo "Using docker cache for build"
else
    echo "NOT using docker cache for build"
fi

hostarch=armel
image=nidas-build-debian-$hostarch
echo "arch is $hostarch"
echo "image is $image"
echo "tagged image is $dockuser/$image:$tag"
docker build $cacheFlag -t $image \
    --build-arg group=$group \
    --build-arg gid=$gid \
    --build-arg hostarch=$hostarch \
    -f Dockerfile.cross_arm .
# Only tag and push if the build worked
if [[ "$?" -eq 0 ]] ; then
    docker tag  $image $dockuser/$image:$tag
    docker push $dockuser/$image:$tag
fi

hostarch=armhf
image=nidas-build-debian-$hostarch
echo "arch is $hostarch"
echo "image is $image"
echo "tagged image is $dockuser/$image:$tag"
docker build $cacheFlag -t $image \
    --build-arg group=$group \
    --build-arg gid=$gid \
    --build-arg hostarch=$hostarch \
    -f Dockerfile.cross_arm .
# Only tag and push if the build worked
if [[ "$?" -eq 0 ]] ; then
    docker tag  $image $dockuser/$image:$tag
    docker push $dockuser/$image:$tag
fi

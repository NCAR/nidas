#!/bin/bash

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
version=2
tag=buster_v$version

cacheFlag="--no-cache"
if [[ "$1" == "--use-cache" ]] || [[ "$1" == "cache" ]] ; then
    cacheFlag=""
    echo "Using docker cache for build"
else
    echo "NOT using docker cache for build"
fi

# Ignoring armel because that's for Titans and Vipers, and we
# do not expect to be using these DSMs in future ISFS projects.
if false; then
    hostarch=armel
    image=$dockuser/nidas-build-debian-$hostarch
    echo "arch is $hostarch"
    echo "image is $image"
    docker build $cacheFlag -t $image \
        --build-arg group=$group \
        --build-arg gid=$gid \
        --build-arg hostarch=$hostarch \
        -f Dockerfile.cross_arm .
    # Only tag and push if the build worked
    if [[ "$?" -eq 0 ]] ; then
        docker tag  $image $image:$tag
        docker push $image:$tag
    fi
fi

hostarch=armhf
image=$dockuser/nidas-build-debian-$hostarch
echo "arch is $hostarch"
echo "image is $image"
echo "tag is $tag"
sleep 5
docker build $cacheFlag -t $image \
    --build-arg group=$group \
    --build-arg gid=$gid \
    --build-arg hostarch=$hostarch \
    -f Dockerfile.buster_cross_arm .
# Only tag and push if the build worked
if [[ "$?" -eq 0 ]] ; 
then
    echo "tagging $image:latest with $tag"
    docker tag  $image:latest $image:$tag
    docker push $image:$tag
fi

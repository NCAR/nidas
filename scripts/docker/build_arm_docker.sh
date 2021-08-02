#!/bin/sh

# Build a docker image of Debian Jessie for doing C/C++ debian builds for
# various targets, such as armel and armhf (RPi).
# The image is built from the Dockerfile.cross_arm in this directory.

set -e

# You must do a "podman login $dockuser" before doing
# the docker push
dockuser=ncar

version=2
tag=jessie_v$version

if [[ "$1" == "--no-cache"  ]] ; then
    cacheFlag="--no-cache"
    echo "NOT using podman cache for build"
else
    cacheFlag=""
    echo "Using podman cache for build"
fi

hostarch=armel
image=nidas-build-debian-$hostarch
echo "arch is $hostarch"
echo "image is $image"
echo "tagged image is $dockuser/$image:$tag"
podman build $cacheFlag -t $image \
    --build-arg hostarch=$hostarch \
    -f Dockerfile.cross_arm .
# Only tag and push if the build worked
if [[ "$?" -eq 0 ]] ; then
    podman tag  $image $dockuser/$image:$tag
    podman push $dockuser/$image:$tag
fi

exit
# If we want the armhf RPi2 image

hostarch=armhf
image=nidas-build-debian-$hostarch
echo "arch is $hostarch"
echo "image is $image"
echo "tagged image is $dockuser/$image:$tag"
podman build $cacheFlag -t $image \
    --build-arg hostarch=$hostarch \
    -f Dockerfile.cross_arm .
# Only tag and push if the build worked
if [[ "$?" -eq 0 ]] ; then
    podman tag  $image $dockuser/$image:$tag
    podman push $dockuser/$image:$tag
fi

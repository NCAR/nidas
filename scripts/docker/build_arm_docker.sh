#!/bin/sh

# Build a docker image of Debian Jessie for doing C/C++ debian builds for
# various targets, such as armel and armhf (RPi).
# The image is built from the Dockerfile.cross_arm in this directory.

set -e

# images will have a "builder" user, and a group eol:1342

# You must do a "docker login $dockuser" before doing
# the docker push
dockuser=maclean

group=eol
gid=1342

image=debian-armel-cross:jessie 
arch=armel
docker build -t $image \
    --build-arg=group=$group --build-arg=gid=$gid \
    --build-arg=hostarch=$arch \
    -f Dockerfile.cross_arm .
docker tag  $image $dockuser/$image
docker push $dockuser/$image

image=debian-armhf-cross:jessie 
arch=armhf
docker build -t $image \
    --build-arg=group=$group --build-arg=gid=$gid \
    --build-arg=hostarch=$arch \
    -f Dockerfile.cross_arm .
docker tag  $image $dockuser/$image
docker push $dockuser/$image

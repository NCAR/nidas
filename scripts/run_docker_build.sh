#!/bin/sh

# Start a Docker container to do cross-building of NIDAS for various
# non-x86_64, non-redhat systems.

if [ $# -lt 1 ]; then
    echo "usage: ${0##*/} [ armel | armhf | armbe | viper=armel | titan=armel | rpi2=armhf | vulcan=armbe ]"
    exit 1
fi

case $1 in
    armel | viper | titan)
        image=maclean/debian-armel-cross:jessie
        ;;
    armhf | rpi2)
        image=maclean/debian-armhf-cross:jessie
        ;;
    armbe | vulcan)
        image=maclean/fedora25-armbe-cross:ael
        ;;
esac

# The nidas tree is the parent of the directory containing this script.
# It will be bind mounted to ~/nidas in the Docker container.
# The username in the container is "builder".

dir=$(dirname $0)
cd $dir/..

# If the image is not already loaded, docker run will pull the image
# from the Docker Hub.

docker run --rm --volume $PWD:/home/builder/nidas:rw,Z \
    -i -t $image /bin/bash


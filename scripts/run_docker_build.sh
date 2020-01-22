#!/bin/sh

# Start a Docker container to do cross-building of NIDAS for armhf on jessie or buster build containers

if [ $# -lt 1 ]; then
    echo "usage: ${0##*/} [ jessie | buster ]"
    exit 1
fi

case $1 in
    jessie)
        image=ncar/nidas-build-debian-armhf:jessie_v1
        ;;
    buster)
        image=ncar/nidas-build-debian-armhf:buster_v1
        ;;
    armel | viper | titan)
        image=ncar/nidas-build-debian-armel:jessie_v1
        ;;
    armhf | rpi2)
        image=ncar/nidas-build-debian-armhf:jessie_v1
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

# If the image is not already loaded, docker run will pull the image from
# the Docker Hub.

# Run the image as the current user and group=eol, so the image will have
# permission to write to the source tree and the files will have the right
# owner.  It is safe to disregard the "Who am I?" messages in the container
# caused by the user id not existing in the /etc/passwd file.

# group=$(id -g)
group=eol

echo "Running container as group $group, which must have rwx permission on $PWD and /opt/nidas"

set -x
exec docker run --rm --user `id -u`:$group \
    --volume $PWD:/home/builder/nidas:rw,Z \
    --volume /opt/nidas:/opt/nidas:rw,Z \
    --network=host \
    -i -t $image /bin/bash


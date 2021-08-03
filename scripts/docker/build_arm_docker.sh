#!/bin/sh

dockuser=ncar

usage() {
    echo "${0##*/} [--no-cache] [-p]
    --no-cache: dont' use podman cache when building images
    -p: push image to docker.io/$dockuser. You may need to do: podman login docker.io
    "
    exit 1
}

cacheFlag=""
dopush=false
while [ $# -gt 0 ]; do
    case $1 in
        --no-cache)
            cacheFlag="--no-cache"
            ;;
        -p)
            dopush=true
            ;;
        *)
            usage
            ;;
    esac
    shift
done

# Build a docker image of Debian Jessie for doing C/C++ debian builds for
# various targets, such as armel and armhf (RPi).
# The image is built from the Dockerfile.cross_arm in this directory.

set -e

version=2
tag=jessie_v$version

hostarch=armel
image=nidas-build-debian-$hostarch
echo "arch is $hostarch"
echo "image is $image"
echo "tagged image is $dockuser/$image:$tag"
    # --userns-uid-map-user=maclean \
    # --userns-uid-map=0:1000:1 \
    # --userns-uid-map=1:100000:65536 \
    # --userns-gid-map=0:1000:1 \
    # --userns-gid-map=1:100000:65536 \
podman build $cacheFlag -t $image \
    --build-arg hostarch=$hostarch \
    -f Dockerfile.cross_arm .
# Only tag and push if the build worked
if [[ "$?" -eq 0 ]] ; then
    podman tag  $image $dockuser/$image:$tag
    $dopush && podman push docker.io/$dockuser/$image:$tag
fi

exit
# If we want the armhf RPi2 image

hostarch=armhf
image=nidas-build-debian-$hostarch
echo "arch is $hostarch"
echo "image is $image"
echo "tagged image is $dockuser/$image:$tag"
docker build $cacheFlag -t $image \
    --build-arg hostarch=$hostarch \
    -f Dockerfile.cross_arm .
# Only tag and push if the build worked
if [[ "$?" -eq 0 ]] ; then
    docker tag  $image $dockuser/$image:$tag
    $dopush && docker push docker.io/$dockuser/$image:$tag
fi

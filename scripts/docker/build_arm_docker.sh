#!/bin/sh

dockerns=ncar   # namespace on docker.io

usage() {
    echo "${0##*/} [--no-cache] [-p] [armel] [armhf]
    --no-cache: dont' use podman cache when building images
    -p: push image to docker.io/$dockerns. You may need to do: podman login docker.io
    "
    exit 1
}

cacheFlag=""
dopush=false
arches=()
while [ $# -gt 0 ]; do
    case $1 in
        --no-cache)
            cacheFlag="--no-cache"
            ;;
        -p)
            dopush=true
            ;;
        arm*)
            arches=(${arches[*]} $1)
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

for arch in ${arches[*]}; do

    version=2
    tag=jessie_v$version

    image=nidas-build-debian-$arch
    echo "arch is $arch"
    echo "image is $image"
    echo "tagged image is $image:$tag"

    podman build $cacheFlag -t $image \
        --build-arg hostarch=$arch \
        -f Dockerfile.cross_arm .
    # Only tag and push if the build worked
    if [[ "$?" -eq 0 ]] ; then
        podman tag  $image $image:$tag
        if $dopush; then
            echo "Pushing $image:$tag docker://docker.io/$dockerns/$image:$tag"
            podman push $image:$tag docker://docker.io/$dockerns/$image:$tag && echo "push success"
        fi
    fi

done


#!/bin/sh

set -e

dockerns=ncar   # namespace on docker.io

usage() {
    echo "${0##*/} [--no-cache] [-p] [armel] [armhf]
    --no-cache: dont' use podman cache when building images
    -p: push image to docker.io/$dockerns. You may need to do: podman login docker.io
    "
    exit 1
}

dir=${0%%/*}
pushd $dir > /dev/null

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

for arch in ${arches[*]}; do

    version=2
    tag=jessie_v$version

    image=nidas-build-debian-$arch
    echo "arch is $arch"
    echo "image is $image"
    echo "tagged image is $dockerns/$image:$tag"

    podman build $cacheFlag -t $dockerns/$image:$tag \
        --build-arg hostarch=$arch \
        -f Dockerfile.cross_arm .

    # Only push if the build worked
    if [[ "$?" -eq 0 ]] ; then
        if $dopush; then
            echo "Pushing $dockerns/$image:$tag docker://docker.io/$dockerns/$image:$tag"
            podman push $dockerns/$image:$tag docker://docker.io/$dockerns/$image:$tag && echo "push success"
        fi
    fi

done


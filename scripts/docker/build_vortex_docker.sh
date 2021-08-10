#!/bin/sh

set -e

dockerns=ncar   # namespace on docker.io

if [ $# -lt 1 ]; then
    echo "Usage: $0 [-n] [-p] release
    -n: don't install local EOL packages, in case they don't exist yet
    -p: push image to docker.io/$dockerns. You may need to do: podman login docker.io
    for example:  $0 bionic"
    exit 1
fi

dopush=false
dolocal=yes

while [ $# -gt 0 ]; do
    case $1 in
    -n)
        dolocal=no
        ;;
    -p)
        dopush=true
        ;;
    *)
        tag=$1
        ;;
    esac
    shift
done

usage() {
    echo "${0##*/} [-p]
    "
    exit 1
}

image=nidas-build-ubuntu-i386

set -x

echo "image is $image"
echo "tagged image is $dockerns/$image:$tag"

podman build -t $dockerns/$image:$tag \
    --build-arg=dolocal=$dolocal \
    -f Dockerfile.ubuntu_i386_$tag .

if [[ "$?" -eq 0 ]] ; then
    if $dopush; then
        echo "Pushing $dockerns/$image:$tag docker://docker.io/$dockerns/$image:$tag" 
        podman push $dockerns/$image:$tag docker://docker.io/$dockerns/$image:$tag && echo "push success"
    fi
fi

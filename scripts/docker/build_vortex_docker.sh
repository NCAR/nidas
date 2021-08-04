#!/bin/sh

set -e

dockuser=ncar

if [ $# -lt 1 ]; then
    echo "Usage: $0 [-n] [-p] release
    -n: don't install local EOL packages, in case they don't exist yet
    -p: push image to docker.io/$dockuser. You may need to do: podman login docker.io
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
        release=$1
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

podman build -t $image \
    --build-arg=dolocal=$dolocal \
    -f Dockerfile.ubuntu_i386_$release .

    # --build-arg=user=$user --build-arg=uid=$uid \
    # --build-arg=group=$group --build-arg=gid=$gid \
if [[ "$?" -eq 0 ]] ; then
    podman tag $image $image:$release
    if $dopush; then
        echo "Pushing $image:$release docker://docker.io/$dockuser/$image:$release" 
        podman push $image:$release docker://docker.io/$dockuser/$image:$release && echo "push success"
    fi
fi

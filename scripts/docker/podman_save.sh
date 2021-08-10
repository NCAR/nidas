#!/bin/sh

usage() {
    echo "usage: ${0##*/} [-p] [ armel | armhf | armbe | bionic

    Do
        podman save --compress --format oci-dir -o ...
    to save an archive of an imabge.

    -p: pull image from docker.io"
    exit 1
}

dopull=false
dockerns=ncar   # namespace on docker.io

while [ $# -gt 0 ]; do

case $1 in
    armel)
        image=nidas-build-debian-armel
        fullname=$dockerns/$image:jessie_v2
        ;;
    armhf)
        image=nidas-build-debian-armhf
        fullname=$dockerns/$image:jessie_v2
        ;;
    armbe)
        image=nidas-build-ael-armbe
        fullname=$dockerns/$image:ael_v1
        ;;
    bionic)
        image=nidas-build-ubuntu-i386
        fullname=$dockerns/$image:bionic
        ;;
    -p)
        dopull=true
        ;;
    *)
        usage
        ;;
esac
    shift
done

[ -z $image ] && usage

$dopull && podman pull docker.io/$fullname

dir=$(dirname $0)
cd $dir

sdir=saved_images
[ -d $sdir ] || mkdir $sdir

podman save --compress --format oci-dir -o $sdir/$image.dir $fullname && \
    echo "$fullname saved to $sdir/$image.dir"

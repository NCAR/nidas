#!/bin/bash

set -e

usage() {
    echo "Usage: ${1##*/} arch"
    echo "arch is armel, armhf, arm64 or amd64"
    echo "Run this within a chroot"
    exit 1
}

if [ $# -lt 1 ]; then
    usage $0
fi

arch=amd64
while [ $# -gt 0 ]; do
    case $1 in
    armel)
        arch=$1
        PKG_CONFIG_PATH=/usr/lib/arm-linux-gnueabi/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig
        ;;
    armhf)
        arch=$1
        PKG_CONFIG_PATH=/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig
        ;;
    arm64)
        arch=$1
        PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig
        export CC=aarch64-linux-gnu-gcc
        export CXX=aarch64-linux-gnu-g++
        export LINK=aarch64-linux-gnu-g++
        ;;
    amd64)
        arch=$1
        PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig
        ;;
    *)
        usage $0
        ;;
    esac
    shift
done

sdir=$(dirname $0)
dir=$sdir/..
cd $dir

cd src
# scons BUILDS=$arch --config=force PKG_CONFIG_PATH=$PKG_CONFIG_PATH
scons BUILDS=$arch PKG_CONFIG_PATH=$PKG_CONFIG_PATH


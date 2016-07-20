#!/bin/bash

set -e

usage() {
    echo "Usage: ${1##*/} arch"
    echo "arch is armel, armhf or amd64"
    exit 1
}

if [ $# -lt 1 ]; then
    usage $0
fi

arch=amd64
while [ $# -gt 0 ]; do
    case $1 in
    armel|armhf|amd64)
        arch=$1
        ;;
    *)
        usage $0
        ;;
    esac
    shift
done

dist=$(lsb_release -c | awk '{print $2}')
if [ $arch == amd64 ]; then
    chr_name=${dist}-amd64-sbuild
else
    chr_name=${dist}-amd64-cross-${arch}-sbuild
fi
if ! schroot -l | grep -F chroot:${chr_name}; then
    echo "chroot named ${chr_name} not found"
    exit 1
fi

echo "Starting sbuild-shell $chr_name, which takes some time ..."
sudo sbuild-shell $chr_name


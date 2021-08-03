#!/bin/sh
# vim: set shiftwidth=4 softtabstop=4 expandtab:

# Start a Docker container with podman to do cross-building of NIDAS for various
# non-x86_64, non-redhat systems.

# image:
#   fulldnsname/namespace/imagename:tag
#   looks like fulldnsname is before first slash and imagename is after last,
#   so that namespace can contain slashes
#   fulldnsname/nspref/nsmid/nssuf/imagename:tag
#
# See
# /etc/containers/registries.conf   unqualified-search-registries
# /etc/containers/registries.conf.d/shortnames.conf 

dockuser=ncar

usage() {
    echo "usage: ${0##*/} [-p] [ armel | armhf | armbe | xenial | bionic | fedora | busybox ]
    -p: pull image from docker.io/$dockuser. You may need to do: podman login docker.io
    viper and titan are armel, rpi2 is armhf and vulcan is armbe
    xenial (Ubuntu 16) and bionic (Ubuntu 18) are i386 images for the vortex
    fedora: run a fedora image for testing
    busybox: run a busybox image for testing (a small image containing many useful commands)"
    exit 1
}

dopull=false
grpopt="--group-add=keep-groups"

while [ $# -gt 0 ]; do

case $1 in
    armel | viper | titan)
        image=nidas-build-debian-armel:jessie_v2
        ;;
    armhf | rpi2)
        image=ncar/nidas-build-debian-armhf:jessie_v1
        ;;
    armbe | vulcan)
        image=maclean/fedora25-armbe-cross:ael
        ;;
    xenial)
        image=ncar/nidas-build-ubuntu-i386:xenial
        ;;
    bionic)
        image=ncar/nidas-build-ubuntu-i386:bionic
        ;;
    fedora)
        image=$1:latest
        ;;
    busybox)
        image=$1:latest
        ;;
    debian)
        image=$1:latest
        ;;
    -p)
        dopull=true
        ;;
    -u)
        shift
        [ $# -lt 1 ] && usage
        duser=$1
        useropt="--user $duser"
        ;;
    *)
        usage
        ;;
esac
    shift
done

set -x

[ -z $image ] && usage

$dopull && podman pull docker.io/$dockuser/$image

# alpha name of image user

iuser=$(podman inspect $image --format "{{.User}}" | cut -f 1 -d :)

# user name on host
user=$(id -un)

if [ -z "$iuser" -o "$iuser" == root ]; then
    destmnt=/root
else
    destmnt=/home/$iuser
    useropt="--user=0:0"
fi

echo "host file systems will be mounted to $destmnt in the container"

# selinuxenabled && [ $(getenforce) == Enforcing ] && zopt=,Z
selinuxenabled && zopt=,Z

# The nidas tree is the parent of the directory containing this script.
# It will be bind mounted to $destmnt/nidas in the Docker container.
# The username in the container is "ads".

dir=$(dirname $0)
cd $dir/..

nowrite=$(find . \! -writable -print -quit)
if [ -n "$nowrite" ]; then
    echo "Warning, some files in $PWD are not writeable by $user
Do
    find $PWD \! -writable -ls
on the host to list them"
fi

# if embedded-linux is cloned next to nidas then mount that in the container
# for building kernels
embdir=$PWD/../embedded-linux
[ -d $embdir ] && embopt="--volume $embdir:$destmnt/embedded-linux:rw$zopt"

# if cmigits-nidas is cloned next to nidas then mount that too
cmig3dir=$PWD/../cmigits-nidas
[ -d $cmig3dir ] && cmig3opt="--volume $cmig3dir:$destmnt/cmigits-nidas:rw$zopt"

# if eol_scons is cloned next to nidas then mount that too
esconsdir=$PWD/../eol_scons
[ -d $esconsdir ] && esconsopt="--volume $esconsdir:$destmnt/eol_scons:rw$zopt"

# if embedded-daq is cloned next to nidas then mount that too
daqdir=$PWD/../embedded-daq
[ -d $daqdir ] && daqopt="--volume $daqdir:$destmnt/embedded-daq:rw$zopt"

# if nc-server is cloned next to nidas then mount that too
ncsdir=$PWD/../nc-server
[ -d $ncsdir ] && ncsopt="--volume $ncsdir:$destmnt/${ncsdir##*/}:rw$zopt"

repo=/net/ftp/pub/archive/software/debian
[ -d $repo ] && repoopt="--volume $repo:$repo:rw$zopt"

# If local user has a .gnupg, mount it in the container
gnupg=$(eval realpath ~)/.gnupg
# Note [ -d $gnupg ] may fail due to lack of group or world
# execute and read perms on the user's HOME directory.
# Docker can still mount it however.
gpgopt="--volume $gnupg:$destmnt/${gnupg##*/}:rw$zopt"

# Avoid version mismatch between gpg2/reprepro
# in the container and gpg-agent on the host.
# If running old gpg2 version 2.0 in the container, it
# will always start its own gpg-agent. If running gpg2 2.1
# and later, it can talk to gpg-agent on the host if it
# is also version 2.1 and later (and if user ids match
# and SELinux doesn't interfere).
# A version mismatch results in:
#     gpg: WARNING: server 'gpg-agent' is older than us (2.0.22 < 2.1.11)
gpgver=$(gpg2 --version | head -n 1 | awk '{print $NF}')
if [[ $gpgver == 2.0* ]]; then
    echo "Shutting down gpg-agent on the host"
    echo killagent | gpg-connect-agent
fi

echo "Volumes will be mounted to $destmnt in the container"

set -x
# --rm: remove container when it exits

exec podman run -it --rm $useropt $grpopt $keepopt \
    --volume $PWD:$destmnt/nidas:rw$zopt \
    $repoopt $embopt $gpgopt $daqopt $cmig3opt $esconsopt \
    $ncsopt \
    $image /bin/bash


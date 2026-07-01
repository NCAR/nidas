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

usage() {
    echo "usage: ${0##*/} [-h] [-p] [ armel | armhf | armbe | xenial | bionic | fedora | busybox ] [ cmd args ]
    -h: display this usage
    -p: pull image from docker.io. You may need to do: podman login docker.io

    viper and titan are armel, rpi2 is armhf and vulcan is armbe.
    xenial (Ubuntu 16) and bionic (Ubuntu 18) are i386 images for the vortex
    fedora: run a fedora image for testing.
    busybox: run a busybox image for testing (a small image containing many useful commands).

    cmd args: non-interactive command with arguments.
        Otherwise /bin/bash is run interactively.

    Examples:
    # Run shell under Ubuntu bionic
    $0 bionic 
    # Run package build script, non-interactive
    $0 bionic /root/nidas/scripts/build_dpkg.sh i386
    "

    exit 1
}

dopull=false
grpopt="--group-add=keep-groups"
dockerns=ncar   # namespace on docker.io
cmd=/bin/bash

# interactive options for podman run:
# -i: interactive, keep stdin open
# --tty: use pseudo-terminal for stdin of container. man podman-run says it
#       should only be used for when running interactively in a terminal.
interops="-i --tty"

while [ $# -gt 0 ]; do

case $1 in
    armel | viper | titan)
        image=$dockerns/nidas-build-debian-armel:jessie_v2
        shift
        break
        ;;
    armhf | rpi2)
        image=$dockerns/nidas-build-debian-armhf:jessie_v2
        shift
        break
        ;;
    armbe | vulcan)
        image=$dockerns/nidas-build-ael-armbe:ael_v1
        shift
        break
        ;;
    bionic)
        image=$dockerns/nidas-build-ubuntu-i386:bionic
        shift
        break
        ;;
    fedora)
        image=$1:latest
        shift
        break
        ;;
    busybox)
        image=$1:latest
        shift
        break
        ;;
    debian)
        image=$1:latest
        shift
        break
        ;;
    -p)
        dopull=true
        ;;
    -h)
        usage
        ;;
    *)
        usage
        ;;
esac
    shift
done

[ -z $image ] && usage

if [ $# -gt 0 ]; then
    cmd="$@"
    interops=
fi

$dopull && podman pull docker.io/$image

destmnt=/root

# If USER in the image is not root (i.e. one of our old docker-style images),
# then run it podman-style by adding a --user=0:0 option

# alpha name of user in image
iuser=$(podman inspect $image --format "{{.User}}" | cut -f 1 -d :)
[ -z "$iuser" -o "$iuser" == root ] || useropt="--user=0:0"

selinuxenabled && zopt=,Z

# The nidas tree is the parent of the directory containing this script.
# It will be bind mounted to $destmnt/nidas in the container.

dir=$(dirname $0)
cd $dir/..

# mount nidas to container
nidasvol="--volume $PWD:$destmnt/nidas:rw$zopt"

# if embedded-linux is cloned next to nidas then mount that in the container
# for building kernels
embdir=$PWD/../embedded-linux
[ -d $embdir ] && embvol="--volume $embdir:$destmnt/embedded-linux:rw$zopt"

# if cmigits-nidas is cloned next to nidas then mount that too
cmig3dir=$PWD/../cmigits-nidas
[ -d $cmig3dir ] && cmig3vol="--volume $cmig3dir:$destmnt/cmigits-nidas:rw$zopt"

# if eol_scons is cloned next to nidas then mount that too
esconsdir=$PWD/../eol_scons
[ -d $esconsdir ] && esconsvol="--volume $esconsdir:$destmnt/eol_scons:rw$zopt"

# if embedded-daq is cloned next to nidas then mount that too
daqdir=$PWD/../embedded-daq
[ -d $daqdir ] && daqvol="--volume $daqdir:$destmnt/embedded-daq:rw$zopt"

# if nc-server is cloned next to nidas then mount that too
ncsdir=$PWD/../nc-server
[ -d $ncsdir ] && ncsvol="--volume $ncsdir:$destmnt/${ncsdir##*/}:rw$zopt"

echo "Volumes will be mounted to $destmnt in the container"

# --rm: remove container when it exits

set -x

exec podman run $interops --rm $useropt $grpopt \
    $nidasvol $embvol $daqvol $cmig3vol $esconsvol $ncsvol $image $cmd

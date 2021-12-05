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
    viper and titan are armel, rpi2 is armhf and vulcan is armbe
    xenial (Ubuntu 16) and bionic (Ubuntu 18) are i386 images for the vortex
    fedora: run a fedora image for testing
    busybox: run a busybox image for testing (a small image containing many useful commands)

    cmd args: command and arguments to run, default: /bin/bash

    Examples:
    # Run shell under Ubuntu bionic
    ./${0##*/} bionic 
    # Run package build script
    ./${0##*/} bionic /root/nidas/script/build_dpkg.sh -I bionic i386
    "

    exit 1
}

dopull=false
grpopt="--group-add=keep-groups"
dockerns=ncar   # namespace on docker.io
cmd=/bin/bash

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

[ $# -gt 0 ] && cmd="$@"

$dopull && podman pull docker.io/$image

destmnt=/root

# If USER in the image is not root (i.e. one of our old docker-style images),
# then run it podman-style by adding a --user=0:0 option

# alpha name of user in image
iuser=$(podman inspect $image --format "{{.User}}" | cut -f 1 -d :)
[ -z "$iuser" -o "$iuser" == root ] || useropt="--user=0:0"

selinuxenabled && zopt=,Z

# The nidas tree is the parent of the directory containing this script.
# It will be bind mounted to $destmnt/nidas in the Docker container.

dir=$(dirname $0)
cd $dir/..

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

# check for EOL Debian repo
repo=/net/ftp/pub/archive/software/debian
[ -d $repo ] && repoopt="--volume $repo:$repo:rw$zopt"

# If local user has a .gnupg, mount it in the container
gnupg=$(eval realpath ~)/.gnupg
[ -d $gnupg ] && gpgopt="--volume $gnupg:$destmnt/${gnupg##*/}:rw$zopt"

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
interops="-i --tty"
exec podman run $interops --rm $useropt $grpopt \
    --volume $PWD:$destmnt/nidas:rw$zopt \
    $repoopt $embopt $gpgopt $daqopt $cmig3opt $esconsopt \
    $ncsopt \
    $image $cmd


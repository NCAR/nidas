#!/bin/sh
# vim: set shiftwidth=4 softtabstop=4 expandtab:

# Start a Docker container to do cross-building of NIDAS for various
# non-x86_64, non-redhat systems.

defuser=ads
defgroup=eol

user=$defuser
group=$defgroup

usage() {
    echo "usage: ${0##*/} [-u user] [-g group] [ armel | armhf | armbe | vortex ]

    viper and titan are armel, rpi2 is armhf and vulcan is armbe

If user id is numeric and not in /etc/passwd inside the container, you'll see a prompt of \"I have no name\", which isn't generally fatal. If it is a string and not in /etc/passwd, docker will fail.

The user in the container should have have write permission on the file systems that are bind mounted in the container, which includes this nidas tree. If is often easiest to make everything writable by the group.

Default user: $defuser, default group: $defgroup"
    exit 1
}

while [ $# -gt 0 ]; do

case $1 in
    armel | viper | titan)
        image=ncar/nidas-build-debian-armel:jessie_v1
        ;;
    armhf | rpi2)
        image=ncar/nidas-build-debian-armhf:jessie_v1
        ;;
    armbe | vulcan)
        image=maclean/fedora25-armbe-cross:ael
        ;;
    vortex)
        image=ncar/nidas-build-ubuntu-i386:xenial
        ;;
    -u)
        shift
        [ $# -lt 1 ] && usage
        user=$1
        ;;
    -g)
        shift
        [ $# -lt 1 ] && usage
        group=$1
        ;;
esac
    shift
done

[ -z $image ] && usage

selinuxenabled && [ $(getenforce) == Enforcing ] && zopt=,Z

# The nidas tree is the parent of the directory containing this script.
# It will be bind mounted to ~/nidas in the Docker container.
# The username in the container is "ads".

dir=$(dirname $0)
cd $dir/..

nowrite=$(find . \( \! -perm /020 -o \! -group $group \) -print -quit)
[ -n "$nowrite" ] && echo "Warning, some files in $PWD don't have $group group write access. Do \"chgrp -R $group $PWD; chmod -R g+ws $PWD\""

# If the image is not already loaded, docker run should(?) pull the
# image from the Docker Hub.

# if embedded-linux is cloned next to nidas then mount that in the container
# for building kernels
embdir=$PWD/../embedded-linux
[ -d $embdir ] && embopt="--volume $embdir:/home/ads/embedded-linux:rw$zopt"

# if cmigits-nidas is cloned next to nidas then mount that too
cmig3dir=$PWD/../cmigits-nidas
[ -d $cmig3dir ] && cmig3opt="--volume $cmig3dir:/home/ads/cmigits-nidas:rw$zopt"

# if embedded-daq is cloned next to nidas then mount that too
daqdir=$PWD/../embedded-daq
[ -d $daqdir ] && daqopt="--volume $daqdir:/home/ads/embedded-daq:rw$zopt"

repo=/net/ftp/pub/archive/software/debian
[ -d $repo ] && repoopt="--volume $repo:$repo:rw$zopt"

# Get name of the uid on this system
# hostuser=$(id -un $user 2> /dev/null)
hostuser=$(id -un $user)

# If local user has a .gnupg, mount it in the container
gnupg=$(eval realpath ~${hostuser})/.gnupg
# Note [ -d $gnupg ] may fail due to lack of group or world
# execute and read perms on the user's HOME directory.
# Docker can still mount it however.
gpgopt="--volume $gnupg:/home/ads/${gnupg##*/}:rw$zopt"

# In case of a version mismatch between gpg2/reprepro
# in the container and gpg-agent on the host, shutdown
# the gpg-agent on the host, so that it will be started
# in the container.
echo "Shutting down gpg-agent on the host"
echo killagent | gpg-connect-agent

echo "Running container as user $user, group $group.
If $user isn't a valid user on this docker host, writing files to
this host will fail unless the group exists, and group write is permitted
on files and directories.  Signing with gnupg probably won't work.

If $user isn't found in /etc/passwd in the container, docker will fail
if it is a name. If it is a number you'll see a \"I have no name\" prompt
in the container. Builds might work if file permissions are OK."

set -x
exec docker run --rm --user $user:$group \
    --volume $PWD:/home/ads/nidas:rw$zopt \
    --volume /opt/nidas:/opt/nidas:rw$zopt \
    $repoopt $embopt $gpgopt $daqopt $cmig3opt \
    --network=host \
    -i -t $image /bin/bash


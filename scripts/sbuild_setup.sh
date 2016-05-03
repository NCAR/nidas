#!/bin/bash

# set -e

if [ $# -lt 1 ]; then
    echo "Usage: ${0##*/} user"
    exit 1
fi

user=$1
userhome=$(eval readlink -f ~${user})
chrootbase=$userhome/chroots

[ -d $chrootbase ] || sudo mkdir -p $chrootbase

# sbuild-createchroot uses perl tempfile() in the current working directory,
# when creating the tarball.  If that isn't writable by root, e.g. NFS,
# things fail. chrootbase is presumably writable by root.
cd $chrootbase

dist=jessie
# GNU terms:
# 	 build: machine you are building on
# 	 host: machine you are building for
# 	 target: only when building cross tools, machine that
#	     GCC will build code for. In theory you could
#	     be building a GCC on build=A to run on host=B
#            to generate code for target=C, and A,B and C are all
#            different architectures.
#

buildarch=amd64
[ $(arch) != x86_64 ] && buildarch=unknown

for hostarch in armel armhf; do

    if [ $hostarch == $buildarch ]; then
        chr_suffix=-sbuild
    else
        chr_suffix=-cross-$hostarch-sbuild
    fi
    chr_name=${dist}-${buildarch}$chr_suffix
    tarball=$chrootbase/${chr_name}.tgz

    # bootstrapdir could be a temporary directory
    bootstrapdir=$chrootbase/$chr_name

    sudo rm -f /etc/schroot/chroot.d/${chr_name}*
    sudo rm -f $tarball
    sudo rm -rf $bootstrapdir

    # This installs a mess of packages for software building 
    # for amd64 that we don't need. Use --foreign?
    sudo sbuild-createchroot \
        --make-sbuild-tarball=$tarball --chroot-suffix=$chr_suffix \
	${dist} $bootstrapdir http://httpredir.debian.org/debian/

    # remove unique suffix on chroot config file. Should be only one file
    cfs=(/etc/schroot/chroot.d/${chr_name}-*)
    for cf in ${cfs[*]}; do
        sudo mv $cf ${cf%-*}
    done

    # add jenkins to the sbuild group
    grep -F sbuild /etc/group | grep -Fq $user || sudo sbuild-adduser $user
    # sudo usermod -G sbuild $user

    # Add a bind mount of the user's home directory to the chroot
    cf=/etc/schroot/sbuild/fstab
    bindmnt=$userhome
    grep -F $bindmnt $cf || sudo sh -c "echo $bindmnt $bindmnt none rw,bind 0 0 >> /etc/schroot/sbuild/fstab"
    # /scr/tmp
    grep -F $bindmnt $cf || sudo sh -c "echo /scr/tmp /scr/tmp none rw,bind 0 0 >> /etc/schroot/sbuild/fstab"

    # sudo sbuild-update -udcar ${dist}-$arch

    # This didn't didn't work, apt-get prompt for 'y' failed, probably a sudo issue.
    # Didn't seem to be a way to pass -y to apt-get
    # sudo sbuild-apt $chr_name apt-get install vim

    # instead run a shell in the chroot by hand
    sudo sbuild-shell $chr_name << EOD
        dpkg --add-architecture $hostarch
        apt-get -y install curl apt-utils gnupg2
        echo "deb ftp://ftp.eol.ucar.edu/pub/archive/software/debian/ jessie main" > /etc/apt/sources.list.d/eol.list 
        echo "deb http://emdebian.org/tools/debian/ jessie main" > /etc/apt/sources.list.d/crosstools.list 
        curl http://emdebian.org/tools/debian/emdebian-toolchain-archive.key | apt-key add -
        curl ftp://ftp.eol.ucar.edu/pub/archive/software/debian/conf/eol-prog.gpg.key | apt-key add -
        apt-get update
        # hack: install xmlrpc++-dev for build (amd64) architecture too, fakes out scons when
        # it looks for the library in /usr/lib
        apt-get -y install crossbuild-essential-${hostarch} git scons flex gawk devscripts pkg-config eol-scons libbz2-dev:${hostarch} libgsl0ldbl:${hostarch} libgsl0-dev:${hostarch} libcap-dev:${hostarch} libxerces-c-dev:${hostarch} libbluetooth-dev:${hostarch} xmlrpc++-dev:${hostarch} xmlrpc++-dev:${hostarch} libnetcdf-dev:${hostarch}
EOD

    if [ $hostarch == armel ]; then
        sudo sbuild-shell $chr_name << EOD
        apt-get -y install linux-headers-3.16.0-titan2:${hostarch} linux-headers-3.16.0-viper2:${hostarch}
EOD
    fi
done

# To use the schroot
#   cd /scr/tmp/...
# schroot --directory=$PWD --chroot=jessie-amd64-cross-armel-sbuild




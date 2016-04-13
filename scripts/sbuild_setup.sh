#!/bin/sh

if [ "$JENKINS_HOME" ]; then
    chrootbase=$JENKINS_HOME/chroots
else
    chrootbase=$HOME/chroots
fi

[ -d $chrootbase ] || mkdir -p $chrootbase

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

for hostarch in armel armhf; do

    # sbuild-createchroot uses perl tempfile() in the current working directory,
    # when creating the tarball.  If that isn't writable by root, e.g. NFS,
    # things fail. chrootbase is presumably writable by root.
    cd $chrootbase

    bootstrapdir=$chrootbase/${dist}-${hostarch}-sbuild
    tarball=${bootstrapdir}.tgz
    if [ -f $tarball ]; then
	sudo rm -f /etc/schroot/chroot.d/${dist}-${buildarch}-sbuild-*
	sudo rm -f /etc/schroot/chroot.d/${dist}-${buildarch}-${hostarch}-sbuild-*
	sudo rm -f $tarball
    fi

    [ -d $chrootdir ] && sudo rm -rf $bootstrapdir

    # This installs a mess of packages for software building 
    # for amd64 that we don't need. Use --foreign?
    sudo sbuild-createchroot --make-sbuild-tarball=$tarball \
	--chroot-suffix=-$hostarch \
	${dist} $bootstrapdir http://httpredir.debian.org/debian/
    # I: Please add any additional APT sources to /home/maclean/chroots/jessie-armel-sbuild/etc/apt/sources.list
    # I: schroot chroot configuration written to /etc/schroot/chroot.d/jessie-amd64-armel-lqE2LH.


    # add jenkins to the sbuild group
    grep -F sbuild /etc/group | grep -Fq $USER || sudo sbuild-adduser $USER
    # sudo usermod -G sbuild $USER

    # Add a bind mount of /var/lib/jenkins/workspace to the chroot
    if [ "$WORKSPACE" ]; then
	bindmnt=${WORKSPACE%/*}
    else
	bindmnt=${HOME}
    endif
    sudo sh -c "echo $bindmnt $bindmnt none rw,bind 0 0 >> /etc/schroot/sbuild/fstab"

    # sudo sbuild-update -udcar ${dist}-$arch

    # sudo sbuild-apt ${dist}-${arch} apt-get install vim
    # sudo sbuild-apt ${dist}-${arch}-sbuild apt-get install vim

    # Above didn't work, prompt for 'y' failed, probably a sudo issue.
    # instead run a shell by hand
    sudo sbuild-shell ${dist}-${buildarch}-${hostarch} << EOD
    dpkg --add-architecture $hostarch
    apt-get -y install curl apt-utils
    echo "deb ftp://ftp.eol.ucar.edu/pub/archive/software/debian/ jessie main" > /etc/apt/sources.list.d/eol.list 
    echo "deb http://emdebian.org/tools/debian/ jessie main" > /etc/apt/sources.list.d/crosstools.list 
    curl http://emdebian.org/tools/debian/emdebian-toolchain-archive.key | apt-key add -
    curl ftp://ftp.eol.ucar.edu/pub/archive/software/debian/conf/eol-prog.gpg.key | apt-key add -
    apt-get update
    # hack: install xmlrpc++-dev for build (amd64) architecture too, fakes out scons when
    # it looks for the library in /usr/lib
    apt-get -y install crossbuild-essential-${hostarch} git scons flex gawk devscripts pkg-config eol-scons libbz2-dev:${hostarch} libgsl0ldbl:${hostarch} libgsl0-dev:${hostarch} libcap-dev:${hostarch} libxerces-c-dev:${hostarch} libbluetooth-dev:${hostarch} xmlrpc++-dev:${hostarch} xmlrpc++-dev linux-headers-3.16.0-titan2:armel linux-headers-3.16.0-viper2:armel
EOD

    # as normal user, could not
    # sbuild-shell ${dist}-${arch}-sbuild
    # but could
    schroot -c ${dist}-${buildarch}-${hostarch} --directory=$HOME << EOD
cd git/nidas
scripts/build_dpkg.sh ${hostarch}
EOD

done



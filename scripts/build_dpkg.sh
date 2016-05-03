#!/bin/bash

set -e

key='<eol-prog@eol.ucar.edu>'

usage() {
    echo "Usage: ${1##*/} [-s] [-i repository ] arch"
    echo "-s: sign the package files with $key"
    echo "-c: build in a chroot"
    echo "-i: install them with reprepro to the repository"
    echo "-n: don't clean source tree, passing -nc to dpkg-buildpackage"
    echo "arch is armel, armhf or amd64"
    exit 1
}

if [ $# -lt 1 ]; then
    usage $0
fi

sign=false
arch=amd64
args="--no-tgz-check -sa"
use_chroot=false
while [ $# -gt 0 ]; do
    case $1 in
    -i)
        shift
        repo=$1
        ;;
    -c)
        use_chroot=true
        ;;
    -n)
        args="$args -nc -F"
        ;;
    -s)
        sign=true
        ;;
    armel)
        export CC=arm-linux-gnueabi-gcc
        arch=$1
        ;;
    armhf)
        export CC=arm-linux-gnueabihf-gcc
        arch=$1
        ;;
    amd64)
        arch=$1
        ;;
    *)
        usage $0
        ;;
    esac
    shift
done

args="$args -a$arch"

if $use_chroot; then
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
fi

sdir=$(dirname $0)
dir=$sdir/..
cd $dir

# This
#       export CC=arm-linux-gnueabi-gcc
#       debuild -aarmel ...
# reports:
# "dpkg-architecture: warning: specified GNU system type arm-linux-gnueabi
# does not match gcc system type x86_64-linux-gnu, try setting a correct
# CC environment variable".
# This is with debuild 2.15.3, and dpkg-buildpackage/dpkg-architecture 1.17.26.
# Various posts on the web indicate the warning is bogus and can be ignored.

# -tarm-linux-gnueabi does't seem to be passed correctly from debuild
# through dpkg-buildpackage to dpkg-architecture.  So use -aarmel.

# To check the environment set by dpkg-architecture:
#   dpkg-architecture -aarmel -c env

# create changelog
$sdir/deb_changelog.sh > debian/changelog

# The package tools report that using DEB_BUILD_HARDENING is obsolete,
# so we set the appropriate compiler and link options in the SConscript
# export DEB_BUILD_HARDENING=1

# These lintian errors are being suppressed:
#   dir-or-file-in-opt:  since the package installs things to /opt/nidas.
#       It would be nice to put them in /usr/bin, etc, but then some
#       of the nidas executables should probably be renamed to avoid possible
#       name conflicts with other packages, which could be a large number
#       if nidas is installed on a general-purpose system.
#       On a Debian jessie system I don't see any sub-directoryes of /usr/bin.
#   package-modifies-ld.so-search
#       Debian wants us to use -rpath instead. May want to re-visit this.
#   package-name-doesnt-match-sonames
#       This seems to be the result of having multiple libraries 
#       in one package?

karg=
if $sign; then
    if [ -z "$GPG_AGENT_INFO" -a -f $HOME/.gpg-agent-info ]; then
        . $HOME/.gpg-agent-info
        export GPG_AGENT_INFO
    fi
    karg=-k"$key"
else
    args="$args -us -uc"
fi

# clean old results
rm -f ../nidas_*.tar.xz ../nidas_*.dsc
rm -f $(echo ../nidas\*_{$arch,all}.{deb,build,changes})

# export DEBUILD_DPKG_BUILDPACKAGE_OPTS="$args"

if $use_chroot; then
    # as normal user, could not
    # sbuild-shell ${dist}-${arch}-sbuild
    # but could
    schroot -c $chr_name --directory=$PWD << EOD
        set -e
        # . $HOME/.gpg-agent-info
        # export GPG_AGENT_INFO
        debuild $args "$karg" \
            --lintian-opts --suppress-tags dir-or-file-in-opt,package-modifies-ld.so-search-path,package-name-doesnt-match-sonames
EOD
else
    debuild $args "$karg" \
        --lintian-opts --suppress-tags dir-or-file-in-opt,package-modifies-ld.so-search-path,package-name-doesnt-match-sonames
fi

# debuild puts results in parent directory
cd ..

if [ -n "$repo" ]; then
    umask 0002
    chngs=nidas_*_$arch.changes 
    pkgs=$(grep "^Binary:" $chngs | sed 's/Binary: //')
    debs=$(awk '/Checksums-Sha1:/,/Checksums-Sha256:/{print $3}' $chngs)
    archalls=
    for d in $debs; do
        if [[ $d =~ .*\.deb ]]; then
            arch=${d%.*}
            arch=${arch##*_}
            # echo "d=$d, arch=$arch"
            [ $arch == all ] && archalls+=" ${d%%_*}"
        fi
    done

    # nidas-daq is an architecture all package.
    # We want to specify -A "arch" when removing packages
    # so that packages of the same name for other architectures
    # are not deleted.
    # Doing a remove of nidas-daq with -A "source|$arch|all" doesn't work
    # however.  It is only removed when -A is not specified.
    # So I guess we have to look for architecture all packages
    # and remove them separately without a -A.
    flock $repo sh -c "
        reprepro -A 'source|$arch' -V -b $repo remove jessie $pkgs;
        reprepro -V -b $repo remove jessie $archalls;
        reprepro -V -b $repo deleteunreferenced;
        reprepro -V -b $repo include jessie $chngs"

    rm -f nidas_*_$arch.build nidas_*.dsc nidas_*.tar.xz nidas*_all.deb nidas*_$arch.deb $chngs

else
    echo "build results are in $PWD"
fi


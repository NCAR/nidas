#!/bin/bash

set -e

key='<eol-prog@eol.ucar.edu>'

usage() {
    echo "Usage: ${1##*/} [-s] [-i repository ] [-d dest] arch"
    echo "-s: sign the package files with $key"
    echo "-c: build in a chroot"
    echo "-i: install them with reprepro to the repository"
    echo "-n: don't clean source tree, passing -nc to dpkg-buildpackage"
    echo "-d: move the final packages in the given directory"
    echo "arch is armel, armhf or amd64"
    exit 1
}

if [ $# -lt 1 ]; then
    usage $0
fi

sign=false
arch=amd64
args="--no-tgz-check -sa"
dest=""
use_chroot=false
while [ $# -gt 0 ]; do
    case $1 in
    -i)
        shift
        repo=$1
        ;;
    -d)
	shift
	dest=$1
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

sdir=$(realpath $(dirname $0))
cd $sdir/..

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
    echo "Starting schroot, which takes some time ..."
    schroot -c $chr_name --directory=$PWD << EOD
        set -e
        [ -f $HOME/.gpg-agent-info ] && . $HOME/.gpg-agent-info
        export GPG_AGENT_INFO
        debuild $args "$karg" \
            --lintian-opts --suppress-tags dir-or-file-in-opt,package-modifies-ld.so-search-path,package-name-doesnt-match-sonames
EOD
else
    (set -x; debuild $args "$karg" \
        --lintian-opts --suppress-tags dir-or-file-in-opt,package-modifies-ld.so-search-path,package-name-doesnt-match-sonames)
fi

# debuild puts results in parent directory
cd ..
chngs=nidas_*_$arch.changes 
archdebs=nidas*$arch.deb

if [ -n "$repo" ]; then
    umask 0002

    echo "Build results:"
    ls
    echo ""

    # display changes file
    echo "Contents of $chngs"
    cat $chngs
    echo ""


    # echo "pkgs=$pkgs"
    # echo "archalls=$archalls"
    # echo "chngs=$chngs"

    # install all packages and sources for armel.
    # for other architectures, just install the packages for that arch

    # Use --keepunreferencedfiles so that the previous version .deb files 
    # are not removed. Then user's who don't do an apt-get update will
    # get the old version without an error. Nightly, or once-a-week one could do
    # a deleteunreferenced.

    # try to catch the reprepro error which happens when it tries to
    # install a package version that is already in the repository.
    # This repeated-build situation can happen in jenkins if a
    # build is triggered by a pushed commit, but git pull grabs
    # an even newer commit, and a build for the newer commit is then
    # triggered later.

    # reprepro has a --ignore option with many types of errors that
    # can be ignored, but I don't see a way to ignore this error,
    # so we'll use a fixed grep.

    tmplog=$(mktemp)
    trap "{ rm -f $tmplog; }" EXIT
    status=0

    if [ $arch == armel ]; then
        flock $repo sh -c "
            reprepro -V -b $repo -C main --keepunreferencedfiles include jessie $chngs" 2> $tmplog || status=$?
    else
        echo "Installing $archdebs"
        flock $repo sh -c "
            reprepro -V -b $repo -C main -A $arch --keepunreferencedfiles includedeb jessie $archdebs" 2> $tmplog || status=$?
    fi

    if [ $status -ne 0 ]; then
        cat $tmplog
        if grep -E -q "(can only be included again, if they are the same)|(is already registered with different checksums)" $tmplog; then
            echo "One or more package versions are already present in the repository. Continuing"
        else
            exit $status
        fi
    fi

    rm -f nidas_*_$arch.build nidas_*.dsc nidas_*.tar.xz nidas*_all.deb nidas*_$arch.deb $chngs

elif [ -n "$dest" ]; then
    echo "moving results to $dest"
    mv -f nidas_*_$arch.build nidas_*.dsc nidas_*.tar.xz nidas*_all.deb nidas*_$arch.deb $chngs $dest
else
    echo "build results are in $PWD"
fi

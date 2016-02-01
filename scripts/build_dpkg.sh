#!/bin/sh

usage() {
    echo "Usage: ${1##*/} arch"
    echo "arch is armel or amd64"
    exit 1
}

if [ $# -lt 1 ]; then
    usage $0
fi
arch=$1

dir=$(dirname $0)/..
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

case $arch in
    armel):
        export CC=arm-linux-gnueabi-gcc
        ;;
    amd64)
        ;;
    *)
        usage $0
        ;;
esac

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

debuild -sa -a$arch -k'<eol-prog@eol.ucar.edu>' \
    --lintian-opts --suppress-tags dir-or-file-in-opt,package-modifies-ld.so-search-path,package-name-doesnt-match-sonames

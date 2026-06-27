#!/bin/bash
# vim: set shiftwidth=4 softtabstop=4 expandtab:

set -e


usage() {
    cat <<EOF
Usage: ${1##*/} [-i repository ] [ -I codename ] arch
-b: just build binary, not source (faster)
-f: faster, i.e. don't do scons --config=force
-i repo: install packages with reprepro to repo
-n: don't clean source tree, passing -nc to dpkg-buildpackage, implies -b
-d: move the final packages in the given directory
arch is armel, armhf, amd64 or i386
After packages are built, they can be uploaded to the EOL repository:
  '$HOME'/eol-repo/scripts/upload_packages.sh codename=<codename> upload <pkgs>
EOF
    exit 1
}

if [ $# -lt 1 ]; then
    usage $0
fi

arch=amd64
binary=false

# hack: pass the scons --config option using GCJFLAGS, which are the java flags :-)
# Otherwise I don't know how to pass options through to debian/rules and the
# Makefile from debuild
export DEB_GCJFLAGS_MAINT_SET=--config=force

args="--prepend-path /usr/local/bin --no-tgz-check -sa"
dest=""

# Add to path for pyenv support of python 3, and /usr/local/bin in case scons
# was installed there with pip3
args="--prepend-path=/usr/local/bin:/root/.pyenv/shims:/root/.pyenv/bin"

while [ $# -gt 0 ]; do
    case $1 in
    -i)
        shift
        repo=$1
        ;;
    -b)
        binary=true
        ;;
    -d)
        shift
        dest=$1
        ;;
    -f)
        export DEB_GCJFLAGS_MAINT_SET=
        ;;
    -n)
        args="$args -nc"
        binary=true
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
    i386)
        arch=$1
        ;;
    *)
        usage $0
        ;;
    esac
    shift
done

if $binary; then
    args="$args -b"
else
    # args="$args -sa --no-tgz-check"
    args="$args -sa"
fi

args="$args -a$arch -i -I -Ibuild* -Idoc/doxygen -Iidir"

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

args="$args -us -uc"

# clean old results
rm -f ../nidas_*.tar.xz ../nidas_*.dsc
rm -f $(echo ../nidas\*_{$arch,all}.{deb,build,changes})

# export DEBUILD_DPKG_BUILDPACKAGE_OPTS="$args"

# To test cleans, do: debuild -- clean
# DEB_BUILD_MAINT_OPTIONS
(set -x; time debuild $args \
    --lintian-opts --suppress-tags dir-or-file-in-opt,package-modifies-ld.so-search-path,package-name-doesnt-match-sonames)

# debuild puts results in parent directory
cd ..
chngs=nidas_*_$arch.changes
archdebs=nidas*$arch.deb

echo "Build results:"
ls
echo ""

# display changes file
echo "Contents of $chngs"
cat $chngs
echo ""

# Grab all the package names from the changes file
pkgs=($(awk '/Checksums-Sha1/,/Checksums-Sha256/ { if (NF > 2) print $3 }' $chngs | grep ".*\.deb" | sed "s/_.*_.*\.deb//"))

# Dispatch the packages unless neither -d nor -i were specified.
if [ -n "$dest" ]; then
    echo "moving results to $dest"
    (set -x; mv -f nidas_*_$arch.build nidas_*.dsc nidas_*.tar.xz nidas*_$arch.deb $chngs $dest)
else
    echo "build results are in $PWD"
fi

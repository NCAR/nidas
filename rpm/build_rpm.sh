#!/bin/bash

script=`basename $0`
dir=`dirname $0`

dopkg=nidas
buildmodules=true
destdir=

while [ $# -gt 0 ]; do
    case $1 in
        --no-modules)
            buildmodules=false
            ;;
        --modules)
            buildmodules=true
            ;;
        -d)
            destdir="$2"
            shift
            ;;
        -*)
            echo "Usage: $0 [-nr] [--[no-]modules] [spec-file]"
            exit 1
            ;;
        *)
            dopkg=$1
            ;;
    esac
    shift
done

cd $dir || exit 1
cd ..    # to top of nidas tree

# Change topdir for a machine specific build. Use $TOPDIR if it exists.
# So that we don't compile from scratch everytime, do not --clean the BUILD
# tree with rpmbuild.  nidas.spec %setup also has a -D option that
# does not clear the BUILD tree before un-taring the source

# If hostname is not available, then likely this is a container and it's not
# useful.
host=""
if command -v hostname >/dev/null ; then
    host="_$(hostname)"
fi
topdir=${TOPDIR:-$(rpmbuild --eval %_topdir)$host}

# echo "topdir=$topdir"
[ -d $topdir/SOURCES ] || mkdir -p $topdir/SOURCES
[ -d $topdir/BUILD ] || mkdir -p $topdir/BUILD
[ -d $topdir/SRPMS ] || mkdir -p $topdir/SRPMS
[ -d $topdir/RPMS ] || mkdir -p $topdir/RPMS

log=`mktemp /tmp/${script}_XXXXXX.log`
trap "{ rm -f $log; }" EXIT

set -o pipefail

if [ $dopkg == nidas -o $dopkg == nidas-doxygen ]; then

    if $buildmodules; then
        args="$args LINUX_MODULES=yes"
        withmodules="--with modules"
    else
        args="$args LINUX_MODULES=no"
        withmodules=
    fi

    if ! gitdesc=$(git describe --match "v[0-9]*"); then
        echo "git describe failed, looking for a tag of the form v[0-9]*"
        exit 1
    fi
    # example output of git describe: v2.0-14-gabcdef123
    gitdesc=${gitdesc/#v}       # remove leading v
    version=${gitdesc%%-*}      # 2.0

    release=${gitdesc#*-}       # 14-gabcdef123
    release=${release%-*}       # 14
    [ $gitdesc == "$release" ] && release=0 # no dash

    if [ $dopkg == nidas ]; then
        # Don't build nidas source package.  We cannot release the source
        # if it contains the Condor code, and no one uses it anyway.
        buildopt=-bb
    else
        # Don't build source for nidas-doxygen.
        buildopt=-bb
    fi

    cd src   # to src
    scons BUILDS=host $args versionfiles
    cd -    # back to top

    tar czf $topdir/SOURCES/${dopkg}-${version}.tar.gz \
            rpm src/filters src/SConstruct src/nidas src/tools src/firmware \
            src/systemd src/xml doc/doxygen_conf || exit $?

    # set _unpackaged_files_terminate_build to false, which risks the situation
    # of not knowing that an important file is missing from the RPM.
    # The warnings are printed out at the end of the script, so hopefully they'll
    # be noticed.

    # set debug_package to %{nil} to suppress the build of the debug package,
    # which avoids this failure:
    #
    # extracting debug info from /tmp/maclean/rpmbuild_tikal/BUILDROOT/nidas-1.1-1.el6.x86_64/opt/nidas/bin/configedit
    # /usr/lib/rpm/debugedit: canonicalization unexpectedly shrank by one character
    #
    # Apparently this problem is often due to double slashes, "//", in path names that are
    # being extracted from binaries. I tried to find them in the build messages for
    # configedit, but no luck.

    rpmbuild $buildopt $withmodules \
        --define "gitversion $version" --define "releasenum $release" \
        --define "_topdir $topdir" \
        --define "_unpackaged_files_terminate_build 0" \
        --define "debug_package %{nil}" \
        rpm/${dopkg}.spec 2>&1 | tee -a $log  || exit $?

fi

pkg=nidas-ael
if [ $dopkg == $pkg ]; then
    rpmbuild -ba --define "_topdir $topdir" rpm/${pkg}.spec 2>&1 | tee -a $log  || exit $?
fi

echo "RPMS:"
grep -E "^Wrote:" $log
rpms=`grep -E '^Wrote:' $log | grep -E RPMS/ | awk '{print $2}'`
echo "rpms=$rpms"

# print out warnings: and the following file list
sed -n '
/^warning:/{
: next
p
n
/^ /b next
}
' $log

if [ -n "$destdir" ]; then
    echo "moving packages to $destdir..."
    (set -x; mv -f $rpms $destdir)
fi

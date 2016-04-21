#!/bin/bash

script=`basename $0`
dir=`dirname $0`

dopkg=all

while [ $# -gt 0 ]; do
    case $1 in
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
topdir=${TOPDIR:-$(rpmbuild --eval %_topdir)_$(hostname)}

# echo "topdir=$topdir"
[ -d $topdir/SOURCES ] || mkdir -p $topdir/SOURCES
[ -d $topdir/BUILD ] || mkdir -p $topdir/BUILD
[ -d $topdir/SRPMS ] || mkdir -p $topdir/SRPMS
[ -d $topdir/RPMS ] || mkdir -p $topdir/RPMS

log=`mktemp /tmp/${script}_XXXXXX.log`
tmpspec=`mktemp /tmp/${script}_XXXXXX.spec`
awkcom=`mktemp /tmp/${script}_XXXXXX.awk`
trap "{ rm -f $log $tmpspec $awkcom; }" EXIT

set -o pipefail

pkg=nidas
if [ $dopkg == all -o $dopkg == $pkg ]; then

    # In the RPM changelog, copy most recent commit subject lines
    # since this tag (max of 100).
    sincetag=v1.2

    # to get the most recent tag of the form: vN
    # sincetag=$(git tag -l --sort=version:refname "[vV][0-9]*" | tail -n 1)

    if ! gitdesc=$(git describe --match "v[0-9]*"); then
        echo "git describe failed, looking for a tag of the form v[0-9]*"
        exit 1
    fi
    # example output of git describe: v2.0-14-gabcdef123
    gitdesc=${gitdesc/#v}       # remove leading v
    version=${gitdesc%%-*}       # 2.0

    release=${gitdesc#*-}       # 14-gabcdef123
    release=${release%-*}       # 14
    [ $gitdesc == "$release" ] && release=0 # no dash

    # run git describe on each hash to create a version
    cat << \EOD > $awkcom
/^[0-9a-f]{7}/ {
    cmd = "git describe --match '[vV][0-9]*' " $0 " 2>/dev/null"
    res = (cmd | getline version)
    close(cmd)
    if (res == 0) {
        version = ""
    }
}
/^\*/ { print $0,version }
/^-/ { print $0 }
/^$/ { print $0 }
EOD

    # create change log from git log messages since the tag $sincetag.
    # Put SHA hash by itself on first line. Above awk script then
    # converts it to the output of git describe, and appends it to "*" line.
    # Truncate subject line at 60 characters 
    # git convention is that the subject line is supposed to be 50 or shorter
    git log --max-count=100 --date-order --format="%H%n* %cd %aN%n- %s%n" --date=local ${sincetag}.. | sed -r 's/[0-9]+:[0-9]+:[0-9]+ //' | sed -r 's/(^- .{,60}).*/\1/' | awk --re-interval -f $awkcom | cat rpm/${pkg}.spec - > $tmpspec

    cd src   # to src
    scons BUILDS=host build/include/nidas/Revision.h build/include/nidas/linux/Revision.h
    cd -    # back to top

    tar czf $topdir/SOURCES/${pkg}-${version}.tar.gz \
            rpm pkg_files src/SConstruct src/nidas src/build/include \
            src/xml || exit $?

    # If $JLOCAL/include/raf or /opt/local/include/raf exists then
    # build configedit package
    [ -d ${JLOCAL:-/opt/local}/include/raf ] && withce="--with configedit"

    # If moc-qt4 is in PATH, build autocal
    type -p moc-qt4 > /dev/null && withac="--with autocal"

    # edit_cal has an rpath of /usr/{lib,lib64}
    # Setting QA_RPATHS here prevents rpmbuild from dying until
    # that is fixed.
    # export QA_RPATHS=$[ 0x0001|0x0010 ]

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

    rpmbuild -ba $withce $withac \
        --define "gitversion $version" --define "releasenum $release" \
        --define "_topdir $topdir" \
        --define "_unpackaged_files_terminate_build 0" \
        --define "debug_package %{nil}" \
        $tmpspec 2>&1 | tee -a $log  || exit $?

fi

pkg=nidas-ael
if [ $dopkg == all -o $dopkg == $pkg ];then
    rpmbuild -ba --define "_topdir $topdir" rpm/${pkg}.spec 2>&1 | tee -a $log  || exit $?
fi

echo "RPMS:"
egrep "^Wrote:" $log
rpms=`egrep '^Wrote:' $log | egrep RPMS/ | awk '{print $2}'`
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

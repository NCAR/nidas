#!/bin/sh

script=`basename $0`
dir=`dirname $0`

dopkg=all
install=false
rsync_install=false
rsync_host=unknown

while [ $# -gt 0 ]; do
    case $1 in
        -i)
            install="true"
            ;;
        -r)
            rsync_install="true"
	    shift
	    rsync_host=$1
            ;;
        *)
            dopkg=$1
            ;;
    esac
    shift
done

cd $dir || exit 1
cd ..    # to top of nidas tree

rroot=unknown
rf=repo_scripts/repo_funcs.sh
[ -f $rf ] || rf=/net/www/docs/software/rpms/scripts/repo_funcs.sh
if [ -f $rf ]; then
    source $rf
    rroot=`get_eol_repo_root`
else
    [ -d /net/www/docs/software/rpms ] && rroot=/net/www/docs/software/rpms
fi

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
trap "{ rm -f $log $tmpspec; }" EXIT

set -o pipefail

pkg=nidas
if [ $dopkg == all -o $dopkg == $pkg ]; then

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

    # create change log from git log messages since v1.2
    # Truncate subject line at 60 characters 
    # git convention is that the subject line is supposed to be 50 or shorter
    git log --format="* %cd %aN%n- (%h) %s%d%n" --date=local  v1.2.. | sed -r 's/[0-9]+:[0-9]+:[0-9]+ //'  | sed -r 's/(^- \([^)]+\) .{,60}).*/\1/' | cat rpm/${pkg}.spec - > $tmpspec

    cd src   # to src
    scons BUILDS=host build/include/nidas/Revision.h build/include/nidas/linux/Revision.h
    cd -    # back to top

    tar czf $topdir/SOURCES/${pkg}-${version}.tar.gz \
            rpm src/SConstruct src/nidas src/build/include \
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
        --define "version $version" --define "releasenum $release" \
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

if $install && [ -d $rroot ]; then
    echo "Moving rpms to $rroot"
    copy_rpms_to_eol_repo $rpms
elif $rsync_install; then
    echo "Rsyncing rpms to $rsync_host"
    rsync_rpms_to_eol_repo $rsync_host $rpms
elif $install; then
    echo "$rroot not found. Leaving RPMS in $topdir"
else
    echo "-i or -r options not specified. RPMS will not be installed"
fi

# print out warnings: and the following file list
sed -n '
/^warning:/{
: next
p
n
/^ /b next
}
' $log

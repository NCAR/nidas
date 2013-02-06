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
source repo_scripts/repo_funcs.sh || exit 1

topdir=${TOPDIR:-`get_rpm_topdir`}

# Change topdir for a machine specific build. Use $TOPDIR if it exists.
# So that we don't compile from scratch everytime, do not --clean the BUILD
# tree with rpmbuild.  nidas.spec %setup also has a -D option that
# does not clear the BUILD tree before un-taring the source
topdir=${TOPDIR:-`get_rpm_topdir`_`hostname`}

# echo "topdir=$topdir"
[ -d $topdir/SOURCES ] || mkdir -p $topdir/SOURCES
[ -d $topdir/BUILD ] || mkdir -p $topdir/BUILD
[ -d $topdir/SRPMS ] || mkdir -p $topdir/SRPMS
[ -d $topdir/RPMS ] || mkdir -p $topdir/RPMS

rroot=`get_eol_repo_root`

log=`mktemp /tmp/${script}_XXXXXX.log`
trap "{ rm -f $log; }" EXIT

set -o pipefail

get_version() 
{
    awk '/^Version:/{print $2; exit 0}' $1
}

pkg=nidas
if [ $dopkg == all -o $dopkg == $pkg ];then

    version=`get_version ${pkg}.spec`

    cd ../
    scons BUILDS=x86 nidas/core/SvnInfo.h nidas/linux/SvnInfo.h
    cd -

    tarversion=`tar --version | cut -d \  -f 4`
    echo $tarversion
    # tar 1.15.1 doesn't have --transform option
    if echo $tarversion | fgrep -q 1.15; then
        tar czf $topdir/SOURCES/${pkg}-${version}.tar.gz --exclude .svn \
            nidas -C ../../.. \
            ./nidas/src/SConstruct ./nidas/src/nidas ./nidas/src/site_scons \
            ./nidas/src/xml ./nidas/src/scripts || exit $?
    else
        tar czf $topdir/SOURCES/${pkg}-${version}.tar.gz --exclude .svn \
            nidas -C ../.. --transform="s,^./,nidas/," \
            ./src/SConstruct ./src/nidas ./src/site_scons \
            ./src/xml ./src/scripts || exit $?
    fi

    # If $JLOCAL/include/raf or /opt/local/include/raf exists then
    # build configedit package
    [ -d ${JLOCAL:-/opt/local}/include/raf ] && withce="--with configedit"

    # edit_cal has an rpath of /usr/{lib,lib64}
    # Setting QA_RPATHS here prevents rpmbuild from dying until
    # that is fixed.
    export QA_RPATHS=$[ 0x0001|0x0010 ]

    # set _unpackaged_files_terminate_build to false, which risks the situation
    # of not knowing that an important file is missing from the RPM.
    # The warnings are printed out at the end of the script, so hopefully they'll
    # be noticed.
    rpmbuild -ba $withce \
        --define "_topdir $topdir" \
        --define "_unpackaged_files_terminate_build 0" \
        ${pkg}.spec 2>&1 | tee -a $log  || exit $?

fi

pkg=nidas-ael
if [ $dopkg == all -o $dopkg == $pkg ];then
    rpmbuild -ba --define "_topdir $topdir" ${pkg}.spec 2>&1 | tee -a $log  || exit $?
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

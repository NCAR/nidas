#!/bin/sh

script=`basename $0`

dopkg=all
install=false

while [ $# -gt 0 ]; do
    case $1 in
        -i)
            install="true"
            ;;
        *)
            dopkg=$1
            ;;
    esac
    shift
done

source repo_scripts/repo_funcs.sh

topdir=${TOPDIR:-`get_rpm_topdir`}
rroot=`get_eol_repo_root`

log=/tmp/$script.$$
trap "{ rm -f $log; }" EXIT

set -o pipefail

get_version() 
{
    awk '/^Version:/{print $2}' $1
}

pkg=nidas
if [ $dopkg == all -o $dopkg == $pkg ];then
    version=`get_version ${pkg}.spec`
    tar czf $topdir/SOURCES/${pkg}-${version}.tar.gz --exclude .svn nidas
    rpmbuild -ba ${pkg}.spec | tee -a $log  || exit $?
fi

pkg=nidas-ael
if [ $dopkg == all -o $dopkg == $pkg ];then
    rpmbuild -ba ${pkg}.spec | tee -a $log  || exit $?
fi

pkg=nidas-bin
if [ $dopkg == all -o $dopkg == $pkg ];then

    # Change topdir for a machine specific build. Use $TOPDIR if it exists.
    # So that we don't compile from scratch everytime, do not --clean the BUILD
    # tree with rpmbuild.  nidas-bin.spec %setup also has a -D option that
    # does not clear the BUILD tree before un-taring the source
    topdirx=${TOPDIR:-`get_rpm_topdir`_`hostname`}

    # echo "topdir=$topdirx"
    [ -d $topdirx/SOURCES ] || mkdir -p $topdirx/SOURCES
    [ -d $topdirx/BUILD ] || mkdir -p $topdirx/BUILD
    [ -d $topdirx/SRPMS ] || mkdir -p $topdirx/SRPMS
    [ -d $topdirx/RPMS ] || mkdir -p $topdirx/RPMS

    version=`get_version ${pkg}.spec`

    cd ../
    scons BUILDS=x86 nidas/core/SvnInfo.h nidas/linux/SvnInfo.h
    cd -

    tarversion=`tar --version | cut -d \  -f 4`
    # echo $tarversion
    # tar 1.15.1 doesn't have --transform option
    if echo $tarversion | fgrep -q 1.15; then
        tar czf $topdirx/SOURCES/${pkg}-${version}.tar.gz --exclude .svn -C ../../.. \
            ./nidas/src/SConstruct ./nidas/src/nidas ./nidas/src/site_scons \
            ./nidas/src/xml ./nidas/src/scripts
    else
        tar czf $topdirx/SOURCES/${pkg}-${version}.tar.gz --exclude .svn -C ../.. \
            --transform="s,^./,nidas/," \
            ./src/SConstruct ./src/nidas ./src/site_scons \
            ./src/xml ./src/scripts
    fi

    # If $JLOCAL/include/raf or /opt/local/include/raf exists then also build configedit package
    [ -d ${JLOCAL:-/opt/local}/include/raf ] && withce="--with configedit"

    rpmbuild -ba --define "_topdir $topdirx" $withce ${pkg}.spec | tee -a $log  || exit $?
fi

echo "RPMS:"
egrep "^Wrote:" $log
rpms=`egrep '^Wrote:' $log | egrep RPMS/ | awk '{print $2}'`
echo "rpms=$rpms"

if $install && [ -d $rroot ]; then
    echo "Moving rpms to $rroot"
    copy_rpms_to_eol_repo $rpms
elif $install; then
    echo "$rroot not found. Leaving RPMS in $topdir"
else
    echo "-i option not specified. RPMS will not be installed in $rroot"
fi


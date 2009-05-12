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

# source repo_scripts/repo_funcs.sh
# source repo_scripts/repo_funcs.sh
source /home/maclean/svn/eol/imports/ael/repo_scripts/repo_funcs.sh

topdir=`get_rpm_topdir`
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
    tar czf $topdir/SOURCES/${pkg}-${version}.tar.gz --exclude .svn ${pkg}-${version}
    rpmbuild -v -ba ${pkg}.spec | tee -a $log  || exit $?
fi

pkg=isff-named
if false && [ $dopkg == all -o $dopkg == $pkg ];then
    named-checkzone isff.ucar.edu ${pkg}/var/named/isff.ucar.edu || exit 1
    named-checkzone 12.168.192.in-addr.arpa ${pkg}/var/named/192.168.12 || exit 1
    named-checkzone 13.168.192.in-addr.arpa ${pkg}/var/named/192.168.13 || exit 1
    named-checkzone 14.168.192.in-addr.arpa ${pkg}/var/named/192.168.14 || exit 1
    version=`get_version ${pkg}.spec`
    tar czf ${topdir}/SOURCES/${pkg}-${version}.tar.gz --exclude .svn --exclude "*.swp" ${pkg}
    rpmbuild -ba --clean ${pkg}.spec | tee -a $log  || exit $?
fi

echo "RPMS:"
egrep "^Wrote:" $log

if $install && [ -d $rroot ]; then
    rpms="$topdir/RPMS/noarch/nidas-*.noarch.rpm"
    echo "rpms=$rpms"
    for r in $rpms; do
        echo $r
    done
    copy_rpms_to_eol_repo $rpms
fi


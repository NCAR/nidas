#!/bin/sh

# distroverpkg=fedora-release
# cf=/etc/yum.conf
# if grep -q "^distroverpkg" /etc/yum.conf; then
#     distroverpkg=`grep "^distroverpkg" $df | cut -d= -f 2`
# fi
# releasever=`rpm -q --queryformat "%{VERSION}\n" $distroverpkg`
# basearch=`uname -i`

script=`basename $0`

tmpdir=/tmp/$script_$$
mkdir $tmpdir
trap "{ rm -rf $tmpdir; }" EXIT

cd $tmpdir || exit 1

# set -x

# yumdownloader --urls nidas-bin nidas-bin-devel

yumdownloader nidas-bin nidas-bin-devel

shopt -s nullglob
rpms=(nidas-bin-*.rpm)
if [ ${#rpms[*]} -eq 0 ]; then
    sudo yum clean metadata
    yumdownloader --enablerepo=eol nidas-bin nidas-bin-devel
    rpms=(nidas-bin-*.rpm)
fi

if [ ${#rpms[*]} -eq 0 ]; then
    echo "No rpms successfully downloaded"
    exit 1
fi

sudo rpm -Uhv --force ${rpms[*]}

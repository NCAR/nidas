#!/bin/sh

# Check if nidas, nidas-libs or nidas-devel RPMs are installed.
# If so, it then run yumdownloader to download the current
# version of those RPMs from the EOL repository, and then do an
# "rpm -Uhv --force" to force update those RPMs.

# The version and release number of these rpms are not updated on
# every subversion checkin. Currently they are 1.1-0. Therefore
# a "yum update" typically won't result in an update to the latest RPM 
# on the repository. Instead, do a yumdownload and force update.

script=`basename $0`

tmpdir=`mktemp -d`
trap "{ rm -rf $tmpdir; }" EXIT

cd $tmpdir || exit 1

cleaned=false

# yumdownloader --urls nidas-bin nidas-bin-devel

for rpm in nidas nidas-libs nidas-devel; do
    if rpm -q $rpm; then
        if ! yumdownloader $rpm; then
            if ! $cleaned; then
                echo "doing yum clean all"
                yum clean all
                cleaned=true
            fi
            yumdownloader --enablerepo=eol $rpm
        fi
    fi
done

shopt -s nullglob
rpms=(*.rpm)

if [ ${#rpms[*]} -eq 0 ]; then
    echo "No nidas rpms found or successfully downloaded"
    exit 1
fi

# echo ${rpms[*]}

echo "Doing sudo yum -Uhv --force ${rpms[*]}"
sudo rpm -Uhv --force ${rpms[*]}

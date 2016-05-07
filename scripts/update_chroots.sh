#!/bin/bash

# script to update packages in chroots

dist=jessie

buildarch=amd64
[ $(arch) != x86_64 ] && buildarch=unknown

jobfile=$(mktemp)
trap "{ rm -f $jobfile; }" EXIT

for hostarch in armel armhf; do

    if [ $hostarch == $buildarch ]; then
        chr_suffix=-sbuild
    else
        chr_suffix=-cross-$hostarch-sbuild
    fi
    chr_name=${dist}-${buildarch}$chr_suffix

    cat << EOD > $jobfile
        apt-get update
        apt-get -y upgrade eol-scons \
            nidas-libs:$hostarch nidas-dev:$hostarch nidas:$hostarch \
            nc-server-lib:$hostarch nc-server-dev:$hostarch \
            xmlrpc++:$hostarch xmlrpc++-dev:$hostarch
EOD
    if [ $hostarch == armel ]; then
        cat << EOD >> $jobfile
        apt-get -y upgrade linux-headers-3.16.0-titan2:${hostarch} linux-headers-3.16.0-viper2:${hostarch}
EOD
    fi
    sudo sbuild-shell $chr_name < $jobfile
done

#!/bin/bash

hostarch=$1

# Install xmlrpc++-dev for build (amd64) architecture, so that scons
# finds the xmlrpc++ include files. 
# Must build xmlrpc++ because C++-11 has changed the abi!!!
# 
# Goal is to not have to create another special package build for NIDAS which must 
# then be hosted in an EOL package repo somewhere. So we force link libxmlrpcpp.a 
# static library into the NIDAS core shared library for all to use. However, the 
# build script # for this library does not include the static library. So we hack 
# the build script to copy it out of the temporary build folders before they are 
# nuked in a post build cleanup.
# 
git clone https://github.com/NCAR/xmlrpcpp.git
cd xmlrpcpp

if command -v apt-get ; then
    ./build_dpkg_std.sh --no-sign ./amd64 amd64
    ./build_dpkg_std.sh --no-sign ./${hostarch} ${hostarch}
    echo "Checking out build artifacts in ./amd64"
    ls -alg ./amd64
    echo "Checking out build artifacts in ./${hostarch}"
    ls -alg ./${hostarch}
    # It looks like installing with dpkg -i is not enough to satisfy the build
    # dependency check run by debuild, but it does work to install with apt.
    apt-get install ./amd64/xmlrpc++_0.7-3_amd64.deb ./amd64/xmlrpc++-dev_0.7-3_amd64.deb
    apt-get install ./${hostarch}/xmlrpc++_0.7-3_${hostarch}.deb ./${hostarch}/xmlrpc++-dev_0.7-3_${hostarch}.deb
else
    # we only build native x86_64 packages for rpm systems
    dnfi="dnf -y install"
    if ! command -v dnf; then
        dnfi="yum -y localinstall"
    fi
    export TOPDIR=$HOME/rpmbuild
    ./build_rpm.sh
    $dnfi "$TOPDIR/RPMS/x86_64/"xmlrpc++-*.rpm
fi

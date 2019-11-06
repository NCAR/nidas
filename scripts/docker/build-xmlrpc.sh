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
# We also hack the build script to not sign the packages built, as we don't have 
# have access to the private key.
sed -i 's/\-k/#\-k/' build_dpkg_std.sh
sed -i 's/cd \.\./pwd \&\& ls -alg \&\& cp libXmlRpcpp.a ${dest}\/libxmlrpcpp.a \&\& cd \.\./' build_dpkg_std.sh
cat build_dpkg_std.sh
./build_dpkg_std.sh ./amd64 amd64
./build_dpkg_std.sh ./${hostarch} ${hostarch}
echo "Checking out build artifacts in ./amd64"
ls -alg ./amd64
echo "Checking out build artifacts in ./${hostarch}"
ls -alg ./${hostarch}
sudo dpkg -i ./amd64/xmlrpc++_0.7-3_amd64.deb ./amd64/xmlrpc++-dev_0.7-3_amd64.deb
# We don't want to actually install the shared libs for ${hostarch} 
# as we're linking the static library instead
#sudo dpkg -i ./${hostarch}/xmlrpc++_0.7-3_${hostarch}.deb ./${hostarch}/xmlrpc++-dev_0.7-3_${hostarch}.deb

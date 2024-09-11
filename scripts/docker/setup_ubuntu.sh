#! /bin/bash

export DEBIAN_FRONTEND=noninteractive
export TZ=Etc/UTC

apt-get update

apt-get install -y --no-install-recommends \
    apt-utils sudo vim curl \
    build-essential fakeroot libncurses-dev bc dh-make ca-certificates \
    git ssh doxygen graphviz xsltproc docbook-xsl \
    flex libfl-dev gawk devscripts pkg-config \
    libbz2-dev libgsl-dev libgslcblas0 libcap-dev \
    libxerces-c-dev libbluetooth-dev libnetcdf-dev \
    rsync quilt lsb-release reprepro libjsoncpp-dev libxmlrpcpp-dev

# net-tools is for netstat, used in tests, also valgrind
apt-get install -y --no-install-recommends \
    debmake libmodbus-dev libusb-1.0-0-dev net-tools libboost-regex-dev \
    libboost-test-dev valgrind

# installation of tzdata wants to ask user for the time zone.  We have to be noninterative
apt-get install -y --no-install-recommends tzdata

# Add EOL repository for some local packages
#RUN echo "deb https://archive.eol.ucar.edu/software/debian/ jessie main" > /etc/apt/sources.list.d/eol.list 
#RUN curl https://archive.eol.ucar.edu/software/debian/conf/eol-prog.gpg.key | apt-key add -

# On more recent Ubuntu releases, there is a fork of the xmlrpc++ library
# available as a Debian package, thanks to https://wiki.ros.org/xmlrpcpp.
# Since that is what is used for the Vortex, use it on AMD64 also and skip the
# local build:

# Install the xmlrpc++ dependency by building and installing inside the
# container, and if /packages is mounted then move the packages there too.

# RUN git clone https://github.com:/NCAR/xmlrpcpp
# RUN cd xmlrpcpp && ./build_dpkg_std.sh results amd64 && cd results && \
#   dpkg -i xmlrpc++-dev_0.7-3_amd64.deb xmlrpc++_0.7-3_amd64.deb && \
#   (test ! -d /packages || mv -f xmlrpc++-dev_0.7-3_amd64.deb xmlrpc++_0.7-3_amd64.deb /packages)

# Need newer scons than 3.0.1, but it isn't on the bionic repos,
# so install it with pip3. Point /usr/bin/scons to the new version.
# Don't uninstall the old scons because eol-scons requires it
# and will bring it back in on an update.
apt-get install -y --no-install-recommends python3-pip && \
    pip3 install scons && \
    update-alternatives --install /usr/bin/scons scons /usr/local/bin/scons 2

# apt-get upgrade -y eol-scons

# installing a generic linux-headers package allows module builds to be tested
# in the container
apt-get update && \
    apt-get install -y --no-install-recommends linux-headers-generic

# install a fallback for eol_scons.  if it is mounted into the container under
# the home directory, then that will take precedence.
esdir=/usr/share/scons/site_scons
mkdir -p $esdir
(cd $esdir && git clone https://github.com/NCAR/eol_scons)

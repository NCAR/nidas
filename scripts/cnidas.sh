#!/bin/bash

# Script functions related to building NIDAS with containers for various
# architectures and OS releases.

# These are the standard targets (OS/arch) for NIDAS builds:
#
# fedora 31 x86_64 (native)
# centos 7  x86_64 (native)
# centos 8  x86_64 (native)
# buster    amd64 (native)  (I think we build these now, but EOL does not install them...?)
# jessie armbe Vulcan (cross)
# jessie armhf Raspberry Pi 2B (cross)
# jessie armel Viper Titan (cross)
# buster armhf Raspberry Pi 3B (cross)
#
# Here is the naming convention for NIDAS build container images:
#
# nidas-build-<dist>-<arch>:<release>[_v?]
#
# Where <dist> is {centos,fedora,debian} and <arch> is
# {x86_64,amd64,armel,armbe,armhf}, and not all combinations are relevant.
# So here are the build container images we need for the target list above.
# The first column is an alias for that target, associated with the
# particular platform that target would run on.
#
# fedora31 nidas-build-fedora-x86_64:fedora31
# centos7  nidas-build-centos-x86_64:centos7   Dockerfile.centos7
# centos8  nidas-build-centos-x86_64:centos8   Dockerfile.centos8
# buster   nidas-build-debian-amd64:buster
# vulcan   nidas-build-debian-armbe:jessie     Dockerfile.cross_ael_armbe
# titan    nidas-build-debian-armel:jessie     Dockerfile.cross_arm hostarch=armel
# pi2      nidas-build-debian-armhf:jessie     Dockerfile.cross_arm hostarch=armhf
# pi3      nidas-build-debian-armhf:buster     Dockerfile.buster_cross_arm hostarch=armhf (on the buster branch)
#
# Titan is equivalent to Viper.
#
# One confusion in this naming scheme is that the armbe Vulcan DSMs run
# Debian Jessie, but the NIDAS binaries are cross-compiled on a fedora25
# container.
#
# The original container approach created a separate user called 'builder',
# and then tried to map the user running the container to that user, with
# hardcoded group permissions for eol.  This really complicated things
# unnecessarily.  By default, the user running the container maps to the
# root user in the container, and the container has no more privileges in
# the host OS than that user.  So all that's necessary is that the mounted
# source and install directories are writable by the user running the
# container.  Technically, by running as root the build could seriously and
# unexpectedly break the container, and builds definitely should not
# usually run that way.  Also, there might be permissions bugs in the build
# which get missed by container builds.  However, containers are meant to
# be transient, disposable wrappers, so none of that is worth the
# complication hardcoding non-root users in the containers.


targets=(centos7 centos8 vulcan titan pi2)

get_image_tag() # alias
{
    case "$1" in
	centos8)
	    echo nidas-build-centos-x86_64:centos8
	    ;;
	centos7)
	    echo nidas-build-centos-x86_64:centos7
	    ;;	    
	pi2)
	    echo nidas-build-debian-armhf:jessie
	    ;;
	titan)
	    echo nidas-build-debian-armel:jessie
	    ;;
	vulcan)
	    echo nidas-build-debian-armbe:jessie
	    ;;
    esac
}


build_image() # alias
{
    tag=`get_image_tag "$1"`
    set -x
    case "$1" in
	centos8)
	    podman build -t $tag -f docker/Dockerfile.centos8
	    ;;
	centos7)
	    podman build -t $tag -f docker/Dockerfile.centos7
	    ;;	    
	pi2)
	    podman build -t $tag -f docker/Dockerfile.cross_arm --build-arg hostarch=armhf
	    ;;
	titan)
	    podman build -t $tag -f docker/Dockerfile.cross_arm --build-arg hostarch=armel
	    ;;
	vulcan)
	    podman build -t $tag -f docker/Dockerfile.cross_ael_armbe
	    ;;
    esac
}
	    

# Run the image to mount the source tree containing this script as /nidas,
# the user .scons directory, and the given install path as /opt/nidas.  If
# no install path, then default to nidas/install/<alias>.
run_image() # alias command...
{
    alias="$1"
    shift
    source=$(cd `dirname $0`/.. && pwd)
    echo "Top of nidas source tree: $source"
    tag=`get_image_tag "$alias"`
    install=""
    if [ -z "$install" ]; then
	install="$source/install/$alias"
	mkdir -p "$install"
    fi
    if [ -z "$1" ]; then
	set /bin/bash
    fi
    set -x
    exec podman run -i -t \
      --volume "$source":/nidas:rw,Z \
      --volume "$install":/opt/nidas:rw,Z \
      --volume "$HOME/.scons":/root/.scons:rw,Z \
      $tag "$@"
}

build_packages()
{
    alias="$1"
    shift
    run_image "$alias" /nidas/scripts/build_dpkg.sh armhf -d /nidas/packages
}

# To build armhf jessie packages:
#
# scripts/build_dpkg.sh -c -s -i $DEBIAN_REPOSITORY armhf

# Seems like debuild has to clean up the source tree first to tar
# everything up, where it makes more sense to me to clone the git repo into
# a clean checkout and then tar that.  Perhaps use the current repo to
# update the debian changelog, then use a shallow clone.


case "$1" in

    build)
	build_image "$2"
	;;

    run)
	shift
	run_image "$@"
	;;

    package)
	shift
	build_packages "$@"
	;;

    list)
	shift
	# list all the target aliases and their container image tags
	for target in ${targets[*]}; do
	    echo "$target" `get_image_tag "$target"`
	done
	;;

esac

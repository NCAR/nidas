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


targets=(centos7 centos8 vulcan titan pi2 pi3)

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
	pi3)
	    echo nidas-build-debian-armhf:buster
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
	pi3)
	    podman build -t $tag -f docker/Dockerfile.buster_cross_arm --build-arg hostarch=armhf
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
    echo "Top of nidas source tree: $source"
    # If packagepath not set, then default to a subdirectory of the source.
    if [ -z "$packagepath" ]; then
	packagepath="$source/packages/$alias"
	mkdir -p "$packagepath"
    fi
    # If a tag has been requested, clone it and use that for the source.
    dest="$source"
    if [ -n "$tag" ]; then
	dest="$source/clones/nidas-$tag"
	clone_local "$tag" "$source" "$dest"
    fi
    imagetag=`get_image_tag "$alias"`
    install=""
    if [ -z "$install" ]; then
	install="$dest/install/$alias"
	mkdir -p "$install"
    fi
    if [ -z "$1" ]; then
	set /bin/bash
    fi
    set -x
    # Mount the local scripts directory over top of the source, so the
    # local build scripts are used no matter what version of source is
    # being built.
    exec podman run -i -t \
      --volume "$dest":/nidas:rw,Z \
      --volume "$install":/opt/nidas:rw,Z \
      --volume "$HOME/.scons":/root/.scons:rw,Z \
      --volume "$packagepath":/nidas/packages:rw,Z \
      --volume "$scripts":/nidas/scripts \
      $imagetag "$@"
}

build_packages() # alias dest
{
    alias="$1"
    packagepath="$2"
    shift; shift
    # Make sure packages directory exists, of course.
    # mkdir -p "$source/packages/$alias"
    run_image "$alias" /nidas/scripts/build_dpkg.sh armhf -d /nidas/packages
}


# Push packages to the cloud for the given alias located in the given
# path. Explicitly avoid uploading dbgsym packages.  package.

push_packages() # alias path
{
    # list the specific package files that need to be uploaded, ie, not
    # debug symbols
    alias="$1"
    path="$2"
    if [ -z "$alias" -o -z "$path" ]; then
	echo "push <alias> <path>"
	exit 1
    fi
    case "$alias" in
	pi3)
	    repo="ncareol/isfs-testing/raspbian/buster"
	    (set -x
	     package_cloud push $repo `ls "$path"/*.deb | egrep -v dbgsym`)
	    ;;
	*)
	    echo Only pi3 packages are pushed to package cloud.
	    ;;
    esac
}

# We don't need shallow clone since git automatically optimizes local
# clones with hard links to repo object files, and that way the cloned repo
# still has history for generating the changelog file.
#
# The origin of the cloned repo will be the local path, so submodules with
# relative URLs will then fail to update.  So we have to override the URL
# of each submodule being cloned from the origin repo.
#
# Rather than use git clone -b option to choose a particular remote branch
# to be checked out in the clone, checkout the given tag explicitly.  That
# allows tags to be used, whereas -b only allows branches.

clone_local() # tag source dest
{
    tag="$1"
    source="$2"
    dest="$3"
    if [ -z "$tag" -o -z "$source" -o -z "$dest" ]; then
	echo 'clone_local {tag} {source} {dest}'
	exit 1
    fi
    (cd "$source"
     absource=$(pwd)
     set -x
     git clone . "$dest" || exit 1
     if [ ! -d "$dest" ]; then
	 echo "Destination directory not found: $dest"
	 exit 1
     fi
     (cd "$dest"; git checkout "$tag")
     git submodule --quiet foreach 'echo $sm_path' | while read path ; do
	 (cd "$dest"
	  git submodule init "$path"
	  git config --local submodule."$path".url "$absource"/.git/modules/"$path"
	  git submodule update "$path")
     done)
}



# To build armhf jessie packages:
#
# scripts/build_dpkg.sh -c -s -i $DEBIAN_REPOSITORY armhf

# Seems like debuild has to clean up the source tree first to tar
# everything up, where it makes more sense to me to clone the git repo into
# a clean checkout and then tar that.  Perhaps use the current repo to
# update the debian changelog, then use a shallow clone.

# scripts is the directory from which we're running and in which the build
# scripts and Dockerfiles are found, while source is the source tree that
# will actually be built.
scripts=$(cd `dirname $0`/.. && pwd)
scripts="$scripts/scripts"

source=$(cd `dirname $0`/.. && pwd)
tag=""

if [ "$1" == "--source" ]; then
    source="$2"
    shift; shift
fi
if [ "$1" == "--tag" ]; then
    tag="$2"
    shift; shift
fi
echo "Source tree path: $source"
if [ -n "$tag" ]; then
    echo "Using clone of $tag"
fi

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

    clone)
	shift
	clone_local "$@"
	;;

    push)
	shift
	push_packages "$@"
	;;

    *)
	cat <<EOF
Usage: $0 [--source <path>] [--tag <tag>] {build|run|package,list}
  list
    List the available aliases and container images for OS and architecture targets.
  build <alias>
    Build the container image for the given alias and tag it.
  run <alias> [command args ...]
    Run the image for the given alias, mounting the source, install,
    and ~/.scons paths into the container.
  package <alias> <dest>
    Run the script which builds packages for the alias, and copy the
    packages into <dest>.
  push <alias> <path>
    Push packages to the cloud for alias from the given path.
If --tag is given, it can be HEAD, tag, branch, or any git commit.
That commit of the target source is first shallow cloned before
continuing with the build.
EOF
	exit 0
	;;

esac

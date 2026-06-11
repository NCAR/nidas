#!/bin/bash

# Script functions related to building NIDAS with containers for various
# architectures and OS releases.

# See read_targets function for the list of supported targets and their
# associated containers.
#
# Here is the naming convention for NIDAS build container images:
#
# nidas-build-<dist>-<arch>:<release>[_v?]
#
# Where <dist> is {centos,fedora,debian} and <arch> is
# {x86_64,amd64,armel,armbe,armhf}, and not all combinations are relevant.
#
# I think Titan is equivalent to Viper, except for the
# nidas-modules-titan and nidas-modules-viper packages, since the
# Titans and Vipers have different kernels and thus different kernel
# modules.
#
# One confusion in this naming scheme is that the armbe Vulcan DSMs run
# Debian Jessie, but the NIDAS binaries are cross-compiled on a fedora25
# container.
#
# With rootless podman containers, the user running the container maps to the
# root user in the container, and the container has no more privileges in the
# host OS than that user. So all that's necessary is that the mounted source
# and install directories are writable by the user running the container.
# Technically, by running as root the build could seriously and unexpectedly
# break the container, and builds definitely should not usually run that way.
# Also, there might be permissions bugs in the build which get missed by
# container builds.  However, containers are meant to be transient, disposable
# wrappers, so none of that seems to justify hardcoding non-root users in the
# containers.

usage()
{
    cat <<EOF
Usage: $0 [options] <alias> {list|build|clone|run|scons|package|push}

Options:
  --source <path>
  --tag <tag>
  --work <path>

These operations use the current source directory, ignoring --source
and --tag:

  list
    List the available aliases and container images for OS and
    architecture targets.  This does not list the existing containers,
    only what this script can build and run.

  build
    Build a local container image for the given alias and tag it.

These operations can use the --source, --tag, and --work options.
Using either the current repository or a path to a different source
repository, and optionally using a clone of the source checked out
with a given tag, execute the 'run' or 'package' operation against
that source:

  clone
    Create a clone from the source and tag, as would be done
    with the --tag and --source options.  The clone will be
    in <work>/clones/nidas-<tag>.

  run [command args ...]
    Run the image for the given alias, mounting the source, install,
    and ~/.scons paths into the container.  The install path is
    <work>/install/<alias>, mounted as /opt/nidas in the container.
    The packages directory <work>/install/<alias> is mounted as
    /packages in the container.

  scons [scons args ...]
    Run scons inside the container, passing BUILDS and PREFIX
    on the command line, with any additional arguments.

  package
    Run the script which builds packages for the alias, and copy the
    packages into <work>/packages/<alias>.

If --tag is given, it can be HEAD, tag, branch, or any git commit.
That commit of the target source is first shallow cloned before
continuing with the build.

The work path will be used to hold clones and packages, like so:

Packages: <work>/packages/<alias>
Clones: <work>/clones/nidas-<tag>

The packages path is mounted in the container at path /nidas/packages.
Packages will be moved there after being built.
The work path by default is /tmp/cnidas.

If no tag is given, then no clone is performed and the compile (and
possibly package build) happens inside the source tree.  If no source
is named, then the current repo is used.

EOF
}


# alias arch image_tag containerfile [build args]
read_targets()
{
    header=`cat <<EOF
_alias_  _arch_ _tag_                             _containerfile_               _buildargs_
EOF`
    cat <<EOF
centos7  x86_64 nidas-build-centos-x86_64:centos7 Dockerfile.centos7
centos8  x86_64 nidas-build-centos-x86_64:centos8 Dockerfile.centos8
pi2      armhf  nidas-build-debian-armhf:jessie   Dockerfile.cross_arm          hostarch=armhf
pi3      armhf  nidas-build-debian-armhf:buster   Dockerfile.buster_cross_arm   hostarch=armhf
arm64    arm64  nidas-build-debian-arm64:bookworm Dockerfile.debian_cross_arm64 HOST_ARCH=arm64 CODENAME=bookworm
pi5      arm64  nidas-build-debian-arm64:trixie   Dockerfile.debian_cross_arm64 HOST_ARCH=arm64 CODENAME=trixie
titan    armel  nidas-build-debian-armel:jessie   Dockerfile.cross_arm          hostarch=armel
vulcan   armbe  nidas-build-debian-armbe:jessie   Dockerfile.cross_ael_armbe
ubuntu   amd64  nidas-build-ubuntu-amd64:latest   Dockerfile.ubuntu_amd64
EOF
}


# Return the arch for passing to build_dpkg
get_arch() # alias
{
    while read -r name arch image_tag containerfile buildargs; do
        if [ "$name" == "$1" ]; then
            echo $arch
            return
        fi
    done < <(read_targets)
}

# Return the BUILDS setting for the given alias, same as the arch.
get_builds() # alias
{
    while read -r name arch image_tag containerfile buildargs; do
        if [ "$name" == "$1" ]; then
            echo $arch
            return
        fi
    done < <(read_targets)
}

get_image_tag() # alias
{
    while read -r name arch image_tag containerfile buildargs; do
        if [ "$name" == "$1" ]; then
            echo $image_tag
            return
        fi
    done < <(read_targets)
}


build_image()
{
    while read -r name arch image_tag containerfile buildargs; do
        if [ "$name" == "$alias" ]; then
            if [ -n "$buildargs" ]; then
                for arg in $buildargs; do
                    set -- --build-arg $arg "$@"
                done
            fi
            set -x
            podman build -t $image_tag -f docker/$containerfile "$@"
            break
        fi
    done < <(read_targets)
}


# Run the image to mount the source tree containing this script as /nidas,
# the user .scons directory, and the given install path as /opt/nidas.  If
# no install path, then default to nidas/install/<alias>.
run_image() # command...
{
    echo "Top of nidas source tree: $source"
    # Create packagepath under workpath.
    packagepath="$workpath/packages/$alias"
    mkdir -p "$packagepath"
    # Make sure packagepath is absolute to mount it into container.
    packagepath=$(realpath "$packagepath")
    # If a tag has been requested, clone it and use that for the source.
    dest="$source"
    if [ -n "$tag" ]; then
        dest="$workpath/clones/nidas-$tag"
        clone_local "$tag" "$source" "$dest"
    fi
    imagetag=`get_image_tag "$alias"`
    # install path is also under workpath
    install="$workpath/install/$alias"
    mkdir -p "$install"
    if [ -z "$1" ]; then
        set /bin/bash
    fi
    # If the repository is available on this host, then mount that
    # too.
    DEBIAN_REPOSITORY=/net/ftp/pub/archive/software/debian
    repomount=""
    if [ -d $DEBIAN_REPOSITORY ]; then
        repomount="--volume ${DEBIAN_REPOSITORY}:/debian:rw,Z"
    fi
    pcmount=""
    if [ -f "$HOME/.packagecloud" ]; then
        pcmount="--volume $HOME/.packagecloud:/root/.packagecloud:ro,Z"
    fi
    set -x
    # Mount the local scripts directory over top of the source, so the local
    # build scripts are used no matter what version of source is being built.
    # Remove the container when finished, since it does not need to be kept
    # around and just in any credentials end up on it.
    exec podman run --rm -i -t \
      --volume "$dest":/nidas:rw,Z \
      --volume "$install":/opt/nidas:rw,Z \
      --volume "$HOME/.scons":/root/.scons:rw,Z \
      --volume "$packagepath":/packages:rw,Z \
      --volume "$scripts":/nidas/scripts:rw,Z \
      $pcmount $repomount $imagetag "$@"
}

run_scons() # [scons args ...]
{
    run_image scons -C /nidas/src PREFIX=/opt/nidas BUILDS=`get_builds $alias` "$@"
}

build_packages()
{
    if [ "$1" == "-h" -o -z "$alias" -o $# -ne 0 ]; then
        echo "build_packages"
        echo "Packages will be copied to $workpath/packages/$alias."
        exit 1
    fi
    run_image /nidas/scripts/build_dpkg.sh `get_arch $alias` -d /packages
}


# Test installing nidas packages into the container.  The packages should
# already exist in the /packages directory.  This is just a convenient
# function to run in the container to install the packages that should
# install.  On cross-platform build containers, nidas-daq cannot be installed
# because it depends on just nidas and not nidas:${arch}.
#
# ./cnidas.sh arm64 run /nidas/scripts/cnidas.sh arm64 install_packages
#
# This will break if more than one version of packages exists...
install_packages()
{
    arch=`get_arch $alias`
    # make sure setcap is installed so postinst scripts can call it
    apt -y install libcap2-bin
    cd /packages
    dpkg -i nidas_*_${arch}.deb nidas-min_*_${arch}.deb nidas-libs_*_${arch}.deb nidas-dev_*_${arch}.deb
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
     if [ -d "$dest" ]; then
        echo "Clone already exists: $dest"
        echo "Pulling instead of cloning..."
        (cd "$dest" && git pull) || exit 1
     else
         git clone . "$dest" || exit 1
     fi
     if [ ! -d "$dest" ]; then
        echo "Destination directory not found: $dest"
        exit 1
     fi
     (cd "$dest"; git checkout "$tag")
     git submodule --quiet foreach 'echo $sm_path' | while read path ; do
        (cd "$dest"
        git submodule init "$path"
        git config --local submodule."$path".url "$absource"/.git/modules/"$path"
        git -c protocol.file.allow=always submodule update "$path")
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
scripts=$(realpath `dirname $0`/..)
scripts="$scripts/scripts"

# Global variables.  Since alias is the name for the target to build, avoid
# using a variable named alias when parsing the target list.
source=$(realpath `dirname $0`/..)
tag=""
workpath=""
alias=""

while [ $# -gt 0 ]; do
    case "$1" in

        --source)
            source="$2"
            source=$(realpath "$source")
            shift; shift
            ;;

        --tag)
            tag="$2"
            shift; shift
            ;;

        --work)
            workpath="$2"
            shift; shift
            ;;

        list)
            break
            ;;

        build|run|scons|package|clone|push|install_packages)
            if [ -z "$alias" ]; then
            echo "Alias is required for $1."
            exit 1
            fi
            break
            ;;

        -*)
            echo "Unrecognized option: $1"
            exit 1
            ;;

        *)
            itag=`get_image_tag "$1"`
            if [ -z "$itag" ]; then
            echo "Unrecognized alias: $1"
            exit 1
            fi
            alias="$1"
            shift
            ;;
    esac
done

if [ -z "$workpath" ]; then
    workpath="/tmp/cnidas"
    mkdir -p "$workpath"
fi
if [ ! -d "$workpath" ]; then
    # Technically we don't need workpath for build and list
    # operations, but whatever...
    echo "$workpath does not exist."
    exit 1
fi
echo "Source tree path: $source"
if [ -n "$tag" ]; then
    echo "Using clone of $tag"
fi

case "$1" in

    build)
        shift
        build_image "$@"
        ;;

    run)
        shift
        run_image "$@"
        ;;

    scons)
        shift
        run_scons "$@"
        ;;

    package)
        shift
        build_packages "$@"
        ;;

    list)
        shift
        echo "$header"
        read_targets
        ;;

    clone)
        shift
        clone_local "$tag" "$source" "$workpath/clones/nidas-$tag"
        ;;

    install_packages)
        install_packages "$@"
        ;;

    *)
        usage
        exit 0
        ;;

esac

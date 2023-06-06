#!/bin/bash

# Script functions related to building NIDAS with containers for various
# architectures and OS releases.

# These are the standard targets (OS/arch) for NIDAS builds:
#
# fedora 31 x86_64 (native)
# centos 7  x86_64 (native)
# centos 8  x86_64 (native)
# buster    amd64 (native) (This would be a new one, but something we may as well support.)
# jessie    amd64 (native) (I think we build this on the debian VM, but no one ever used it?)
# jessie armbe Vulcan (cross-compiled on Fedora25)
# jessie armhf Raspberry Pi 2B (cross)
# jessie armel Viper Titan (cross)
# buster armhf Raspberry Pi 3B (cross)
# ubuntu amd64
# alma linux 9 (native)
# bionic i386 (debian i386 native) vortex86dx3
#
# Here is the naming convention for NIDAS build container images:
#
# nidas-build-<dist>-<arch>:<release>[_v?]
#
# Where <dist> is {centos,fedora,debian,ubuntu} and <arch> is
# {x86_64,amd64,armel,armbe,armhf,i386}, and not all combinations are
# relevant.  So here are the build container images we need for the target
# list above.  The first column is an alias for that target, associated with
# the particular platform that target would run on.
#
# fedora31 nidas-build-fedora-x86_64:fedora31
# centos7  nidas-build-centos-x86_64:centos7   Dockerfile.centos7
# centos8  nidas-build-centos-x86_64:centos8   Dockerfile.centos8
# buster   nidas-build-debian-amd64:buster
# vulcan   nidas-build-debian-armbe:jessie     Dockerfile.cross_ael_armbe
# titan    nidas-build-debian-armel:jessie     Dockerfile.cross_arm hostarch=armel
# pi2      nidas-build-debian-armhf:jessie     Dockerfile.cross_arm hostarch=armhf
# pi3      nidas-build-debian-armhf:buster     Dockerfile.buster_cross_arm hostarch=armhf (on the buster branch)
# ubuntu   nidas-build-ubuntu-amd64:latest     Dockerfile.ubuntu_amd64
# vortex   nidas-build-ubuntu-i386:bionic      Dockerfile.ubuntu_i386_bionic
#
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
# The original container approach created a separate user called
# 'builder', and then tried to map the user running the container to
# that user, with hardcoded group permissions for eol.  This really
# complicated things.  With rootless podman containers, the user
# running the container maps to the root user in the container, and
# the container has no more privileges in the host OS than that user.
# So all that's necessary is that the mounted source and install
# directories are writable by the user running the container.
# Technically, by running as root the build could seriously and
# unexpectedly break the container, and builds definitely should not
# usually run that way.  Also, there might be permissions bugs in the
# build which get missed by container builds.  However, containers are
# meant to be transient, disposable wrappers, so none of that is worth
# hardcoding non-root users in the containers.

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

  push
    Push packages to the cloud for alias from the given path.  The
    packages must be in <work>/packages/<alias>.

If --tag is given, it can be HEAD, tag, branch, or any git commit.
That commit of the target source is first shallow cloned before
continuing with the build.

The work path will be used to hold clones and packages, like so:

Packages: <work>/packages/<alias>
Clones: <work>/clones/nidas-<tag>

The packages path is mounted in the container at path /nidas/packages.
Packages will be moved there after being built, or else will be pushed
from there.  The work path by default is /tmp/cnidas.

If no tag is given, then no clone is performed and the compile (and
possibly package build) happens inside the source tree.  If no source
is named, then the current repo is used.

EOF
}

targets=(centos7 centos8 vulcan titan pi2 pi3 ubuntu vortex)

# Return the arch for passing to build_dpkg
get_arch() # alias
{
    case "$1" in
        centos*|alma*)
            echo x86_64
            ;;
        pi*)
            echo armhf
            ;;
        titan)
            echo armel
            ;;
        vulcan)
            echo armbe
            ;;
        vortex)
            echo i386
    esac
}

# Return the BUILD setting for the given alias
get_build() # alias
{
    case "$1" in
        centos*|ubuntu*|alma*)
            echo host
            ;;
        pi*)
            echo armhf
            ;;
        titan)
            echo armel
            ;;
        vulcan)
            echo armbe
            ;;
        vortex)
            echo host
            ;;
    esac
}

get_image_tag() # alias
{
    case "$1" in
        alma9)
            echo nidas-build-alma-x86_64:alma9
            ;;
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
        ubuntu)
            echo nidas-build-ubuntu-amd64:latest
            ;;
        vortex)
            echo nidas-build-ubuntu-i386:bionic
            ;;
    esac
}


build_image()
{
    tag=`get_image_tag "$alias"`
    set -x
    case "$alias" in
        alma9)
            podman build -t $tag -f docker/Dockerfile.alma9
            ;;
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
        ubuntu)
            podman build -t $tag -f docker/Dockerfile.ubuntu_amd64
            ;;
        vortex)
            podman build -t $tag -f docker/Dockerfile.ubuntu_i386_bionic
            ;;
    esac
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
    repomoun=""
    if [ -d $DEBIAN_REPOSITORY ]; then
        repomount="--volume ${DEBIAN_REPOSITORY}:/debian:rw,Z"
    fi
    set -x
    # Mount the local scripts directory over top of the source, so the
    # local build scripts are used no matter what version of source is
    # being built.
    exec podman run -i -t \
      --volume "$dest":/nidas:rw,Z \
      --volume "$install":/opt/nidas:rw,Z \
      --volume "$HOME/.scons":/root/.scons:rw,Z \
      --volume "$packagepath":/packages:rw,Z \
      --volume "$scripts":/nidas/scripts:rw,Z \
      $repomount \
      $imagetag "$@"
}

run_scons() # [scons args ...]
{
    run_image scons -C /nidas/src PREFIX=/opt/nidas BUILD=`get_build $alias` "$@"
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


# Push packages to the cloud for the given alias located in the given
# path.  Explicitly avoid uploading dbgsym packages.

push_packages() # path
{
    # list the specific package files that need to be uploaded, ie, not
    # debug symbols
    path="$1"
    if [ -z "$path" ]; then
        echo "push <path>"
        exit 1
    fi
    packages=`ls "$path"/*.deb | egrep -v dbgsym`
    echo $packages
    case "$alias" in
        pi3)
            repo="ncareol/isfs-testing/raspbian/buster"
            (set -x
            package_cloud push $repo $packages)
            ;;
        pi2)
            # This code runs in the debian container with the repo and
            # packages mounted.
            codename=$(source /etc/os-release ; echo "$VERSION" | sed -e 's/.*(//' -e 's/).*//')
            repo=/debian
            arch=armhf
            tmplog=$(mktemp)
            trap "{ rm -f $tmplog; }" EXIT
            status=0
            set -x
            flock $repo sh -c "reprepro -V -b $repo -C main -A $arch --keepunreferencedfiles includedeb $codename $packages" 2> $tmplog || status=$?
            set +x
            if [ $status -ne 0 ]; then
                cat $tmplog
            if grep -E -q "(can only be included again, if they are the same)|(is already registered with different checksums)" $tmplog; then
                echo "One or more package versions are already present in the repository. Continuing"
            else
                exit $status
            fi
            fi
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

        build|run|scons|package|clone|push)
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
        # list all the target aliases and their container image tags
        for target in ${targets[*]}; do
            echo "$target" `get_image_tag "$target"`
        done
        ;;

    clone)
        shift
        clone_local "$tag" "$source" "$workpath/clones/nidas-$tag"
        ;;

    push)
        shift
        push_packages "$@"
        ;;

    *)
        usage
        exit 0
        ;;

esac

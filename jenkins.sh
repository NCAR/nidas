#! /bin/bash

# Gateway script for CI functionality.

# Always run with the script location as the working directory, ie, the top of
# the source tree.
cd "$(dirname "$0")"

# TOPDIR is the path to the top of the rpmbuild output tree.  We have to set
# it here so that each step uses the same value.  Packages are written there
# after being built, then signed, then pushed to the EOL package repository.

# If the Jenkins WORKSPACE environment variable is set, then use it to set
# TOPDIR.  Otherwise use the default that build_rpm.sh would use.
if [ -n "$WORKSPACE" ]; then
    export TOPDIR=$WORKSPACE/rpm_build
fi
if command -v rpmbuild >/dev/null 2>&1 ; then
    export TOPDIR=${TOPDIR:-$(rpmbuild --eval %_topdir)_$(hostname)}
fi

# In EOL Jenkins, these are global properties set in Manage Jenkins ->
# Configure System.  Provide defaults here to test outside of Jenkins.
DEBIAN_REPOSITORY="${DEBIAN_REPOSITORY:-/net/www/docs/software/debian}"
YUM_REPOSITORY="${YUM_REPOSITORY:-/net/www/docs/software/rpms}"
export DEBIAN_REPOSITORY YUM_REPOSITORY
export GPGKEY="NCAR EOL Software <eol-prog2@eol.ucar.edu>"

echo WORKSPACE=$WORKSPACE
echo TOPDIR=$TOPDIR
echo DEBIAN_REPOSITORY=$DEBIAN_REPOSITORY
echo YUM_REPOSITORY=$YUM_REPOSITORY


compile()
{
    if echo -- "$@" | grep -q BUILDS=arm64 ; then
        # at some point this can be removed, but for now the branch for arm64
        # builds does not support the configure and allow_warnings=off build
        # features.
        (set -x; scons -C src -j5 "$@")
    else
        # cache configuration first, then compile with warnings as errors
        (set -x
         scons -C src --config=force configure "$@"
         scons -C src --config=cache allow_warnings=off -j5 "$@")
    fi
}


runtests()
{
    compile
    # run test with same options as compile to avoid recompiling
    (set -x
     scons -C src --config=cache allow_warnings=off test "$@")
}


build_rpms()
{
    # Only clean the rpmbuild space if it's Jenkins, since otherwise it can be
    # the user's local rpmbuild space with unrelated packages, and we should
    # not go around removing them.
    if [ -n "$WORKSPACE" ]; then
        (set -x; rm -rf "$TOPDIR/RPMS"; rm -rf "$TOPDIR/SRPMS")
    fi
    # this conveniently creates a list of built rpm files in src/rpms.txt.
    (set -x; scons -C src build_rpm ../rpm/nidas.spec "$@")
}


arm64_dpkg_dir="scripts/packages/arm64"

build_dsm3_debs()
{
    # put packages where upload script can find them
    (set -x
     rm -rf "$arm64_dpkg_dir"
     mkdir -p "$arm64_dpkg_dir"
     cd scripts && ./build_dpkg.sh -d ../"$arm64_dpkg_dir" arm64)
}


upload_dsm3_debs()
{
    (set -x
     ~/eol-repo/scripts/upload_packages.sh upload "$arm64_dpkg_dir")
}


# [RpmSignPlugin] - Starting signing RPMs ...
# [nc-server-rhel7] $ gpg --fingerprint eol-prog@eol.ucar.edu
# pub   2048R/3376B2DC 2014-08-29
#       Key fingerprint = 80C3 C53F 0D89 4192 CF7B  77A5 DE26 CBC0 3376 B2DC
# uid                  NCAR EOL Software <eol-prog@eol.ucar.edu>
# sub   2048R/1857091F 2014-08-29


# $1 is list of packages to refresh
update_local_packages()
{
    # These commands must be matched by a NOPWCMDS setting in /etc/sudoers.
    # Since centos7 does not support --refresh, we cannot use the more
    # convenient combined command.
    yum="yum -y --disablerepo=* --enablerepo=eol"
    if false ; then
        sudo -n $yum --refresh -- update $1
    else
        sudo -n $yum -- clean expire-cache
        sudo -n $yum -- update $1
    fi
}


method="${1:-help}"
shift

case "$method" in

    compile)
        compile "$@"
        ;;

    test)
        runtests "$@"
        ;;

    build_rpms)
        build_rpms build "$@"
        ;;

    snapshot)
        build_rpms snapshot "$@"
        ;;

    build_dsm3_debs)
        build_dsm3_debs "$@"
        ;;

    upload_dsm3_debs)
        upload_dsm3_debs "$@"
        ;;

    push_rpms)
        source $YUM_REPOSITORY/scripts/repo_funcs.sh
        pkgs=`cat src/rpms.txt`
        push_eol_repo $pkgs
        ;;

    update_rpms)
        pkgs='nc_server-lib nc_server-devel nc_server-clients nc_server'
        update_local_packages $pkgs
        ;;

    # these are a couple of convenient methods for testing: build the
    # cross-compile container and run it similar to how Jenkins would do it.

    arm64)
        # run the rest of the command in the arm64 xdev container, ie, with
        # the same options as Jenkins would use
        if [ "$1" == build ]; then
            set -x
            exec podman build -t nidas-build-debian-arm64:bookworm --build-arg HOST_ARCH=arm64 -f Dockerfile
        fi
        cmd="/workspace/nidas/jenkins.sh $@"
        set -x
        exec podman run -v .:/workspace/nidas:rw,Z -w /workspace -u root nidas-build-debian-arm64:bookworm bash -lc "$cmd"
        ;;

    *)
        if [ "$method" != "help" ]; then
            echo Unknown command "$1".
        fi
        echo Available commands: build_rpms, sign_rpms, push_rpms, update_rpms.
        exit 1
        ;;

esac

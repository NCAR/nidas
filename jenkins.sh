#! /bin/bash

# Gateway script for CI functionality.

# TOPDIR is the path to the top of the rpmbuild output tree.  We have to set
# it here so that each step uses the same value.  Packages are written there
# after being built, then uploaded to the EOL package repository.

# If the Jenkins WORKSPACE environment variable is set, then use it to set
# TOPDIR.  Otherwise use the default that build_rpm.sh would use.
if [ -n "$WORKSPACE" ]; then
    export TOPDIR=$WORKSPACE/rpm_build
fi
export TOPDIR=${TOPDIR:-$(rpmbuild --eval %_topdir)_$(hostname)}

echo WORKSPACE=$WORKSPACE
echo TOPDIR=$TOPDIR


compile()
{
    # cache configuration first, then compile with warnings as errors
    (set -x; cd src
     scons --config=force configure
     scons --config=cache allow_warnings=off -j5)
}


runtests()
{
    compile
    # run test with same options as compile to avoid recompiling
    (set -x; cd src
     scons --config=cache allow_warnings=off test)
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


PKGDIR_BIONIC='src/build/packages-bionic-i386'

build_bionic()
{
    rm -rf ${PKGDIR_BIONIC}
    mkdir -p ${PKGDIR_BIONIC}
    src/nidas/scripts/start_podman bionic /root/current/scripts/build_dpkg.sh -d /root/current/${PKGDIR_BIONIC} i386
}


# $1 is list of packages to refresh
update_local_packages()
{
    # convert rpm files to package names, and leave package names alone.
    # technically the rpm files might include SRPMS, but that shouldn't matter
    # as long as the name matches an installed package.
    package_names=""
    for pkg in "$@" ; do
        case "$pkg" in
            *.rpm)
                pkg=$(rpm -q --qf "%{name}\n" -p "$pkg")
                ;;
        esac
        package_names="$package_names $pkg"
    done

    # These commands must be matched by a NOPWCMDS setting in /etc/sudoers.
    # Use install to either install a new package or update an existing one.
    # As of CentOS 8, dnf supports --refresh.  This could also use --repo=eol
    # in place of the usage below, but this is the usage that matches the
    # NOPWCMDS setting.
    dnf="dnf -y --disablerepo=* --enablerepo=eol --refresh --"
    (set -x; sudo -n $dnf install $package_names)
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

    push_rpms)
        $HOME/eol-repo/scripts/upload_packages.sh upload `cat src/rpms.txt`
        ;;

    update_rpms)
        update_local_packages `cat src/rpms.txt`
        ;;

    build_bionic)
        build_bionic "$@"
        ;;

    upload_bionic)
        $HOME/eol-repo/scripts/upload_packages.sh codename=bionic upload ${PKGDIR_BIONIC}
        ;;

    *)
        if [ "$method" != "help" ]; then
            echo Unknown command "$method".
        fi
        echo Available commands: build_rpms, push_rpms, update_rpms, build_bionic, upload_bionic.
        exit 1
        ;;

esac

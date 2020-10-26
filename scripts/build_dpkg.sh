#!/bin/bash
# vim: set shiftwidth=4 softtabstop=4 expandtab:

set -e

key='<eol-prog@eol.ucar.edu>'

usage() {
    echo "Usage: ${1##*/} [-i repository ] [ -I codename ] arch"
    echo "-c: build in a chroot"
    echo "-i: install packages with reprepro to the repository"
    echo "-I codename: install packages to /net/ftp/pub/archive/software/debian/codename-<codename>"
    echo "-n: don't clean source tree, passing -nc to dpkg-buildpackage"
    echo "arch is armel, armhf, amd64 or i386"
    echo "codename is jessie, xenial or whatever distribution has been enabled on the repo"
    exit 1
}

if [ $# -lt 1 ]; then
    usage $0
fi

arch=amd64
args="--no-tgz-check -sa"
use_chroot=false
while [ $# -gt 0 ]; do
    case $1 in
    -i)
        shift
        repo=$1
        ;;
    -I)
        shift
        codename=$1
        repo=/net/ftp/pub/archive/software/debian/codename-$codename
        ;;
    -c)
        use_chroot=true
        ;;
    -n)
        args="$args -nc -F"
        ;;
    armel)
        export CC=arm-linux-gnueabi-gcc
        arch=$1
        ;;
    armhf)
        export CC=arm-linux-gnueabihf-gcc
        arch=$1
        ;;
    amd64)
        arch=$1
        ;;
    i386)
        arch=$1
        ;;
    *)
        usage $0
        ;;
    esac
    shift
done

if [ -n "$repo" ]; then
    distconf=$repo/conf/distributions
    if [ -r $distconf ]; then
        codename=$(fgrep Codename: $distconf | cut -d : -f 2)
        key=$(fgrep SignWith: $distconf | cut -d : -f 2)
        # first architecture listed
        primarch=$(fgrep Architectures: $distconf | cut -d : -f 2 | awk '{print $1}')
    fi

    if [ -z "$codename" ]; then
        echo "Cannot determine codename of repository at $repo"
        exit 1
    fi
    export GPG_TTY=$(tty)
    # do a test signing.  gpg2 and reprepro contact the gpg-agent via
    # the .gnupg/S.gpg-agent socket for the key passphrase.
    # When running a Ubuntu xenial container on a RHEL7 host, there
    # is a version mismatch between gpg2/reprepro in the container
    # and the gpg-agent on the host:
    #     gpg: WARNING: server 'gpg-agent' is older than us (2.0.22 < 2.1.11)
    # A solution is to kill the agent:  echo killagent | gpg-connect-agent
    # This can be done on the host before running the container, or I guess
    # it could be done here in the container.
    echo test | gpg2 --clearsign --default-key "$key" > /dev/null
fi

args="$args -a$arch"

if $use_chroot; then
    dist=$(lsb_release -c | awk '{print $2}')
    if [ $arch == amd64 ]; then
        chr_name=${dist}-amd64-sbuild
    else
        chr_name=${dist}-amd64-cross-${arch}-sbuild
    fi
    if ! schroot -l | grep -F chroot:${chr_name}; then
        echo "chroot named ${chr_name} not found"
        exit 1
    fi
fi

sdir=$(realpath $(dirname $0))
cd $sdir/..

# This
#       export CC=arm-linux-gnueabi-gcc
#       debuild -aarmel ...
# reports:
# "dpkg-architecture: warning: specified GNU system type arm-linux-gnueabi
# does not match gcc system type x86_64-linux-gnu, try setting a correct
# CC environment variable".
# This is with debuild 2.15.3, and dpkg-buildpackage/dpkg-architecture 1.17.26.
# Various posts on the web indicate the warning is bogus and can be ignored.

# -tarm-linux-gnueabi does't seem to be passed correctly from debuild
# through dpkg-buildpackage to dpkg-architecture.  So use -aarmel.

# To check the environment set by dpkg-architecture:
#   dpkg-architecture -aarmel -c env

# create changelog
$sdir/deb_changelog.sh > debian/changelog

# The package tools report that using DEB_BUILD_HARDENING is obsolete,
# so we set the appropriate compiler and link options in the SConscript
# export DEB_BUILD_HARDENING=1

# These lintian errors are being suppressed:
#   dir-or-file-in-opt:  since the package installs things to /opt/nidas.
#       It would be nice to put them in /usr/bin, etc, but then some
#       of the nidas executables should probably be renamed to avoid possible
#       name conflicts with other packages, which could be a large number
#       if nidas is installed on a general-purpose system.
#       On a Debian jessie system I don't see any sub-directoryes of /usr/bin.
#   package-modifies-ld.so-search
#       Debian wants us to use -rpath instead. May want to re-visit this.
#   package-name-doesnt-match-sonames
#       This seems to be the result of having multiple libraries 
#       in one package?

args="$args -us -uc"

# clean old results
rm -f ../nidas_*.tar.xz ../nidas_*.dsc
rm -f $(echo ../nidas\*_{$arch,all}.{deb,build,changes})

# export DEBUILD_DPKG_BUILDPACKAGE_OPTS="$args"

if $use_chroot; then
    # as normal user, could not
    # sbuild-shell ${dist}-${arch}-sbuild
    # but could
    echo "Starting schroot, which takes some time ..."
    schroot -c $chr_name --directory=$PWD << EOD
        set -e
        [ -f $HOME/.gpg-agent-info ] && . $HOME/.gpg-agent-info
        export GPG_AGENT_INFO
        debuild $args \
            --lintian-opts --suppress-tags dir-or-file-in-opt,package-modifies-ld.so-search-path,package-name-doesnt-match-sonames
EOD
else
    debuild $args \
        --lintian-opts --suppress-tags dir-or-file-in-opt,package-modifies-ld.so-search-path,package-name-doesnt-match-sonames
fi

# debuild puts results in parent directory
cd ..

if [ -n "$repo" ]; then
    umask 0002

    if ! which reprepro 2> /dev/null; then
        cmd="sudo apt-get install -y reprepro"
        echo "reprepro not found, doing: $cmd. Better yet add it to the image"
        $cmd
    fi

    echo "Build results:"
    ls
    echo ""

    chngs=nidas_*_$arch.changes 
    # display changes file
    echo "Contents of $chngs"
    cat $chngs
    echo ""

    archdebs=nidas*$arch.deb

    # Grab all the package names from the changes file
    pkgs=($(awk '/Checksums-Sha1/,/Checksums-Sha256/ { if (NF > 2) print $3 }' $chngs | grep ".*\.deb" | sed "s/_.*_.*\.deb//"))


    # echo "chngs=$chngs"
    # echo "pkgs=$pkgs"
    # echo "archdebs=$archdebs"

    # Use --keepunreferencedfiles so that the previous version .deb files 
    # are not removed. Then user's who don't do an apt-get update will
    # get the old version without an error. Nightly, or once-a-week one could do
    # a deleteunreferenced.

    # try to catch the reprepro error which happens when it tries to
    # install a package version that is already in the repository.
    # This repeated-build situation can happen in jenkins if a
    # build is triggered by a pushed commit, but git pull grabs
    # an even newer commit, and a build for the newer commit is then
    # triggered later.

    # reprepro has a --ignore option with many types of errors that
    # can be ignored, but I don't see a way to ignore this error,
    # so we'll use a fixed grep.

    tmplog=$(mktemp)
    trap "{ rm -f $tmplog; }" EXIT

    for (( i=0; i < 2; i++ )); do
        status=0
        set -v
        set +e

        # For the first architecture listed in confg/distributions, install
        # all the packages listed in the changes file, including source and
        # "all" packages.
        if [ $arch == $primarch ]; then
            echo "Installing ${pkgs[*]}"
            if [ $i -gt 0 ]; then
                for pkg in ${pkgs[*]}; do
                    # Specifying -A $arch\|source\|all with a remove
                    # doesn't work.
                    # A package built for all archs will be placed into
                    # the repo for each architecture in the repo, but "all"
                    # is ignored in -A for the remove.  So a package for all,
                    # that is installed in the repo for amd64 won't be
                    # removed with -A i386|source|all", and you'll get
                    # a "registered with different checksums" error if
                    # you try to install it for i386. So leave -A off.
                    flock $repo sh -c "
                        reprepro -V -b $repo remove $codename $pkg"
                done
                flock $repo sh -c "
                    reprepro -V -b $repo deleteunreferenced"

            fi

            flock $repo sh -c "
                reprepro -V -b $repo -C main --keepunreferencedfiles include $codename $chngs" 2> $tmplog || status=$?

        # If not the first architecture listed, just install the
        # specific architecture packages.
        else
            echo "Installing $archdebs"

            if [ $i -gt 0 ]; then
                for pkg in ${archdebs[*]}; do
                    # remove last two underscores
                    pkg=${pkg%_*}
                    pkg=${pkg%_*}
                    flock $repo sh -c "
                        reprepro -V -b $repo -A $arch remove $codename $pkg"
                done
                flock $repo sh -c "
                    reprepro -V -b $repo deleteunreferenced"
            fi

            flock $repo sh -c "
                reprepro -V -b $repo -C main -A $arch --keepunreferencedfiles includedeb $codename $archdebs" 2> $tmplog || status=$?
        fi
        echo "status=$status"

        [ $status -eq 0 ] && break

        cat $tmplog
        if grep -E -q "(can only be included again, if they are the same)|(is already registered with different checksums)" $tmplog; then
            echo "One or more package versions are already present in the repository. Removing and trying again"
        fi
    done

    if [ $status -eq 0 ]; then
        rm -f nidas_*_$arch.build nidas_*.dsc nidas_*.tar.xz \
            nidas*_all.deb nidas*_$arch.deb $chngs
    else
        echo "saving results in $PWD"
    fi

else
    echo "build results are in $PWD"
fi


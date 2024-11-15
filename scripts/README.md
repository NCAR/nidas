# README for nidas/scripts

This directory contains scripts for doing builds of NIDAS under Debian and
other containers, supporting amd64 builds and cross-builds for armel (namely
Eurotech viper and titan), and armhf (RaspberryPi 2 and 3).

The builds can be done from a full Debian system (which can of course be
virtual) or from within a docker/podman container.

For information on creating and running docker/podman images see the [Using
Podman/Docker](https://wiki.ucar.edu/pages/viewpage.action?pageId=458588376)
wiki page.

Several scripts are used for building Debian packages.  Builds are simpler
from a container, so using a chroot is no longer recommended.

The docker directory contains scripts and Dockerfiles for creating
docker/podman images for cross-building.

## Scripts

All `Dockerfile` files, as well as the scripts listed below which build them,
are in the [docker](./docker/) subdirectory.

### Building container images for VORTEX

```plain
./build_vortex_docker.sh release
```

### Building Debian jessie image

A Debian Jessie container is used to cross-build for armel (viper,titan) and
armhf (RPi2).

```plain
./build_arm_docker.sh
```

### Running containers

Here is how to run a container using podman for building nidas packages, or
for getting a shell prompt in the container.  The
[run_podman.sh](run_podman.sh) script is in this directory.

Note there is a very similar script
[start_podman.sh](../src/nidas/scripts/start_podman) in the
`src/nidas/scripts` directory in the source tree, and that script is installed
into the NIDAS `bin` directory.

```plain
run_podman.sh [-p] [ armel | armhf | armbe | xenial | bionic ]
    -p: pull image from docker.io

    # If you have started a bionic image for building i386 vortex packages,
    # once the container shell is running, do:

    cd /root/nidas/scripts
    ./build_dpkg.sh -I bionic i386

    # If you have started an armel image for cross-building viper/titan packages,
    # once the container shell is running, do:

    cd /root/nidas/scripts
    ./build_dpkg.sh -I jessie armel
```

```plain
build_dpkg.sh [-c] [-I codename] [-i repo] arch
    Build debian packages of NIDAS for the specified architecture
    -c: build package under a chroot.  Not necessary from within docker/podman containers
    -I codename: install packages to /net/www/docs/software/debian/codename-<codename>
    -i repo: install debian packages to the given repository
    arch is armel, armhf, amd64 or i386 (vortex)
    codename is jessie (for armel), bionic (for vortex) or whatever distribution has been enabled
    /net/www/docs/software/debian
```

## cnidas.sh

The `cnidas.sh` script is an attempt to consolidate scripting for container
builds in a single script, using the same Dockerfiles as used by the scripts
above.  It can build containers for different OS release and architecture
targets from the available Dockerfiles, but it also automates mounting the
source tree, install paths, and package output directory into the container,
so all the container outputs will be available on the container host.

Run the script without arguments to see usage.  The target alias is generally
the first argument, and the available aliases can be seen with `cnidas.sh
list`.  The file
[Develop_Pi.md](https://github.com/NCAR/nidas/blob/buster/Develop_Pi.md) on
the buster branch has specific information for Pi3 builds, but it also has
some general information and useful examples for `cnidas.sh`.

Following are some notes on using `cnidas.sh` for specific targets.

### Vortex (Ubuntu Bionic i386)

The Dockerfile
[Dockerfile.ubuntu_i386_bionic](docker/Dockerfile.ubuntu_i386_bionic) is used
to build container images for building NIDAS on Ubunu Bionic i386.  If
`dolocal` is yes, the default, then certain dependencies are installed from
the EOL repository:

    linux-headers-4.15.18-vortex86dx3 eol-scons eol-repo libxerces-c3.2
    uio48-dev

To use the container to build the EOL packages before they are available, pass
`dolocal=no`:

    ./cnidas.sh vortext build --build-arg=dolocal=no

The container can also be used to test the `ads-daq` and related packages.
Clone `embedded-daq` somewhere in the nidas source directory so it will be
available in the container, then the packages can be built and also installed
in the container:

    ./cnidas.sh vortex build
    ./cnidas.sh vortex run
    cd /nidas/embedded-daq/ads-daq
    ./build_dpkg.sh
    cd ../eol-daq
    ./build_dpkg.sh

Newer versions of `apt-get` should support this command to test install the
packages:

    apt-get install ./eol-daq_*_all.deb ../ads-daq/ads-daq_*_all.deb

But if not, then try this:

    dpkg -i ./eol-daq_*_all.deb ../ads-daq/ads-daq_*_all.deb
    apt-get install -f

The installaction scripts in those packages interact with systemd, but that
does not work if systemd is not running inside the container.  It is possible
to run `systemd` in the container, so that systemd interactions in package
`postinst` scripts can be tested, as follows:

    ./cnidas.sh vortex run systemd
    <login as root>
    cd /nidas/embedded-daq
    apt-get install */*-daq*.deb

When the container runs `systemd`, then it takes control of the terminal, so
the container has to be stopped by running `podman stop` in a different
terminal.

The `postinst` scripts expect the `ads` account to exist, and giving the `ads`
and `root` accounts passwords allows logins to those accounts from the
`systemd` login prompt.  However, those are not done by default in the image,
only if `setup_users=yes` is passed as a build arg when the image is built:

    ./cnidas.sh vortex build --build-arg=setup_users=yes

The `cnidas.sh` script will also mount the EOL Debian repository if it exists
when running the container, so the container can be used to manage the
repository.  Below is an example of listing all the packages known to the
repository.  Note that it must be run as the `jenkins` user to have
permissions to create a lock file in the repository:

    [jenkins@eol-rosetta scripts]$ ./cnidas.sh vortex run reprepro -V -b /debian/codename-bionic -C main --keepunreferencedfiles list bionic
    Source tree path: /var/lib/jenkins/workspace/NIDAS
    Top of nidas source tree: /var/lib/jenkins/workspace/NIDAS
    + exec podman run --rm -i -t --volume /net/www/docs/software/debian:/debian:rw,Z --volume /var/lib/jenkins/workspace/NIDAS:/nidas:rw,Z --volume /tmp/cnidas/install/vortex:/opt/nidas:rw,Z --volume /tmp/cnidas/packages/vortex:/packages:rw,Z --volume /var/lib/jenkins/workspace/NIDAS/scripts:/nidas/scripts:rw,Z nidas-build-ubuntu-i386:bionic reprepro -V -b /debian/codename-bionic -C main --keepunreferencedfiles list bionic
    WARNING: image platform (linux/386) does not match the expected platform (linux/amd64)
    bionic|main|i386: ads-daq 1.0-453
    bionic|main|i386: ads-daq2 1.0+453
    bionic|main|i386: ads-vortex 1.0-424
    ...
    bionic|main|source: nidas 1.2.3+118
    bionic|main|source: uio48 1.0+10
    bionic|main|source: wdt-vortex 1.0+10


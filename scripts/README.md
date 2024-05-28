# README for nidas/scripts

This directory contains scripts for doing builds of NIDAS under Debian and
other containers, supporting amd64 builds and cross-builds for armel (namely
Eurotech viper and titan), and armhf (RaspberryPi 2 and 3).

The builds can be done from a full Debian system (which can of course be
virtual) or from within a docker/podman container.

For information on creating and running docker/podman images see the [Using
Podman/Docker](https://wiki.ucar.edu/pages/viewpage.action?pageId=458588376)
wiki page.

Several scripts are used for building Debian packages.

Builds are simpler from a container, so using a chroot is no longer
recommended.  If you still want to use a chroot, see
[HowToPackageForDebian](https://wiki.debian.org/HowToPackageForDebian).  sbuild is used to create the
necessary chroots, one for each architecture, and schroot or sbuild-shell to
execute commands in the chroots.

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


# NIDAS Development for the Raspberry Pi3

NIDAS is more easily developed and tested on a desktop Linux system and then
cross-compiled to Raspbian armhf for the Pi3.  The NIDAS source tree tries to
simplify this development with containers and scripts.  Following is some
explanation of the steps.

## Build the Debian build container

The [scripts/cnidas.sh](scripts/cnidas.sh) script uses
[podman](https://podman.io/) to build a Debian image with all the dependencies
to build NIDAS, and it also provides shortcuts to run the container from that
image.  The image can be pulled from Docker hub or built locally.  There are
two available Debian images, to cross-compile for 32-bit armhf on Buster or
for aarch64 on Bookworm.

To use the arm64 target on Bookworm, replace all the uses of `pi3` with `arm64` in the
`cnidas.sh` commands below.

### Pull the image

The image can be pulled directly from Docker hub, and also run:

```plain
podman pull docker://docker.io/ncar/nidas-build-debian-armhf:latest
podman run -i -t docker.io/ncar/nidas-build-debian-armhf /bin/bash
```

### Build the image

Build the image with `cnidas.sh pi3 build` or (`cnidas.sh arm64 build`):

```plain
(base) granger@snoopy:/home/granger/code/nidas/scripts$ ./cnidas.sh pi3 build
Source tree path: /home/granger/code/nidas
+ case "$alias" in
+ podman build -t nidas-build-debian-armhf:buster -f docker/Dockerfile.buster_cross_arm --build-arg hostarch=armhf
STEP 1/21: FROM docker.io/debian:buster
STEP 2/21: LABEL organization="NCAR EOL"
...
COMMIT nidas-build-debian-armhf:buster
--> 72a14106fc25
Successfully tagged localhost/nidas-build-debian-armhf:buster
72a14106fc252a445cddbc1ef5832b9398cb3d33a9a22382882294fc173d1223
```

If the image on Docker hub needs to be updated, these are the steps to push
the image built by `cnidas.sh` above:

```plain
podman login docker.io
podman push nidas-build-debian-armhf:buster docker://docker.io/ncar/nidas-build-debian-armhf:latest
```

As indicated in the `podman build` command above, the steps to build this
image are in
[Dockerfile.buster_cross_arm](scripts/docker/Dockerfile.buster_cross_arm).
The image builds and installs a couple dependencies from source:

- ublox binary message library
- xmlrpc++

These are built from source in the container so that building the image does
not require getting those packages from another package repository, such as
from EOL.

NIDAS builds also depend on [eol_scons](https://github.com/NCAR/eol_scons).
However, rather than install that into the container, the `cnidas.sh` script
mounts `~/.scons` into the container instead.  That way the container uses the
same version of `eol_scons` in the container that is used for native host
builds, and it allows the `eol_scons` on the host to be modified and tested
against container builds.  This can be very helpful for testing changes to
`eol_scons`, since they must work on several OS varieties.

This means the developer needs to have cloned `eol_scons` on the host:

```plain
mkdir -p ~/.scons/site_scons
cd ~/.scons/site_scons
git clone https://github.com/NCAR/eol_scons
```

## Building the source

The `cnidas.sh` script and the build image can be used to build the NIDAS
source in different ways.  Most common will be to build the local source tree,
including any local modifications.  The script has a `scons` subcommand to run
`scons` inside the container, with whatever arguments follow `scons` on the
`cnidas.sh` command-line.

```plain
(base) granger@snoopy:/home/granger/code/nidas/scripts$ ./cnidas.sh pi3 scons
Source tree path: /home/granger/code/nidas
Top of nidas source tree: /home/granger/code/nidas
+ exec podman run -i -t --volume /home/granger/code/nidas:/nidas:rw,Z --volume /tmp/cnidas/install/pi3:/opt/nidas:rw,Z --volume /h/eol/granger/.scons:/root/.scons:rw,Z --volume /tmp/cnidas/packages/pi3:/packages:rw,Z --volume /home/granger/code/nidas/scripts:/nidas/scripts:rw,Z --volume /net/ftp/pub/archive/software/debian:/debian:rw,Z nidas-build-debian-armhf:buster scons -C /nidas/src PREFIX=/opt/nidas BUILDS=armhf
scons: Entering directory `/nidas/src'
scons: Reading SConscript files ...
Loading eol_scons from /root/.scons/site_scons/eol_scons/eol_scons...
Config files: ['/nidas/src/nidas.conf']
...
rm-linux-gnueabihf-g++ -o build_armhf/apps/xml_dump -pthread -pie -Wl,-z,relro -Wl,-z,now -rdynamic -Xlinker -rpath-link=build_armhf/util:build_armhf/core build_armhf/apps/xml_dump.o -Lbuild_armhf/util -Lbuild_armhf/core -Lbuild_armhf/dynld -L/usr/lib/arm-linux-gnueabihf -lgsl -lgslcblas -lm -lxerces-c -lxmlrpcpp build_armhf/dynld/libnidas_dynld.so build_armhf/core/libnidas.so build_armhf/util/libnidas_util.so -lboost_system -lboost_filesystem -lboost_regex -lpthread /usr/lib/arm-linux-gnueabihf/libXmlRpcpp.a -ljsoncpp -ldl -lxerces-c -lftdi1
Generating build_armhf/include/nidas/linux/Revision.h
scons: done building targets.
```

Notice the source tree path on the first two lines: this is the location of
the source on the host that is mounted into the container at `/nidas`.  When
`scons` is run inside the container, it runs in `/nidas`, so the `nidas.conf`
file from the host source tree shows up as `/nidas/src/nidas.conf` inside the
container.

Note that whatever `nidas.conf` file is on the host is used in the container,
so any configuration settings there should not conflict.  The `PREFIX` setting
is an exception.  `cnidas.sh` explicitly sets `PREFIX=/opt/nidas` on the
`scons` command-line, but mounts an install location on the host to
`/opt/nidas` in the container.  The `podman` command in the output above shows
this mount option.  By default the host install location is
`/tmp/cnidas/install/pi3`, but it can be changed with the `--work` option to
`cnidas.sh`.  For example, this will install into
`/home/granger/work/install/pi3`:

```plain
./cnidas.sh --work /home/granger/work pi3 scons install
```

The same work path can be used for multiple container builds, since the
`install` and `packages` directories under the work path are further divided
by target, such as `pi3`.

The `scons` subcommand is one way to iterate on source code development while
tseting it for Pi3 builds.  Modify the source on the local host, then run
`cnidas.sh pi3 scons` to check the build.  (This command could also be used as
the build command for an IDE like vscode, except I'm not sure how vscode will
handle the different source file paths in the error messages.)  Or it works to
just run the container and keep it open, then trigger builds from there.
Remember to pass the target in the BUILDS setting or set it in `nidas.conf`,
otherwise `scons` builds for the container architecture, as if it was a `host`
build, and not for the Pi3 architecture.

```plain
(base) granger@snoopy:/home/granger/code/nidas/scripts$ ./cnidas.sh pi3 run
Source tree path: /home/granger/code/nidas
Top of nidas source tree: /home/granger/code/nidas
+ exec podman run -i -t --volume /home/granger/code/nidas:/nidas:rw,Z --volume /tmp/cnidas/install/pi3:/opt/nidas:rw,Z --volume /h/eol/granger/.scons:/root/.scons:rw,Z --volume /tmp/cnidas/packages/pi3:/packages:rw,Z --volume /home/granger/code/nidas/scripts:/nidas/scripts:rw,Z --volume /net/ftp/pub/archive/software/debian:/debian:rw,Z nidas-build-debian-armhf:buster /bin/bash
root@c4853e4b4315:~# pwd
/root
root@c4853e4b4315:~# cd /nidas/src
root@c4853e4b4315:/nidas/src# scons BUILDS=armhf
```

## Testing on a Pi

To test a NIDAS build on a Pi, it is necessary to cross-compile and install
from the source then copy that install tree to the Pi3.  `cnidas.sh` can run
the install, then the script [dsm_rsync.sh](scripts/dsm_rsync.sh) can copy the
tree.  Here is the install:

```plain
(base) granger@snoopy:/home/granger/code/nidas/scripts$ ./cnidas.sh pi3 scons install
Source tree path: /home/granger/code/nidas
Top of nidas source tree: /home/granger/code/nidas
+ exec podman run -i -t --volume /home/granger/code/nidas:/nidas:rw,Z --volume /tmp/cnidas/install/pi3:/opt/nidas:rw,Z --volume /h/eol/granger/.scons:/root/.scons:rw,Z --volume /tmp/cnidas/packages/pi3:/packages:rw,Z --volume /home/granger/code/nidas/scripts:/nidas/scripts:rw,Z --volume /net/ftp/pub/archive/software/debian:/debian:rw,Z nidas-build-debian-armhf:buster scons -C /nidas/src PREFIX=/opt/nidas BUILDS=armhf install
scons: Entering directory `/nidas/src'
...
Install file: "build_armhf/apps/gps_nmea_sysclock" as "/opt/nidas/bin/gps_nmea_sysclock"
Install file: "build_armhf/apps/garmin" as "/opt/nidas/bin/garmin"
Install file: "nidas/scripts/setup_nidas.sh" as "/opt/nidas/bin/setup_nidas.sh"
Install file: "xml/nidas.xsd" as "/opt/nidas/share/xml/nidas.xsd"
scons: done building targets.
```

Here is the `dsm_rsync.sh` command:

```plain
(base) granger@snoopy:/home/granger/code/nidas/scripts$ ./dsm_rsync.sh -P /tmp/cnidas/install/pi3 root@fl1.fl-guest.ucar.edu:/opt/nidas-dev
Source arch is armhf.
Now running rsync ops...
+ rsync -zhvrlptDH -P /tmp/cnidas/install/pi3/bin /tmp/cnidas/install/pi3/firmware /tmp/cnidas/install/pi3/include /tmp/cnidas/install/pi3/share root@fl1.fl-guest.ucar.edu:/opt/nidas-dev
sending incremental file list
...
```

The script just makes sure to `rsync` all of the install tree, except the
libraries must be put in an architecture-specific subdirectory.  Run
`dsm_rsync.sh` without arguments to see usage.  The `-n` option can be helpful
to get an idea of what will be copied and where, without actually copying
anything.  It is also convenient to setup ssh key authentication to the Pi,
otherwise each `rsync` run by the script prompts for a password.

Normally the install is copied to a different location than the package
location, so as above `/opt/nidas-dev` and not `/opt/nidas`.  To use the
alternate install location, login to the Pi and add the NIDAS binaries to the
PATH with `setup_nidas.sh`:

```plain
(base) granger@snoopy:/home/granger/code/nidas/scripts$ ssh daq@fl1.fl-guest.ucar.edu
...
daq@fl1:~ $ source /opt/nidas-dev/bin/setup_nidas.sh 
daq@fl1:~ $ which pio
/opt/nidas-dev/bin/pio
daq@fl1:~ $ pio
dcdc    on   
bank1   on   
bank2   on   
aux     on   
port0   on   485f noterm  
port1   on   232  noterm  
port2   on   485h noterm  
port3   on   485f noterm  
port4   on   232  noterm  
port5   on   485f noterm  
port6   on   232  noterm  
port7   on   232  noterm  
p1      off  up
wifi    off  up
daq@fl1:~ $ dsm -v
Version: v1.3-343M
```

The `dsm` version should match the `git describe` output from the source tree
on the build host:

```plain
(base) granger@snoopy:/home/granger/code/nidas/scripts$ git describe
v1.3-343-gd7f303b0
```

## Building source from tags

The `cnidas.sh` script has the `--source` option  to build a different source
tree from the one where `cnidas.sh` is located, and it also has the `--tag`
option to build a specific tag from the source.  To build from a tag,
`cnidas.sh` clones the local source tree and checks out the given tag; it does
not fetch from `github`.  So the tag that is built is whatever the tag points
to in the local clone, and that may not match what is on `github` if a branch
has not been pulled from `github` or a local tag has not been pushed there.

```plain
(base) granger@snoopy:/home/granger/code/nidas/scripts$ ./cnidas.sh --tag buster --work /home/granger/work pi3 scons install
Source tree path: /home/granger/code/nidas
Using clone of buster
Top of nidas source tree: /home/granger/code/nidas
+ '[' -d /home/granger/work/clones/nidas-buster ']'
+ git clone . /home/granger/work/clones/nidas-buster
Cloning into '/home/granger/work/clones/nidas-buster'...
done.
...
```

## Building packages with a container

The `cnidas.sh` script can also be used to build the NIDAS Debian packages for
the Pi3.  The `package` subcommand runs the
[build_dpkg.sh](scripts/build_dpkg.sh) script inside the container, and that
script copies the finished package files to `/packages`, which is mounted onto
the `cnidas.sh` work path.  Here is an example:

```plain
(base) granger@snoopy:/home/granger/code/nidas/scripts$ ./cnidas.sh pi3 package
Source tree path: /home/granger/code/nidas
Top of nidas source tree: /home/granger/code/nidas
+ exec podman run -i -t --volume /home/granger/code/nidas:/nidas:rw,Z --volume /tmp/cnidas/install/pi3:/opt/nidas:rw,Z --volume /h/eol/granger/.scons:/root/.scons:rw,Z --volume /tmp/cnidas/packages/pi3:/packages:rw,Z --volume /home/granger/code/nidas/scripts:/nidas/scripts:rw,Z --volume /net/ftp/pub/archive/software/debian:/debian:rw,Z nidas-build-debian-armhf:buster /nidas/scripts/build_dpkg.sh armhf -d /packages
+ debuild --prepend-path=/usr/local/bin:/root/.pyenv/shims:/root/.pyenv/bin -sa -aarmhf -us -uc --lintian-opts --suppress-tags dir-or-file-in-opt,package-modifies-ld.so-search-path,package-name-doesnt-match-sonames
...
dpkg-deb: building package 'nidas-buildeol' in '../nidas-buildeol_1.3+343_all.deb'.
dpkg-deb: building package 'nidas-min' in '../nidas-min_1.3+343_armhf.deb'.
dpkg-deb: building package 'nidas-dbgsym' in '../nidas-dbgsym_1.3+343_armhf.deb'.
dpkg-deb: building package 'nidas' in '../nidas_1.3+343_armhf.deb'.
dpkg-deb: building package 'nidas-dev' in '../nidas-dev_1.3+343_armhf.deb'.
dpkg-deb: building package 'nidas-libs-dbgsym' in '../nidas-libs-dbgsym_1.3+343_armhf.deb'.
dpkg-deb: building package 'nidas-daq' in '../nidas-daq_1.3+343_all.deb'.
dpkg-deb: building package 'nidas-libs' in '../nidas-libs_1.3+343_armhf.deb'.
dpkg-deb: building package 'nidas-build' in '../nidas-build_1.3+343_all.deb'.
 dpkg-genbuildinfo
 dpkg-genchanges -sa >../nidas_1.3+343_armhf.changes
dpkg-genchanges: info: including full source code in upload
 dpkg-source --after-build .
dpkg-buildpackage: info: full upload; Debian-native package (full source is included)

real    4m31.646s
user    11m14.436s
sys     1m5.878s
moving results to /packages
```

The packages end up in `/tmp/cnidas/packages/pi3`:

```plain
(base) granger@snoopy:/home/granger/code/nidas/scripts$ ls -1 /tmp/cnidas/packages/pi3
nidas_1.3+343_armhf.build
nidas_1.3+343_armhf.changes
nidas_1.3+343_armhf.deb
nidas_1.3+343.dsc
nidas_1.3+343.tar.xz
nidas-build_1.3+343_all.deb
nidas-buildeol_1.3+343_all.deb
nidas-daq_1.3+343_all.deb
nidas-dbgsym_1.3+343_armhf.deb
nidas-dev_1.3+343_armhf.deb
nidas-libs_1.3+343_armhf.deb
nidas-libs-dbgsym_1.3+343_armhf.deb
nidas-min_1.3+343_armhf.deb
```

The package files can be copied to a Pi3 and installed directly (maybe there
should be a script for that), or they can be pushed to the `isfs-testing`
repository on [packagecloud](https://packagecloud.io/).

## Pushing packages to packagecloud

The `cnidas.sh` script can push package files to the `isfs-testing` repository
on [packagecloud](https://packagecloud.io/), using the `package_cloud` Ruby
gem:

```plain
(base) granger@snoopy:/home/granger/code/nidas/scripts$ ./cnidas.sh pi3 push /tmp/cnidas/packages/pi3
Source tree path: /home/granger/code/nidas
/tmp/cnidas/packages/pi3/nidas_1.3+343_armhf.deb /tmp/cnidas/packages/pi3/nidas-build_1.3+343_all.deb /tmp/cnidas/packages/pi3/nidas-buildeol_1.3+343_all.deb /tmp/cnidas/packages/pi3/nidas-daq_1.3+343_all.deb /tmp/cnidas/packages/pi3/nidas-dev_1.3+343_armhf.deb /tmp/cnidas/packages/pi3/nidas-libs_1.3+343_armhf.deb /tmp/cnidas/packages/pi3/nidas-min_1.3+343_armhf.deb
+ package_cloud push ncareol/isfs-testing/raspbian/buster /tmp/cnidas/packages/pi3/nidas_1.3+343_armhf.deb /tmp/cnidas/packages/pi3/nidas-build_1.3+343_all.deb /tmp/cnidas/packages/pi3/nidas-buildeol_1.3+343_all.deb /tmp/cnidas/packages/pi3/nidas-daq_1.3+343_all.deb /tmp/cnidas/packages/pi3/nidas-dev_1.3+343_armhf.deb /tmp/cnidas/packages/pi3/nidas-libs_1.3+343_armhf.deb /tmp/cnidas/packages/pi3/nidas-min_1.3+343_armhf.deb
...
```

This requires the `package_cloud` Ruby gem to be installed and an
authenticaion key from `packagecloud`.  Unfortunately the gem has proven
problematic to install and run without errors on Fedora, which would be the
most convenient since the packagecloud credentials could be cached on the host
and not be needed in the container.

It is possible to run `package_cloud` from inside the container, in which case
the script will prompt for credentials.  However, that is risky since the
authentication token is cached in `/root/.packagecloud`, and those credentials
will stay there if the container and the container's image are not removed.

```plain
root@35cf8923cc2d:/nidas/scripts# ./cnidas.sh pi3 push /packages
Source tree path: /nidas
/packages/nidas-build_1.3+343_all.deb /packages/nidas-buildeol_1.3+343_all.deb /packages/nidas-daq_1.3+343_all.deb /packages/nidas-dev_1.3+343_armhf.deb /packages/nidas-libs_1.3+343_armhf.deb /packages/nidas-min_1.3+343_armhf.deb /packages/nidas_1.3+343_armhf.deb
+ package_cloud push ncareol/isfs-testing/raspbian/buster /packages/nidas-build_1.3+343_all.deb /packages/nidas-buildeol_1.3+343_all.deb /packages/nidas-daq_1.3+343_all.deb /packages/nidas-dev_1.3+343_armhf.deb /packages/nidas-libs_1.3+343_armhf.deb /packages/nidas-min_1.3+343_armhf.deb /packages/nidas_1.3+343_armhf.deb
No config file exists at /root/.packagecloud. Login to create one.
...
Got your token. Writing a config file to /root/.packagecloud... success!
Looking for repository at ncareol/isfs-testing... success!
Pushing /packages/nidas-buildeol_1.3+343_all.deb... success!
Pushing /packages/nidas-daq_1.3+343_all.deb... success!
Pushing /packages/nidas-dev_1.3+343_armhf.deb... success!
Pushing /packages/nidas-libs_1.3+343_armhf.deb... success!
Pushing /packages/nidas-min_1.3+343_armhf.deb... success!
Pushing /packages/nidas_1.3+343_armhf.deb... success!
Pushing /packages/nidas-build_1.3+343_all.deb... success!
```

This shows that the credentials are still available if the container is restarted:

```plain
(base) granger@snoopy:/home/granger/code/nidas/scripts$ podman restart 35cf8923cc2d
35cf8923cc2d
(base) granger@snoopy:/home/granger/code/nidas/scripts$ podman attach 35cf8923cc2d
root@35cf8923cc2d:~# ls .packagecloud 
.packagecloud
```

The `cnidas.sh` script adds `--rm` to each podman run command to remove the
container when it exits.  However, it is still possible to run the image
without `cnidas.sh` and forget to remove the container afterwards.

It should also be possible to mount the user credentials cached on the host
into the container, but that hasn't been tried yet.

If all else fails, package files can be uploaded through the
[packagecloud](https://packagecloud.io/ncareol/isfs-testing) web site.

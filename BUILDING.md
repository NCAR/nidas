# Building NIDAS

## Install Dependencies

NIDAS uses [SCons](https://scons.org/) and
[eol_scons](https://github.com/NCAR/eol_scons) to build from source.  Install
`SCons` as a system package or with `pip`.  `eol_scons` is most easily
installed by cloning it into the one of the locations searched by `SCons`, as
shown below.

```sh
mkdir -p ~/.scons/site_scons
cd ~/.scons/site_scons
git clone https://github.com/ncar/eol_scons
```

See the [SCons](https://scons.org/) and
[eol_scons](https://github.com/NCAR/eol_scons) web sites for more installation
options.

On RedHat systems, other dependencies can be installed as system packages
using the `nidas.spec` file and the DNF `builddep` plugin:

```sh
git clone https://github.com/NCAR/nidas
cd nidas
dnf builddep rpm/nidas.spec
```

NIDAS also depends on the [XmlRpc++](https://xmlrpcpp.sourceforge.net/)
library, now very old and not always available as a system package.  It can be
downloaded and installed separately from the version kept in the [NCAR github
xmlrpcpp repo](https://github.com/NCAR/xmlrpcpp), usually by first creating a
package then installing it.

```sh
git clone https://github.com/NCAR/xmlrpcpp.git
cd xmlrpcpp
./build_rpm.sh  # or ./build_dpkg.sh
# then install the resulting package, eg:
dnf install xmlrpc++-0.7-1.fc40.x86_64
```

The advantage to installing the package is that then the RPM build
dependencies are met, necessary to build the NIDAS RPM packages.  See below
for more options for installing `XmlRpc++`.

## Build NIDAS

The `SCons` configuration is controlled by the `src/SContruct` file and then
various `SConscript` files and python tool scripts in `src/tools`.  A native
build is done by default, but a specific target can be named with the `BUILD`
variable, such as to cross-compile for the Raspberry Pi 3 armhf architecture
on a Debian host.  In the `src` directory, run `scons -Q -h` to see brief
usage info, including a list of the available targets.  On most systems, it
should suffice to run the default build with just `scons`

```sh
scons
```

Change the PREFIX by setting it on the command-line or storing it in
`nidas.conf`:

```python
PREFIX="/opt/local/nidas"
```

Run the `install` build target to install, `test` to run the tests:

```sh
scons test
scons install
```

There is a second install target called `target.root` for installing into
locations which usually require root permissions.  Usually that target is only
run when building system packages.

## Developer Hints

### Compiler Warnings

NIDAS has been developed with the help of GCC compiler checks like `-Wall`,
`-Wextra`, and `-Weffc++`, and the goal is to compile cleanly without warnings
whenever possible.

The `SCons` configuration provides a variable called `allow_warnings` which
defaults to `on`, but the CI builds disable it on systems where the build is
expected to compile without warnings.  For development, it's a good idea to
set it off in `nidas.conf`:

```python
allow_warnings="off"
```

This adds the `-Werror` flag to the compiler command-line.  For systems or
containers which do not compile without warnings, leave `allow_warnings` unset
or override the setting on the command line.

### Variant builds

Each target architecture and OS is assigned a different variant output
directory under `src/build`, eg `x86_64_fedora40`.  This allows the same
source tree to be used to build different targets, either from the host or by
mounting the source tree into a container.  However, the tests do not work for
cross-compiles, and they are not well-tested running inside a container.

The layout of the variant build output matches the install layout, so a
specific build can be tested without installing it using `setup_nidas.sh`:

```sh
source build/x86_64_fedora40/bin/setup_nidas.sh
which data_stats
.../src/build/x86_64_fedora40/bin/data_stats
```

Of course, `setup_nidas.sh` is intended primarily for switching between
different install locations, such as between NIDAS versions installed from
different branches, or between the system package location (`/opt/nidas`) and
a local build (eg `/opt/local/nidas`).

See [Variant-Builds.md](doc/Variant-Builds.md) for more information on variant
builds.

### Testing

Running all the tests can take significant time, so it is possible to run a
single test by naming the test directory, like so:

```sh
cd src
scons tests/core
# ...or...
cd src/tests/core
scons -u .
```

Some tests also have their own aliases in the `SConscript` file.

### Kernel modules

Kernel modules are built by default for targets which support them, but they
can be disabled when not needed by setting `LINUX_MODULES=no`.

### Container builds

It is possible to build the same source tree for different targets using
containers.  See the [README.md](scripts/README.md) file in the scripts
directory for more (but incomplete) information.

## Documentation

Doxygen API documentation can be built with `scons`:

```sh
cd src
scons dox
```

Browse to this location to see it: `../doc/doxygen/html/index.html`.

## Install XmlRpc++ directly from source

```sh
git clone https://github.com/NCAR/xmlrpcpp.git
cd xmlrpcpp/xmlrpcpp
sudo make prefix=/usr/local install
sudo ldconfig
```

# NIDAS packages

## Main NIDAS packages

The following RedHat, Fedora, and Debian packages may already be available in
EOL repositories:

* nidas: The main package of NIDAS binaries and shared libraries.
* nidas-libs: Shared libraries needed by NIDAS executables.
* nidas-devel (nidas-dev on Debian): NIDAS header files, symbolic links to
  shared libraries and `nidas.pc` `pkg-config` file, to build software which
  uses NIDAS.

There is support in the source for building a `nidas-doxygen` package, which
installs the Doxygen-generated API documentation into
`/opt/nidas/doxygen/html`, but that package is not currently being built or
distributed through the EOL repository.

## Debian NIDAS packages for specific architectures

* nidas-modules-{target}: Linux kernel modules for NIDAS, used on PC104
  systems, and for USB TwoD acquisition.  Target is vortex or amd64.
* nidas-vortex: Meta-package to install nidas and nidas-modules-vortex on
  Vortex.

## EOL Debian package repository

Debian packages for NIDAS and related dependencies are hosted on the EOL
Debian package repository for the OS releases and architectures currently in
use in EOL.  The repository can be installed like below:

```sh
cd /tmp    
curl https://archive.eol.ucar.edu/software/debian/eol-repo.deb -o eol-repo.deb
sudo dpkg -i eol-repo.deb
```

## EOL RPM repository

There are also RPM package repositories for systems based on Red Hat and
Fedora.  The EPEL name (Extra Packages for Enterprise Linux) is an unfortunate
name collision, and in this case it only includes packages built by EOL.  It
can be installed like so:

```sh
sudo rpm -ihv https://www.eol.ucar.edu/software/rpms/eol-repo-epel.noarch.rpm
```

For Fedora:

```sh
sudo dnf install https://www.eol.ucar.edu/software/rpms/eol-repo-fedora.noarch.rpm
```

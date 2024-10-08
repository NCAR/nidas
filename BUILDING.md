# Building NIDAS

# Wiki Information

The NIDAS Wiki at GitHub (https://github.com/NCAR/nidas/wiki) contains
information on building NIDAS, including how to install developer packages of
necessary external software from the Debian and NCAR/EOL repositories.

# Other Notes on Building

These notes cover much of the same as the Wiki, but include some information people have found useful.

You will need the eol_scons addon in order to manually build.  You can add it via the following (brute force, assumes no other scons addons):

```sh
    mkdir -p ~/.scons/
    pushd ~/.scons
    git clone https://github.com/ncar/eol_scons site_scons
    popd
```

## Documentation
There are 2 scripts that can build the documentation.  One is a bit older and still uses svn (```scripts/make_doxy.sh```)  to create and rsync the data to an internal server, where as ```scripts/create-doxygen-ghpages.sh```  builds the doxygen API doc off HEAD and pushes them to github.io via the gh-pages branch. If you dont care to push data just running ```doxygen doc/doxygen_conf/nidas.doxy``` from the git checkout root should suffice.

## Getting Nidas on Debian (For the truly impatient)

Simply connect to EOL's build server to fetch the build products:

```sh
    curl -O https://archive.eol.ucar.edu/software/debian/eol-repo.deb 
    sudo dpkg -i eol-repo.deb 
    sudo apt update
    sudo apt install nidas
```

You will need the eol_scons addon in order to build properly.  You can add it via the following (brute force, assumes no other scons addons):

```sh
    mkdir -p ~/.scons/
    pushd ~/.scons
    git clone https://github.com/ncar/eol_scons site_scons
    popd
```

## Building Nidas (Manually) on Debian
Standard building preqs are required:
```sh
    sudo apt get install build-essentials scons libxerces-c-dev devscripts debhelper flex subversion git
```

- NIDAS uses a custom version of [xmlrpcpp](http://svn.eol.ucar.edu/svn/eol/imports/xmlrpcpp).  Heaven forbid you are using this lib in another project as it is rather old an sorta stale.  We picked the loser in the XML-RPC wars  :-(.

```sh
    git clone https://github.com/NCAR/xmlrpcpp.git
    pushd xmlrpcpp/xmlrpcpp
    sudo make prefix=/usr/local install
    sudo ldconfig
    popd
```
- After the pre-reqs are built, all that is needed is to run scons to get a working copy
```sh
    git clone https://github.com/NCAR/nidas.git
    pushd nidas/src
    scons
    scons install PREFIX=/usr/local
```

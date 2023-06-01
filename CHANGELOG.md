# Changelog

This file summarizes notable changes to NIDAS source.  The format is based on
[Keep a Changelog], the versions should follow [semantic versioning].

## [2.0] - Unreleased

The 2.0 version of NIDAS will be a merge of the [master], [refactor-builds],
and [buster] branches.  Following are the significant recent changes on each
branch:

## [master] - Branch

## [buster] - Branch

## [refactor-builds] - Branch

This branch includes all the changes done on the branches
`scons-build-single-target`, `conatiner-builds`, and
`move-netcdf-to-nc_server`.

- The [SCons](https://www.scons.org/) setup no longer supports multiple
  variants in the same build.  Instead, scons builds one target OS and
  architecture, specified with the BUILD variable, or else the native host by
  default.  The variant build directory names include an OS name as well as
  architecture.  This allows different OS builds to share the same source
  tree, especially useful when using containers to test builds of source
  changes.  This includes a few key changes to prevent the scons cache and
  configuration from conflicting between variants.  Related to that,
  `Revision.h` is generated in the source and not in a variant dir, since it
  does not depend on the target and needs to be part of source archives. Lots
  of similar scons code has been consolidated into tool files in the
  `nidas/src/tools` subdirectory.  Since there is no longer any confusion
  caused by sharing environments across multiple build targets, tools can be
  used safely and easily to share environment setups with the rest of the
  source tree.

- SCons help is more brief by default by leaving out lots of less used
  variables.  Run `scons -h -Q` to see the short help, `scons --help-all` to see
  all of it.

- The optional `nc_server` dependency has been removed.  The two classes which
  needed it, `NetcdfRPCChannel` and `NetcdfRPCOutput` in the
  `nidas::dynld::isff` namespace, have moved to the
  [nc-server](https://github.com/ncareol/nc-server) source and are now built
  and installed from there.  NIDAS loads them from the `nc_server` lib
  directory (usually under `/opt/nc_server`) as long as that directory is on
  the `LD_LIBRARY_PATH`.

- The `pkg_files` directory in the source tree is now deprecated.  Anything
  that was installed as part of a package script should now be generated and
  installed by the scons instead.  Files are installed from the relevant
  location in the source tree.  For example, module conf files are now in the
  source dirctory for the related driver.

- To allow scons to install everything, there is a second install target,
  `install.root`, for installing files into root locations.  The plain install
  target works as usual, installing everything under the `PREFIX`, typically
  `/opt/nidas`.  The packaging scripts use the `install.root` target and set a
  new scons variable, `INSTALL_ROOT`, which is prepended to every install
  path.  This allows scons to generate files with the correct PREFIX path,
  even when the files will be installed into a temporary location for
  packaging.  Packaging scripts no longer need to regenerate those files with
  the correct install path.

- There is a script `setup_nidas.sh` which gets installed into nidas bin
  path.  It can be used with bash shells to setup `PATH` and `LD_LIBRARY_PATH`
  to run nidas from different install directories.  This makes it
  convenient to install NIDAS builds into alternate locations from the
  NIDAS package, then select which NIDAS install to use.  Thus
  nidas-buildeol is not needed anymore.  the nidas.csh and nidas.sh
  profile.d files have been removed, as they likely were never used, and
  setup_nidas.sh can be used instead, at least for bash.

- NIDAS RPM packages have been simplified.  The nidas-build, nidas-min, and
  nidas-buildeol packages have been removed.

- There is a new script, `cnidas.sh`, which consolidates scripting for
  container builds.  It can build containers for different OS release and
  architecture targets from the available Dockerfiles, but it also automates
  mounting the source tree, install paths, and package output directory into
  the container, so all the container outputs will be available on the host.
  The `dsm_rsync.sh` script can be used to copy a NIDAS install,
  cross-compiled on the local host with a container, onto a DSM under an
  alternate prefix.  That alternate install can be selected and tested in a
  using `setup_nidas.sh`.  See
  [Develop_Pi.md](https://github.com/ncareol/nidas/blob/buster/Develop_Pi.md)
  for an example of using it for Pi3 development.

- Lots of obsolete code and executables have been removed.

## [1.3] - 2019-06-03

This is a version tag on the auto-config branch that was not meant for
release.  All versions relative to v1.3 (`v1.3-316-gfaba0065d` as of
2023-06-01) are on the auto-config branch, and those versions are only built
and installed on the DSM3.  The DSM3 runs NIDAS on a Raspberry Pi3 running the
Buster release of Raspbian OS, so that branch is now known as [buster].  See
the changes on that branch in the section above.

## [1.2] - 2015-10-18

This version tag is on the main development branch.  All NIDAS package
versions since then have been generated relative to that tag, except for the
DSM3 packages which use `v1.3`.  As of 2023-06-21 the generated version is up
to `v1.2-1721-g1ebc8bcd4`.

<!-- Links -->
[keep a changelog]: https://keepachangelog.com/en/1.0.0/
[semantic versioning]: https://semver.org/spec/v2.0.0.html

<!-- Versions -->
[master]: https://github.com/ncareol/nidas
[buster]: https://github.com/ncareol/nidas/tree/buster
[refactor-builds]: https://github.com/ncareol/nidas/tree/refactor-builds
[2.0]: https://github.com/ncareol/nidas/compare/master
[1.3]: https://github.com/ncareol/nidas/compare/master...v1.3
[1.2]: https://github.com/ncareol/nidas/releases/tag/v1.2

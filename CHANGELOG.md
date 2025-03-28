# Changelog

The release version in each header below is a link to that tag on GitHub.  The
NIDAS source code is hosted here:

- [https://github.com/NCAR/nidas](https://github.com/NCAR/nidas)

## 2.0 - Planned merge of master and buster

The 2.0 version of NIDAS will be a merge of the [master] and [buster]
branches.  Currently only the ISFS DSM3 platform runs the [buster] branch.
See the [Changelog](https://github.com/NCAR/nidas/blob/buster/CHANGELOG.md) on
the [buster] branch for the changes on that branch.  Until the merge, all
releases from the master branch start with `v1.2`.  All versions starting with
`v1.3` and `v1.4` are only installed on the DSM3.  The DSM3 runs NIDAS on a
Raspberry Pi3 running the Buster release of Raspbian OS, so that branch is
known as [buster].

## [master] - Unreleased on master branch

- `UTime::format()` specifier `%[n]f` now truncates to the number of digits
  instead of rounding.  This makes it more consistent with the other time
  fields which are never rounded, and times by default show the second which
  contains them, regardless of specified precision.

- `SampleInputStream` logs a warning on the first sample filtered to help
  diagnose unexpected effects from filtering rules.

- Ignore XML attributes like "xmlns" in `DOMable::checkUnhandledAttributes()`,
  since they are added by `XMLConfigWriter` to XML configs written through
  `XMLConfigService`.  This fixes a bug where XML parsing would fail on `dsm`
  clients with an error message like `unknown attribute xmlns`.

## [1.2.5] - 2025-02-03

- The deprecated logging arguments `--logconfig` and `--loglevel` have been
  removed.  The rarely used arguments `--logshow`, `--logfields`, and
  `--logparam` are now omitted from brief help usage (`-h`) and instead only
  included in the full usage (`--help`).

### Changes related to M2HATS

- Add `irgadiagmask` parameter to `CSI_IRGA_Sonic` class to prevent selected
  IRGA diagnostic bits from causing H2O and CO2 to be set to NANs.

- `nidsmerge` now supports `--clip`: clipping expands the time range for
  filename pattern inputs to catch samples within the requested time range but
  which were recorded in preceding or succeeding files.  The time bounds
  arguments `--start` and `--end` are always applied to the output, so no
  samples are ever written outside those bounds.

- Fix a bug where an output file started within 1 second of the next output
  period would include the next period instead of starting a new file.

- `UTime` formats the MIN and MAX times as "MIN" and "MAX", since they are
  indecipherable and indistinguishable when formatted as time strings.  Also,
  when parsing a time string with the default ISO-based formats, a "Z" suffix
  will be accepted and parsed for UTC times.

- The `-i/--samples` sample filter criteria have been expanded to include a
  time range and an input name, and the argument has been added to
  `nidsmerge`.  As an example, this filter specifier excludes samples 2,10
  from a network file stream over 5 days:
  `^2,10,file=isfs_,[2023-07-27,2023-08-02]`.

- Much more metadata from the XML and from processing is now attached to
  variables at run time as attributes.  The netcdf output uses them to set the
  netcdf variable attributes.

- NIDAS XML configurations can now associate a different site with a sensor
  than the DSM which acquires that sensor.  If the site has been defined, the
  name of that site can be added as an attribute to the sensor:

  ```xml
  <site name="t6" class="isff.GroundStation" suffix=".${SITE}"/>
  <site name="t5" class="isff.GroundStation" suffix=".${SITE}">
    <dsm IDREF="COREV" name="${SITE}" id="5">
      ...
      <serialSensor IDREF="CSAT3B" devicename="/dev/ttyDSM5" height="4m" id="60" site="t6"/>
    </dsm>
  </site>
  ```

  The sensor's variable names then contain the right site suffix as well as
  important metadata like `height`.

- The sensor suffix can override the site suffix if prepended with `!`.  This
  can be used, for example, to omit the site name for DSM-specific GPS
  variables even when the site element sets `suffix=".${SITE}"`.

  ```xml
  <serialSensor devicename="usock::32947" suffix="!.${DSM}"/>
  ```

- `svnStatus()` has been removed.  It was only used by the NetCDF RPC outputs,
  and it has been a while since the project configurations were under
  subversion control.  See notes in `nc-server` about using the
  `ISFS_CONFIG_VERSION` environment variable instead.

## [1.2.4] - 2025-01-28

- Fix bug in `StatisticsCruncher`: an empty data period would be written when
  gap filling was disabled but a start time had been set.  Now if gap fills
  are disabled, the output starts at the first sample, even when the first
  sample is days after the start time.

### threading fixes

- The Logger macros now use thread_local for the LogContext instance, and the
  Logger locking has been modified, to fix errors detected by concurrency
  checks.  Other missing synchronization points for critical sections have
  been fixed.  As always with synchronization changes, be aware of unintended
  consequences.

- Related to fixing concurrency errors, calcStatistics() is now only called on
  opened sensors, where before it was called on all sensors, including the
  ones which were not open yet.  Presumably that will not interfere with
  expected diagnostics.

### enhanced version info

- The standard `--version` argument now also shows the compiler version and the
  CPP definitions used to build NIDAS.  This can be helpful to show what
  conditional components were built into a NIDAS application.

### data_stats and related improvements

- `data_stats` JSON output includes problems detected in the statistics, so
  far sample rate mismatches, missing values, and no samples.  The schema is
  still likely to change.
- The `-i` argument to programs like `data_dump` and `data_stats` now accepts
  '.' to select the DSM ID in the first sample in the data, and '/' to accept
  all IDs.  So this works to report on all of the samples from a single DSM:
  `data_stats -i . -a sock:localhost`.
- `data_stats` can generate periodic reports on archive data using the `-P
  period` and `-n` arguments, and multiple json reports can be written by
  adding time specifiers to the `--json` file path.

## [1.2.3] - 2024-03-02

- Removed TWODS detection.
- Statsproc now supports --clip argument to clip netcdf output like prep.
- UTime API improvements for unset times.

## [1.2.2] - 2023-12-13

Variable substitution is now done on sensor element attributes height, depth,
and suffix when they are applied to a sensor object.  This accommodates the
SOS config which uses environment  variables to specify changes in sonic
heights during the project.

`NDEBUG` is never defined by default for any target, so any `assert()` calls
in the code will now be compiled everywhere, where before they were being
omitted everywhere.

The SCons variant builds are now done with `duplicate` disabled.  Kernel
modules are built in the variant directories by using explicit symbolic links
to the module source files.

Fixed `fork()` and `exec()` problems in the _UDPArinc_ and _UDPiPM_ sensors.

Cleaned up compiler warnings.

## [1.2.1] - 2023-10-01

### bug fixes for min/max and variance statistics

The min/max statistics could be reported as +- 1e37 even when there were no
values during a time period.  Similarly, the mean in the variance statistics
could be 0 even when there were no values during the statistics period.

### udev rules restored, nidas-daq package removed

The `nidas-daq` package is no longer built.  Instead, the `dsm.init` script is
now installed into the NIDAS bin directory with other scripts, where it can be
copied or linked on systems which still use it.  The udev rules file no longer
changes permissions on all serial devices.  It only sets permissions on
devices associated with NIDAS Linux modules, so the rules file is only
installed as part of a nidas modules package, such as `nidas-modules-vortex`.
RPM and Debian packages are more consistent in when and how the rules file and
the `dsm.init` script are packaged.

### Libraries install into lib

The nidas install layout now uses just `lib` for the library directory name,
rather than an arch-specific path like `lib64` or `lib/i386-linux-gnu`.  The
libraries in `lib` have the same architecture as the executables in `bin`.
There are no NIDAS targets which require multiple architectures installed
under the same `/opt/nidas` install path.

For software that builds against nidas using the `nidas.py` tool from
[eol_scons](https://github.com/NCAR/eol_scons), that tool already checks for a
plain `lib` subdirectory if the arch-specific path is not found.  Likewise,
`pkg-config` will return the new library path for linking to NIDAS libraries.
However, if `PKG_CONFIG_PATH` is being used to select the `nidas.pc` settings
for a specific NIDAS installation, then it probably needs to be adjusted.  For
example, it must be set like below to use the `pkg-config` settings for a NIDAS
install in `/opt/local/nidas`:

```plain
export PKG_CONFIG_PATH=/opt/local/nidas/lib/pkgconfig
```

## refactor-builds - Branch

This branch includes all the changes done on the branches
`scons-build-single-target`, `container-builds`, `simplify-nidas-rpms`, and
`move-netcdf-to-nc_server`.

- The [SCons](https://www.scons.org/) setup now builds a single target OS and
  architecture, specified with the BUILD variable, or else the native host by
  default.  The variant build directory names include an OS name as well as
  architecture so different OS builds can share the same source tree,
  especially useful when testing builds with containers.  There are a few key
  changes related to this to prevent the scons cache and auto-configuration
  from conflicting between variants.  Likewise, `Revision.h` is generated in
  the source and not in a variant dir, since it does not depend on the target
  and needs to be part of source archives. Lots of similar scons code has been
  consolidated into tool files in the `nidas/src/tools` subdirectory.  Since
  there is no longer any confusion caused by sharing environments across
  multiple build targets, tools can be used safely and easily to share
  environment setups with the rest of the source tree.  There is now just a
  single `PREFIX` setting, `ARCHPREFIX` has been removed.

- SCons help is more brief by default by leaving out lots of less used
  variables.  Run `scons -h -Q` to see the short help, `scons --help-all` to see
  all of it.

- The optional `nc_server` dependency has been removed.  The two classes which
  needed it, `NetcdfRPCChannel` and `NetcdfRPCOutput` in the
  `nidas::dynld::isff` namespace, have moved to the
  [nc-server](https://github.com/NCAR/nc-server) source and are now built
  and installed from there.  NIDAS loads them from the `nc_server` lib
  directory (usually under `/opt/nc_server`) as long as that directory is on
  the `LD_LIBRARY_PATH`.

- The `pkg_files` directory in the source tree is now deprecated.  Anything
  that was installed as part of a package script should now be generated and
  installed by scons instead.  Files are installed from the relevant location
  in the source tree.  For example, module conf files are now in the module
  source directory.

- To allow scons to install everything needed by system packages, there is a
  second install target, `install.root`, for installing files into root
  locations.  The plain install target works as usual, installing everything
  under `PREFIX`, typically `/opt/nidas`.  The packaging scripts use the
  `install.root` target and set a new scons variable, `INSTALL_ROOT`, which is
  prepended to every install path.  This allows scons to generate files with
  the correct `PREFIX` path, even when the files will be installed into a
  temporary location for packaging.  Packaging scripts no longer need to
  regenerate those files with the correct install path.

- There is a script `setup_nidas.sh` which gets installed into the NIDAS bin
  directory.  It can be used with bash shells to setup `PATH` and
  `LD_LIBRARY_PATH` to run nidas from different install directories.  This
  makes it convenient to install NIDAS builds into alternate locations, then
  select which NIDAS install to use in the shell.  Thus `nidas-buildeol` is
  not needed anymore.  The `nidas.csh` and `nidas.sh` files in `profile.d`
  have been removed, as they likely were never used, and `setup_nidas.sh` can
  be used instead, at least for bash.

- The layout of the variant build directories has changed to match the install
  layout.  Programs are built into `variant_dir/bin` and libraries into
  `variant_dir/lib`.  Since `setup_nidas.sh` is installed there also, it can
  be used to setup a shell to use the NIDAS binaries in that build tree,
  without installing them first.  For example:

  ```plain
  source build_x86_64_fedora37/bin/setup_nidas.sh
  which dsm
  ~/code/nidas/src/build_x86_64_fedora37/bin/dsm
  ```

- NIDAS RPM packages have been simplified.  The `nidas-build`, `nidas-min`,
  and `nidas-buildeol` packages have been removed.

- There is a new script, `cnidas.sh`, which consolidates scripting for
  container builds.  It can build containers for different OS release and
  architecture targets from the available Dockerfiles, but it also automates
  mounting the source tree, install paths, and package output directory into
  the container, so all the container outputs will be available on the host.
  The `dsm_rsync.sh` script can be used to copy a NIDAS install,
  cross-compiled on the local host with a container, onto a DSM under an
  alternate prefix.  That alternate install can be selected and tested using
  `setup_nidas.sh`.  See
  [Develop_Pi.md](https://github.com/NCAR/nidas/blob/buster/Develop_Pi.md)
  for an example of using it for Pi3 development.

- Lots of obsolete code, executables, and scripts have been removed.

<!-- Versions -->
[master]: https://github.com/NCAR/nidas
[buster]: https://github.com/NCAR/nidas/tree/buster
[1.2.5]: https://github.com/NCAR/nidas/releases/tag/v1.2.5
[1.2.4]: https://github.com/NCAR/nidas/releases/tag/v1.2.4
[1.2.3]: https://github.com/NCAR/nidas/releases/tag/v1.2.3
[1.2.2]: https://github.com/NCAR/nidas/releases/tag/v1.2.2
[1.2.1]: https://github.com/NCAR/nidas/releases/tag/v1.2.1

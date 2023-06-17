# Changelog

This file summarizes notable changes to NIDAS source.  The format is based on
[Keep a Changelog], the versions should follow [semantic versioning].

## [buster] - Branch

This is the development and production branch for the DSM3, a DSM based on
Raspberry Pi 3 and a custom serial interface card with software control for
sensor power and serial transciever modes.  Following are the main changes on
the buster branch.

### Hardware Configuration

DSM3 hardware support is built into NIDAS, especially autoconfiguration of the
serial transceiver mode according to the sensor type: RS232, RS485 full
duplex, or RS485 half-duplex.  This also includes the sensor power relays, so
`dsm` can turn on power to a port when a sensor is opened.

### Sensor Configuration

Select ISFS sensors can be queried and configured automatically, including the
NCAR TRH, Gill 2D sonic anemometers, PTB210, PTB220, and Wisard Motes.  Sensor
parameters can be specified in the XML, then those parameter settings can be
applied to the sensor.  The sensor classes can also query for the current
sensor settings and related metadata like serial number, so those metadata can
be logged.  Eventually the metadata will be captured and preserved at regular
intervals.

### Sensor Search and Detection

Sensors can now specify a set of standard serial port configurations,
including hardware transceiver modes on DSM3 ports, and NIDAS will search
those port configurations to find the one in use by the attached sensor.  Once
the sensor serial comms are detected, some sensors can then be switched
automatically to use the serial communication settings set in the XML
configuration.

This does not yet include identifying the kind of sensor attached, NIDAS only
looks for a response according to the single sensor specified for the port.

## [1.4]

The hardware API was refactored to simplify it, consolidate it into one
header, hide the implementation so it can be swapped or compiled out entirely,
and fix a memory leak.

The `pio` utility has been simplified and has subsumed the `dsm_port_config`
utility.  It now reports hardware status for all available FTDI itnerfaces:
outputs, inputs, and serial modes.  Any of the settings can be changed with
simple command-line arguments.  See `pio -h` for usage.

## [1.3.253]

This is the version deployed for the SOS field project, released with version
string `v1.3-253`.

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
[buster]: https://github.com/ncareol/nidas/tree/buster
[1.4]: https://github.com/ncareol/nidas/compare/v1.3.253...v1.4
[1.3.253]: https://github.com/ncareol/nidas/compare/v1.3...v1.3.253
[1.3]: https://github.com/ncareol/nidas/compare/master...v1.3
[1.2]: https://github.com/ncareol/nidas/releases/tag/v1.2

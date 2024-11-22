# NCAR Insitu Data Acqusition Software (NIDAS)

## Introduction

[Introduction.md](doc/Introduction.md) is an overview of NIDAS written by the
original author, Gordon Maclean.

## Changelog

Recent changes are noted in the [Changelog](CHANGELOG.md).

## Building

See [BUILDING.md](BUILDING.md) for notes on building and developing NIDAS.

## Packages

See [PACKAGES.md](PACKAGES.md) for installing NIDAS from RPM or Debian
packages.

## API Documentation

The EOL web site hosts the [NIDAS API
documentation](https://www.eol.ucar.edu/software/nidas/doxygen/html) generated
with [Doxygen](https://doxygen.nl/), but it is not always updated.

## Time tags

See [timetags.md](doc/timetags.md) for a disucssion of time tags in NIDAS.

## Future documentation

NIDAS needs more documentation and a clear plan for hosting it.  Currently
most (and certainly the most current) documentation is in markdown files
within the source repository, some of it having been migrated from the wiki
and google docs.

One possibility is to use a second github repository to host a static web
site, generated from the NIDAS source, so it includes both the markdown pages
and the Doxygen API documentation.  Keep the generated documentation and
rendering artifacts out of the source repository, while the information can
still reside with the source.

In particular, NIDAS is lacking documentation on the XML configuration, since
that is a fundamental part of the data acquisition, but it has many esoteric
settings and variations.

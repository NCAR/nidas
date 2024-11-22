# Variant Builds

The NIDAS SCons configuration always writes build output into a _variant_
directory.  The name of the variant is in the form `{architecture}_{os}`, and
the variant build directory is relative to the top of the SCons build,
`build/{variant}/`.  Even default builds for the current host architecture and
OS will have a variant build directory named accordingly.  This allows the
same source tree to be built for different targets, whether using containers
or networked filesystems.  Also, libraries and executables are explicitly
written into the variant build directory using the install layout, so
executables are written to `bin/`, libraries to `lib/`.  Other build artifacts
are written into their normal variant output location, named after their
source subdirectory.

This layout makes it easier to test against the source build because
environment variables like `PATH` and `LD_LIBRARY_PATH` can be built the same
way from a single root path, `build/{variant}`.  The variants could be given
other names, rather just named after architecture and OS, such as optimization
and debug settings or git branch names, except that is not yet supported.  The
goal is to make builds more independent and make it easier to switch between
branches and build settings without losing other builds.

The builds still have an install step, for moving files into the right system
prefix and performing other system-level installation, since not all files are
installed into the variant path.  In particular, for cross-builds, it is still
safer to install to some install prefix to generate a complete installation,
and _then_ copy the installation tree to the target.

To summarize the current software build configuration, the goals are these:

Keep related files near each other in the source tree, and keep the logic for
where those files get installed in one place.  Let the build system contain
the install logic, so the packaging just needs to divide up the install tree
into the right packages.

The build system can install into a live system same as installing into a
package root.  It's possible to install all the right dependencies and build
artifacts on a development system from the build system both to test the
installation logic and to test software changes, without building intermediate
packages.

The logic is simpler for running test programs inside the source tree, and
there is no need to add several different directories to the
`LD_LIBRARY_PATH`.  Running tests against the source and against a system
installation is simply a matter of changing the path to the install root.

Eventually it would be nice to have a way to build test programs and run them
on different targets.  The build system should be able to cross-compile test
progams same as user programs, install the test programs and test artifacts
into an intermediate directory, such as under the variant directory, copy them
to the target, and finally then run them on the target.

Keep the build configuration simpler by only building one target (one variant)
at a time.  There was a time when multiple targets could be built in the same
`scons` run, but it proved difficult to keep variant configurations from
interfering with each other, and it was rarely used.

## Future improvements

SCons does not really support out-of-source builds, so an alternative would be
to provide the variant name as a build variable.  The value could still
default to the architecture and OS, or it could be overridden:

```sh
scons VARIANT=armhf-pi3-debug
```

It would also be convenient to cache the current variant, so it becomes the
default on subsequent builds, rather than requiring the variant setting in the
`scons` command every time.

Likewise, it would be convenient for `nidas.conf` to be specific to each
variant, to support a use case like below:

```sh
mkdir build/armhf-pi3  
vi build/armhf-pi3/nidas.conf (set TARGET=armhf_rpi3, PREFIX=...)  
scons VARIANT=armhf-pi3
```

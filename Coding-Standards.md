# NIDAS Coding Standards

This is a short list of coding standards for NIDAS development.

## C++ Standard: C++11

NIDAS compiles to the C++11 standard (`-std=c++11`) so it builds on all the
current targets.  The `buster` branch for the Raspberry Pi DSM cannot build
with C++17, because the removal of Dynamic Exception Specifiers has not yet
been merged into that branch.  At least C++17 should work for the master
branch, but for now the standard is consistent between branches.  It would be
good to evaluate going to C++17 or higher once the branches are merged. See
this issue for more background: [move to c++17 and start a coding standards
doc](https://github.com/NCAR/nidas/issues/29).

## Other rules

These are issues that have been or are being fixed in the NIDAS source, so
they are listed here to prevent re-introducing them.

* Do not put `using namespace` declarations in header files at namespace
  scope.  It pollutes the namespace of any source file which needs to include
  that header and resolve names from different namespaces.

* Include only the headers for the definitions needed in a source module. It
  can confuse dependencies and slow down compiles if several headers and
  copied and pasted to new source modules, even if not all of them are needed.

* If a header needs to include another header just to define the
  implementation in the header, then put the implementation in the source
  module instead and include the other header there.  In other words, do not
  expose implementation in the header or add include dependencies unless
  really necessary.  Inline definitions are rarely a good enough reason.

* Compile with warnings enabled and keep the build free of warnings.  NIDAS
  builds with `-Wall` and `-Wextra` by default, and `-Werror` is added when
  `allow_warnings=off` is set for the SCons command.  The Jenkins builds use
  `allow_warnings=off`, so they will fail if code changes cause a compile
  warning.  (Originally started with this issue: [compile without
  warnings](https://github.com/NCAR/nidas/issues/14).)

* Do not declare or define copy constructors or assignment operators if they
  are not needed.  If they do not make sense or the defaults would be invalid,
  then declare them with `= delete`.

* Use `override` for virtual method overrides.  NIDAS makes heavy use of
  virtual methods, abstract base classes, and multiple inheritance, and it can
  be difficult to track which methods may be called dynamically, and which
  methods are overloading instead of overriding because of slightly different
  signatures.

## Other guidelines

NIDAS logging is designed to be efficient even when log messages are not
enabled.  In general, log messages are more useful if they are always compiled
into the code rather than conditionally compiled, so they can be enabled
wherever the code is deployed and exhibiting problems.  Messages that might be
excessive even for debugging can use `VLOG` (V for _verbose_ or
_deVelopment_).  See [Logger.h](src/nidas/util/Logger.h).

Dynamic exception specifiers are deprecated and should not be used, and the
value of declaring functions as `throw()` or `noexcept(true)` so far is not
definitive.  See [what to do about throw() and
noexcept](https://github.com/NCAR/nidas/issues/5).

As of issue [what to do about assert() and
NDEBUG](https://github.com/NCAR/nidas/issues/13), `assert()` calls are never
compiled out.  They can be used if the code needs to abort on an unexpected
condition, but they will always run, and complicated asserts could impact
performance.

## Coding Styles

NIDAS coding styles have varied over the years and across developers. There is
a start at a [clang-format](.clang-format) configuration file, tested to
introduce as few changes as possible to `data_dump.cc`.  As code is developed,
it might be prudent to also reformat it to a common coding style. Sorting
includes seems to be a common practice and could be worth adopting.

If it were up to Gary, the tabs would be replaced gradually with spaces.

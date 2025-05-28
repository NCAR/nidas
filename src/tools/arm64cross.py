# Copyright (c) 2007-present, NSF NCAR, UCAR
#
# This source code is licensed under the MIT license found in the LICENSE
# file in the root directory of this source tree.
"""
scons arm64cross tool

Customize an environment to use the GCC ARM cross-compiler tools for aarch64
"""

import os
import eol_scons.utils

prefix = 'aarch64-linux-gnu'


def generate(env, **kw):
    """
    Add construction variables for C compilers to an Environment.
    """

    env.Replace(AR=prefix + '-ar')
    env.Replace(AS=prefix + '-as')
    env.Replace(CC=prefix + '-gcc')
    env.Replace(LD=prefix + '-ld')
    env.Replace(CXX=prefix + '-g++')
    env.Replace(LINK=prefix + '-g++')
    env.Replace(RANLIB=prefix + '-ranlib')
    env.Replace(
        KMAKE='make KERNELDIR=$KERNELDIR KCFLAGS="$KCFLAGS" ARCH=arm CROSS_COMPILE=' + prefix + '-')

    # if a multiarch pkgconfig path exists, add it to PKG_CONFIG_PATH
    pkgpath = os.path.join("/usr", "lib", prefix, "pkgconfig")
    if os.path.isdir(pkgpath):
        env.PrependENVPath("PKG_CONFIG_PATH", pkgpath)
        print("arm64cross: PKG_CONFIG_PATH=%s" %
              (env['ENV']['PKG_CONFIG_PATH']))

    if not exists(env):
        print("*** %s not found on path: %s" %
              (env['CC'], env['ENV']['PATH']))
        return

    print("arm64cross: found %s and %s" %
          (env.WhereIs(env['CC']), env.WhereIs(env['CXX'])))

    cxxrev = eol_scons.utils.get_cxxversion(env)
    if cxxrev != None:
        env.Replace(CXXVERSION=cxxrev)


def exists(env):
    return bool(env.Detect(prefix + '-gcc')) and bool(env.Detect(prefix + '-g++'))

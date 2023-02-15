"""
Create the env for cross-building to the xScale ARM processor (big-endian) on
the arcom Vulcan.
"""
import re
from SCons.Script import Environment


def generate(env: Environment):
    env.Require(['armbecross'])
    env['ARCHLIBDIR'] = 'lib'
    env['ARCHPREFIX'] = '$PREFIX/armbe'

    #############################################################################
    # These comments apply to the arm-linux-ld and armbe-linux-ld that we are using,
    # version 2.16.1.  Versions of ld for x86 in EL5 and Fedora are able to find
    # libnidas with a -Ldir when linking against libnidas_dynld.so, and do not
    # need the -rpath-link option.
    #
    # When ld is resolving these internal dependencies between
    # shared libraries it does not search the -Ldir (LIBPATH) directories.
    # Instead it searches these paths: -rpath-link, -rpath, $LD_RUN_PATH,
    # $LD_LIBRARY_PATH, $DT_RUN_PATH,$DT_RPATH,/lib,/usr/lib and ld.so.conf.
    # See man ld, under -rpath-link.
    #
    # Hence these -rpath-link options to search the build directories
    # at link time.  Note that the run-time linker does not use
    # -rpath-link, it uses -rpath
    #
    # Here's an example of the error that occurs when linking an executable
    # program from nidas/apps:
    # /opt/arcom/lib/gcc/arm-linux/3.4.4/../../../../arm-linux/bin/ld:
    # warning: libnidas.so, needed by build_arm/build_dynld/libnidas_dynld.so,
    # not found (try using -rpath or -rpath-link)
    #############################################################################

    env.Append(LINKFLAGS=
       ['-Xlinker', '-rpath-link=' + env.Dir('#/util').path + ':' +
        env.Dir('#/core').path])

    env.AppendUnique(CPPDEFINES='NIDAS_EMBEDDED')
    # clock_gettime is in librt on vulcans
    env.AppendUnique(LIBS=['rt'])

    if re.search("^3", env['CXXVERSION']):
        env.AppendUnique(CXXFLAGS=['-Wnon-virtual-dtor','-Wno-uninitialized'])


def exists(env):
    return True

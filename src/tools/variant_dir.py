"""
Derive a variant dir based on ARCH, MACH, and host OS release.
"""
from SCons.Script import Environment

import system_info as si


def get_variant_dir(env):
    """
    Derive the variant build dir for the arch and mach being built.  For
    native builds, generate a path which includes the host architecture and
    OS, to differentiate "native" builds of the same tree but from different
    hosts or containers.
    """
    arch = env.get('ARCH')
    mach = env.get('MACH')
    # in this case, "machine" is really more like architecture.
    if not arch or arch == 'host':
        arch = si.get_arch() or 'host'
    # for host builds, use the os release in place of machine type.
    if not mach or mach == 'host':
        mach = si.get_os() or 'host'
    vdir = "build/%s_%s" % (arch, mach)
    return vdir


def generate(env: Environment):
    env['VARIANT_DIR'] = '#/' + get_variant_dir(env)
    # stash the variant identifiers for use elsewhere, qualified by the name
    # of this tool.
    env['VARIANT_OS'] = si.get_os()
    env['VARIANT_ARCH'] = si.get_arch()
    env.AddMethod(si.get_debian_multiarch, "GetDebianMultiarch")
    # add it to the environment for scripts to use, especially test scripts.
    env['ENV']['VARIANT_DIR'] = env.Dir('$VARIANT_DIR').abspath
    # any environment which uses the variant dir can also get the runtime
    # settings to run against that variant
    env.PrependENVPath('PATH', env.subst('${VARIANT_DIR}/bin'))
    env.PrependENVPath('LD_LIBRARY_PATH', env.subst('${VARIANT_DIR}/lib'))
    # Likewise these are required to build against the variant
    env.AppendUnique(CPPPATH=['$VARIANT_DIR/include'])
    env.AppendUnique(LIBPATH=['$VARIANT_DIR/lib'])


def exists(env):
    return True

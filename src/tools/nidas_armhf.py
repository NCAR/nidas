"""
Configure an Environment for an armhf cross-compile using GCC cross-compiler
on Debian.
"""
from SCons.Script import Environment


def generate(env: Environment):
    env.Require(['armhfcross'])
    if 'ARCHLIBDIR' not in env:
        env['ARCHLIBDIR'] = 'lib/arm-linux-gnueabihf'
    env.AppendUnique(CCFLAGS = ['-g', '-fpie',
        '-Wformat', '-Werror=format-security',
        '-D_FORTIFY_SOURCE=2'])
    env.AppendUnique(CXXFLAGS = ['-Wextra','-Weffc++'])
    env.AppendUnique(LINKFLAGS = ['-pie', '-Wl,-z,relro', '-Wl,-z,now'])


def exists(env):
    return True

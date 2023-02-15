"""
Cross-compile for armel.
"""
from SCons.Script import Environment


def generate(env: Environment):
    env.Require(['armelcross'])
    if 'ARCHLIBDIR' not in env:
        env['ARCHLIBDIR'] = 'lib/arm-linux-gnueabi'
    env.AppendUnique(CCFLAGS = ['-g', '-fpie',
        '-Wformat', '-Werror=format-security',
        '-D_FORTIFY_SOURCE=2', '-std=c++11'])
    env.AppendUnique(CXXFLAGS = ['-Wextra','-Weffc++'])
    env.AppendUnique(LINKFLAGS = ['-pie', '-Wl,-z,relro', '-Wl,-z,now'])
    env.AppendUnique(CPPDEFINES = 'NIDAS_EMBEDDED')


def exists(env):
    return True

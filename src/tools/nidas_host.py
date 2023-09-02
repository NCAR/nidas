"""
Configure a native nidas build.
"""
from SCons.Script import Environment


def generate(env: Environment):
    # this assumes that native builds are only done on x86_64 arch
    if not env.get('ARCHLIBDIR'):
        env['ARCHLIBDIR'] = 'lib64'

    env.AppendUnique(CCFLAGS=['-g', '-fpic', '-Wformat',
                              '-Werror=format-security',
                              '-D_FORTIFY_SOURCE=2'])
    env.AppendUnique(CXXFLAGS=['-Wextra', '-Weffc++'])
    env.AppendUnique(CXXFLAGS=['-std=c++11'])


def exists(env):
    return True

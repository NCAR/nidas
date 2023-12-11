"""
Configure a native nidas build.
"""
from SCons.Script import Environment


def generate(env: Environment):
    # this assumes that native builds are only done on x86_64 arch
    if not env.get('ARCHLIBDIR'):
        env['ARCHLIBDIR'] = 'lib64'
    env.AppendUnique(CCFLAGS=['-fpic'])


def exists(env):
    return True

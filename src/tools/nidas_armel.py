"""
Cross-compile for armel.
"""
from SCons.Script import Environment


def generate(env: Environment):
    env.Require(['armelcross'])
    if 'ARCHLIBDIR' not in env:
        env['ARCHLIBDIR'] = 'lib/arm-linux-gnueabi'
    env.AppendUnique(CCFLAGS=['-fpie'])
    env.AppendUnique(LINKFLAGS=['-pie', '-Wl,-z,relro', '-Wl,-z,now'])


def exists(env):
    return True

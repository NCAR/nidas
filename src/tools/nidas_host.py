"""
Configure a native nidas build.
"""
from SCons.Script import Environment


def generate(env: Environment):
    env.AppendUnique(CCFLAGS=['-fpic'])


def exists(env):
    return True

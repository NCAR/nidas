"""
Create the env for cross-building to the xScale ARM processor (little-endian)
on the arcom Viper and Titan.
"""

import re
from SCons.Script import Environment


def generate(env: Environment):
    env.Require(['armcross'])
    env['ARCHLIBDIR'] = 'lib'
    # g++ version 3.4.4 often gives false positives when
    # trying to detect uninitialized variables.
    if re.search("^3", env['CXXVERSION']):
        env.AppendUnique(CXXFLAGS=['-Wnon-virtual-dtor', '-Wno-uninitialized'])


def exists(env):
    return True

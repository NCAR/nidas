"""scons arm tool

Customize an environment to use the GCC ARM cross-compiler tools.
"""

import os
import kmake

def generate(env):
    """
    Add Builders and construction variables for C compilers to an Environment.
    """

    # Just put the common locations of arm tools on the path too, in case
    # they are not already there.
    env.AppendENVPath('PATH', '/opt/arcom/bin')
    
    env.Replace(AR	= 'armbe-linux-ar')
    env.Replace(AS	= 'armbe-linux-as')
    env.Replace(CC	= 'armbe-linux-gcc')
    env.Replace(LD	= 'armbe-linux-ld')
    env.Replace(CXX	= 'armbe-linux-g++')
    env.Replace(LINK	= 'armbe-linux-g++')
    env.Replace(RANLIB	= 'armbe-linux-ranlib')

    k = env.Builder(action=kmake.Kmake)
    env.Append(BUILDERS = {'Kmake':k})


def exists(env):
    return env.Detect(['armbe-linux-gcc'])

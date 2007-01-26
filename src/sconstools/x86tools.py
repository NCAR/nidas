"""scons x86 tool

Add Builder for linux kernel modules
"""

import os
import kmake

def generate(env):
    """
    Add Builders and construction variables for C compilers to an Environment.
    """

    k = env.Builder(action=kmake.Kmake)
    env.Append(BUILDERS = {'Kmake':k})

def exists(env):
    return true

"""scons x86 tool

Add Builder for linux kernel modules
"""

import os
import kmake
import SCons.Tool

def generate(env):
    """
    Add Builders and construction variables for C compilers to an Environment.
    """

    k = env.Builder(action=kmake.Kmake,
        source_scanner=SCons.Tool.SourceFileScanner)
    env.Append(BUILDERS = {'Kmake':k})

def exists(env):
    return true

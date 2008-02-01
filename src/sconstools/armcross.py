"""scons arm tool

Customize an environment to use the GCC ARM cross-compiler tools.
"""

import os
import kmake
import SCons.Tool

def generate(env):
    """
    Add Builders and construction variables for C compilers to an Environment.
    """

    # Append /opt/arcom/bin to env['ENV']['PATH'],
    # so that it is fallback if arm-linux-gcc is
    # not otherwise found in the path.
    # But scons is too smart. If you append /opt/arcom/bin
    # to env['ENV']['PATH'], scons will remove any earlier
    # occurances of /opt/arcom/bin in the PATH, and you may
    # get your second choice for arm-linux-gcc.
    # So, we only append /opt/arcom/bin if "which arm-linux-gcc"
    # fails.

    if env.Execute("which arm-linux-gcc") or env.Execute("which arm-linux-g++"):
        env.AppendENVPath('PATH', '/opt/arcom/bin')
        print "PATH=" + env['ENV']['PATH'];

    env.Replace(AR	= 'arm-linux-ar')
    env.Replace(AS	= 'arm-linux-as')
    env.Replace(CC	= 'arm-linux-gcc')
    env.Replace(LD	= 'arm-linux-ld')
    env.Replace(CXX	= 'arm-linux-g++')
    env.Replace(LINK	= 'arm-linux-g++')
    env.Replace(RANLIB	= 'arm-linux-ranlib')
    env.Replace(LEX	= 'arm-linux-flex')

    k = env.Builder(action=kmake.Kmake,
        source_scanner=SCons.Tool.SourceFileScanner)
    env.Append(BUILDERS = {'Kmake':k})


def exists(env):
    return env.Detect(['arm-linux-gcc'])


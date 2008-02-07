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
    # so that it is the fallback if armbe-linux-gcc is
    # not otherwise found in the path.
    # But scons is too smart. If you append /opt/arcom/bin
    # to env['ENV']['PATH'], scons will remove any earlier
    # occurances of /opt/arcom/bin in the PATH, and you may
    # get your second choice for armbe-linux-gcc.
    # So, we only append /opt/arcom/bin if "which armbe-linux-gcc"
    # fails.

    if env.Execute("which armbe-linux-gcc") or env.Execute("which armbe-linux-g++"):
        env.AppendENVPath('PATH', '/opt/arcom/bin')
        print "PATH=" + env['ENV']['PATH'];

    env.Replace(AR	= 'armbe-linux-ar')
    env.Replace(AS	= 'armbe-linux-as')
    env.Replace(CC	= 'armbe-linux-gcc')
    env.Replace(LD	= 'armbe-linux-ld')
    env.Replace(CXX	= 'armbe-linux-g++')
    env.Replace(LINK	= 'armbe-linux-g++')
    env.Replace(RANLIB	= 'armbe-linux-ranlib')
    env.Replace(LEX	= 'armbe-linux-flex')

    k = env.Builder(action=kmake.Kmake,
        source_scanner=SCons.Tool.SourceFileScanner)
    env.Append(BUILDERS = {'Kmake':k})


def exists(env):
    return env.Detect(['armbe-linux-gcc'])

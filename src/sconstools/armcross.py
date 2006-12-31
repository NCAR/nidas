"""scons arm tool

Customize an environment to use the GCC ARM cross-compiler tools.
"""

import os

def generate(env):
    """
    Add Builders and construction variables for C compilers to an Environment.
    """

    # Just put the common locations of arm tools on the path too, in case
    # they are not already there.
    env.AppendENVPath('PATH', '/opt/arm_tools/bin')
    
#     # Look for the compiler to get the path.
#     gcc = env.WhereIs('arm-linux-gcc')
#     if (gcc):
#         env['ARM_CROSS_BINDIR'] = os.path.dirname(gcc)
    env.Replace(AR	= 'arm-linux-ar')
    env.Replace(AS	= 'arm-linux-as')
    env.Replace(CC	= 'arm-linux-gcc')
    env.Replace(CXX	= 'arm-linux-g++')
    env.Replace(LINK	= 'arm-linux-g++')
    env.Replace(RANLIB	= 'arm-linux-ranlib')
    env.Replace(LEX	= 'arm-linux-flex')

def exists(env):
    return env.Detect(['arm-linux-gcc'])


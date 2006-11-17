"""scons arm tool

Customize an environment to use the GCC ARM cross-compiler tools.
"""

def generate(env):
    """
    Add Builders and construction variables for C compilers to an Environment.
    """
    env.Replace(AR	= 'arm-linux-ar')
    env.Replace(AS	= 'arm-linux-as')
    env.Replace(CC	= 'arm-linux-gcc')
    env.Replace(CXX	= 'arm-linux-g++')
    env.Replace(LINK	= 'arm-linux-g++')
    env.Replace(RANLIB	= 'arm-linux-ranlib')
    env.Replace(LEX	= 'arm-linux-flex')

def exists(env):
    return env.Detect(['arm-linux-gcc'])


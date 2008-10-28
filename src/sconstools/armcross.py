"""scons arm tool

Customize an environment to use the GCC ARM cross-compiler tools.
"""

import os
import re
import subprocess
import kmake
import SCons.Tool

def generate(env):
    """
    Add Builders and construction variables for C compilers to an Environment.
    """

    # temporary hack.  RTLinux vipers have GLIBC_2.3.1
    # and something in nibnidas needs GLIBC_2.3.2
    # so build with old tools as long as we have RTLinux vipers
    env.PrependENVPath('PATH', '/opt/arm_tools/bin')

    # Append /opt/arcom/bin to env['ENV']['PATH'],
    # so that it is the fallback if arm-linux-gcc is
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

    # do g++ --version, grab 3rd field for CXXVERSION
    try:
        # rev = re.split('\s+',os.popen(env['CXX'] + ' --version').readline())[2]
        # env.Replace(CXXVERSION = rev)
        revline = subprocess.Popen(env['CXX'] + ' --version').readline()
        print "revline=" + revline
    except OSError, (errno,strerror):
        # print "Error(%s): %s" %(errno,strerror)
        print "Error: %s: %s" %(env['CXX'],strerror)

def exists(env):
    return env.Detect(['arm-linux-gcc'])


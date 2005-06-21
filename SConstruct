# -*- python -*-

##  Copyright 2005 UCAR, NCAR, All Rights Reserved

##
Help("""
targets
(none) build ARM driver modules and ARM & X86 library and executables.
x86    build library and executable programs for X86.
arm    build driver modules, library and executable programs for ARM.
lib    build library for X86 and ARM.
x86_install  build and install X86 library and executables.
arm_install  build and install ARM modules, library and executables.

Files are installed at /opt/ads3/arm and /opt/ads3/x86.
""")

##
##  Store all signatures in the '.sconsign.dblite'
##  file at the top-level SConstruct directory.
##
SConsignFile()

##
##  Create a new construction environment variable and
##  restrict it's build methods to be POSIX based only.
##
##  If you want to use the user's env variable PATH to find
##  the compiler and build tools, uncomment these two lines,
##  and remove the hard-coded paths to the arm-linux-*
##  tools below.
##
# import os
# env = Environment(platform = 'posix',ENV= os.environ)
##
##  Otherwise, do this:
##
env = Environment(platform = 'posix')

##
##  TODO - Check out missing files from source control.
##
##  scons will, by default, fetch files from SCCS or RCS subdirecto-
##  ries without explicit configuration.  This takes some extra pro-
##  cessing time to search for the necessary source code  management
##  files  on disk.  You can avoid these extra searches and speed up
##  your build a little by disabling these searches as follows:
##
env.SourceCode('.', None)

##
##  Define it's compiler flags for the all build tools.
##-O2 <-> -g
env['CCFLAGS'] = Split("""
    -Wall -O2
""")

env['CXXFLAGS'] = Split("""
    -Wall -O2
""")

##
##  Define it's C/C++ include paths for all builds.
##

env['CPPPATH'] = Split("""
  #/dsm/modules
  #/dsm/class
  .
""")

##
##  Adjust the env for cross-building to the xScale ARM processor...
##
arm_env = env.Copy()

arm_env.Replace(ADS3_INSTALL = '/opt/ads3/arm')

# arm_env.AppendUnique(CCFLAGS=Split("""
#   -mcpu=xscale
# """))


arm_env.AppendUnique(CPPPATH =
    Split("""
	$ADS3_INSTALL/include
    """)
)

arm_env.AppendUnique(LIBPATH =
    Split("""
	#dsm/class/arm
	$ADS3_INSTALL/lib
    """)
)

##
## Specify RPATH to avoid the need for LD_LIBRARY_PATH later 
##
arm_env.AppendUnique(RPATH = 
    Split("""
	/var/tmp/lib
	$ADS3_INSTALL/lib
    """)
)

arm_env.Replace(AR	= '/opt/arm_tools/bin/arm-linux-ar')
arm_env.Replace(AS	= '/opt/arm_tools/bin/arm-linux-as')
arm_env.Replace(CC	= '/opt/arm_tools/bin/arm-linux-gcc')
arm_env.Replace(CXX	= '/opt/arm_tools/bin/arm-linux-g++')
arm_env.Replace(LINK	= '/opt/arm_tools/bin/arm-linux-g++')
arm_env.Replace(RANLIB	= '/opt/arm_tools/bin/arm-linux-ranlib')
arm_env.Replace(LEX	= '/opt/arm_tools/arm-linux/bin/flex++')


##
##  Adjust the env for building to the x86 processor...
##
x86_env = env.Copy()

x86_env.Replace(ADS3_INSTALL = '/opt/ads3/x86')

##
##  Define it's compiler flags for the all build tools.
##-O2 <-> -g
x86_env.Replace(CCFLAGS = Split("-Wall -O2 -g"))
x86_env.Replace(CXXFLAGS = Split("-Wall -O2 -g"))

##     /scr/tmp/maclean/isa_tmp/fc3/include
x86_env.AppendUnique(CPPPATH =
    Split("""
	$ADS3_INSTALL/include
    """)
)

x86_env.AppendUnique(LIBPATH =
    Split("""
	#dsm/class/x86
	$ADS3_INSTALL/lib
    """)
)

##
## Specify RPATH to avoid the need for LD_LIBRARY_PATH later
##
x86_env.AppendUnique(RPATH = [
    '$ADS3_INSTALL/lib',
    x86_env.Dir("#dsm/class/x86").get_abspath()
    ]) 

##
##  Build dsm/modules
##
SConscript('dsm/modules/SConscript',
	build_dir='dsm/modules/arm',
	duplicate=0,exports={'env':arm_env})

##
##  Build libDsm.a
##
SConscript('dsm/class/SConscript',
	build_dir='dsm/class/arm',
	duplicate=0,exports={'env':arm_env})

SConscript('dsm/class/SConscript',
	build_dir='dsm/class/x86',
	duplicate=0,exports={'env':x86_env})

##
##  Build arm executables
##
SConscript('dsm/src/SConscript',
	build_dir='dsm/src/arm',
	duplicate=0,exports={'env':arm_env})

##
##  Build x86 executables
##
SConscript('dsm/src/SConscript',
	build_dir='dsm/src/x86',
	duplicate=0,exports={'env':x86_env})

##
##  target for creating arm modules, library and executables
##
Alias('arm', ['dsm/modules/arm', 'dsm/class/arm','dsm/src/arm'])

##
##  target for creating x86 library and executables
##
Alias('x86', ['dsm/class/x86','dsm/src/x86'])

##
##  target for creating arm and x86 libraries
##
Alias('lib', ['dsm/class/x86','dsm/class/arm'])

##
##  target for installing arm modules, library and executables
##
Alias('arm_install', [
	arm_env['ADS3_INSTALL'] + '/modules',
	arm_env['ADS3_INSTALL'] + '/lib',
	arm_env['ADS3_INSTALL'] + '/bin'
    ]
)

##
##  target for installing x86 library and executables
##
Alias('x86_install', [
	x86_env['ADS3_INSTALL'] + '/lib',
	x86_env['ADS3_INSTALL'] + '/bin'
    ]
)

Alias('install', ['arm_install','x86_install'])

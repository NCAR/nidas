# -*- python -*-
##  Copyright 2005 UCAR, NCAR, All Rights Reserved

import os

##
## command line options
##
opts = Options('nids.conf')
opts.Add('PREFIX', 'installation path: $PREFIX/x86, $PREFIX/arm', '/opt/ads3')
##
##  Create a new construction environment variable and
##  restrict it's build methods to be POSIX based only.
##
env = Environment(platform = 'posix',options=opts,
    ENV = {'PATH' : os.environ['PATH']})

opts.Save('nids.conf',env)

Help(opts.GenerateHelpText(env) + """
targets
(none) build ARM driver modules and ARM & X86 library and executables.
x86    build library and executable programs for X86.
arm    build driver modules, library and executable programs for ARM.
lib    build library for X86 and ARM.
x86_install  build and install X86 library and executables.
arm_install  build and install ARM modules, library and executables.
""")

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
##  Store all signatures in the '.sconsign.dblite'
##  file at the top-level SConstruct directory.
##
SConsignFile()

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

##
## Append arm to PREFIX
##
armprefix = arm_env['PREFIX'] + '/arm'
arm_env.Replace(PREFIX = armprefix)

# arm_env.AppendUnique(CCFLAGS=Split("""
#   -mcpu=xscale
# """))


arm_env.AppendUnique(CPPPATH =
    Split("""
	$PREFIX/include
    """)
)

arm_env.AppendUnique(LIBPATH =
    Split("""
	#dsm/class/arm
	$PREFIX/lib
    """)
)

##
## Specify RPATH to avoid the need for LD_LIBRARY_PATH later 
##
arm_env.AppendUnique(RPATH = 
    Split("""
	/var/tmp/lib
	$PREFIX/lib
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

##
## Append x86 to PREFIX
##
x86prefix = x86_env['PREFIX'] + '/x86'
x86_env.Replace(PREFIX = x86prefix)

##
##  Define it's compiler flags for the all build tools.
##-O2 <-> -g
x86_env.Replace(CCFLAGS = Split("-Wall -O2 -g"))
x86_env.Replace(CXXFLAGS = Split("-Wall -O2 -g"))

##     /scr/tmp/maclean/isa_tmp/fc3/include
x86_env.AppendUnique(CPPPATH =
    Split("""
	$PREFIX/include
    """)
)

x86_env.AppendUnique(LIBPATH =
    Split("""
	#dsm/class/x86
	$PREFIX/lib
    """)
)

##
## Specify RPATH to avoid the need for LD_LIBRARY_PATH later
##
x86_env.AppendUnique(RPATH = [
    x86_env.Dir("#dsm/class/x86").get_abspath(),
    '$PREFIX/lib'
    ]) 

##
##  Build dsm/modules/arm
##
SConscript('dsm/modules/SConscript',
	build_dir='dsm/modules/arm',
	duplicate=0,exports={'env':arm_env,'headers_only':0})

##
##  Build dsm/modules/x86, but only target the headers
##
SConscript('dsm/modules/SConscript',
	build_dir='dsm/modules/x86',
	duplicate=0,exports={'env':x86_env,'headers_only':1})

##
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
arm_env.Alias('arm_install', [
	'$PREFIX/modules',
	'$PREFIX/include',
	'$PREFIX/lib',
	'$PREFIX/bin'
    ]
)

##
##  target for installing x86 library and executables
##
x86_env.Alias('x86_install', [
	'$PREFIX/include',
	'$PREFIX/lib',
	'$PREFIX/bin'
    ]
)
#	'$PREFIX/modules',

Alias('install', ['arm_install','x86_install'])

Default([ 'arm', 'x86' ])

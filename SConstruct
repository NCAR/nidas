# -*- python -*-

##
##  Create a new construction environment variable.
##
env = Environment()

##
##  Restrict it's build methods to be POSIX based only.
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
  -Wall -Wno-trigraphs -Wstrict-prototypes -pipe -g
  -fno-omit-frame-pointer -fno-strict-aliasing
""")

##
##  Define it's C/C++ include paths for all builds.
##
env['CPPPATH'] = Split("""
  /jnet/linux/include
  #/dsm/modules
  #/dsm/class
  #/disc/class
""")

##
##  Define common library path
##
env['LIBPATH'] = '#/lib'

##
##  Adjust the env for cross-building to the xScale ARM processor...
##

arm_env = env.Copy()
arm_env.AppendUnique(CCFLAGS=Split("""
  -Wa,-mcpu=xscale -mapcs -mapcs-32 -march=armv4 -mno-sched-prolog
  -mshort-load-bytes -mtune=strongarm -Uarm -DDSM
"""))

arm_env['AR']        = '/opt/rtldk-2.0/bin/arm-linux-ar'
arm_env['AS']        = '/opt/rtldk-2.0/bin/arm-linux-as'
arm_env['CC']        = '/opt/rtldk-2.0/bin/arm-linux-gcc'
arm_env['CXX']       = '/opt/rtldk-2.0/bin/arm-linux-g++'
arm_env['LINK']      = '/opt/rtldk-2.0/bin/arm-linux-g++'
arm_env['RANLIB']    = '/opt/rtldk-2.0/bin/arm-linux-ranlib'
arm_env['OBJSUFFIX'] = '.arm.o'

##
##  Adjust the env for cross-building to the x86 ARM processor...
##
x86_env = env.Copy()
x86_env['OBJSUFFIX'] = '.x86.o'

##
##  Export the environments to the SConscript files
##
Export('arm_env')
Export('x86_env')

##
##  Build dsm/modules/???.o
##
SConscript('dsm/modules/SConscript', build_dir='dsm/modules/obj', duplicate=0)

##
##  Build lib/libArmDsm.a
##
SConscript('dsm/class/SConscript',   build_dir='dsm/class/obj',   duplicate=0)

##
##  Build bin/dsmAsync, bin/dsmSync, bin/dsmComm
##
SConscript('dsm/src/SConscript',     build_dir='dsm/src/obj',     duplicate=0)

##
##  Build lib/libArmDisc.a and lib/libX86Disc.a
##
SConscript('disc/class/SConscript',  build_dir='disc/class/obj',  duplicate=0)

##
##  Build bin/discAsync, bin/discSync, bin/discComm
##
SConscript('disc/src/SConscript',    build_dir='disc/src/obj',    duplicate=0)

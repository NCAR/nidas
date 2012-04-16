# -*- python -*-
##  Copyright 2005,2006 UCAR, NCAR, All Rights Reserved

Import('env')

# The qt4 tool updates the environment from eol_scons.GlobalVariables().
# If the original value of PREFIX is set as a global Variable in the top
# level SConstruct, via eol_scons.GlobalVariables(), then env['PREFIX']
# will be reset back to the original value by the qt4 tool, overwriting
# the value that is passed in the environment to this SConscript.
# If that is not what is wanted, one can correct back to the modified value of
# PREFIX in the Clone:
# env = env.Clone(tools = ['qt4'],PREFIX=env['PREFIX'])

env = env.Clone(tools = ['qt4'])
arch = env['ARCH']  # empty string for native builds

Import(['LIBNIDAS_UTIL' + arch,'LIBNIDAS' + arch,'LIBNIDAS_DYNLD' + arch,'NIDAS_APPS' + arch])

libutil = locals()['LIBNIDAS_UTIL' + arch]
libnidas = locals()['LIBNIDAS' + arch]
libdynld = locals()['LIBNIDAS_DYNLD' + arch]
apps = locals()['NIDAS_APPS' + arch]

libpath = [ libutil.Dir(''), libnidas.Dir(''), libdynld.Dir('') ]

# Override CXXFLAGS in order to turn off -Weffc++ for now
env['CXXFLAGS'] = [ '-Wall','-O2' ]

qt4Modules = Split('QtSql QtGui QtCore QtNetwork')
env.EnableQt4Modules(qt4Modules)

uis = Split("""
    EditCalDialog.ui
    ViewTextDialog.ui
""")

env.Uic4(uis)

sources = Split("""
    main.cc
    polyfitgsl.cc
    TreeItem.cc
    TreeModel.cc
    AutoCalClient.cc
    CalibrationWizard.cc
    Calibrator.cc
    ComboBoxDelegate.cc
    EditCalDialog.cc
    ViewTextDialog.cc
""")

auto_cal = env.Program('auto_cal', sources,
    LIBS=[env['LIBS'],'nidas_util','nidas','nidas_dynld','gsl','gslcblas','xerces-c','xmlrpcpp'],
    LIBPATH=[env['LIBPATH'],libpath])

name = env.subst("${TARGET.filebase}", target=auto_cal)
apps[name] = auto_cal
Export({'NIDAS_APPS' + arch: apps})

inode = env.Install('$PREFIX/bin',auto_cal)
env.Clean('install',inode)


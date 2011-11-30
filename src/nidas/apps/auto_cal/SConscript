# -*- python -*-
##  Copyright 2005,2006 UCAR, NCAR, All Rights Reserved

Import('env')
# The qt4 tool updates the environment from the eol_scons GlobalVariables,
# which results in env['PREFIX'] being reset back to the original value, not
# the modified value passed in the environment down to this SConscript.
# So, short of removing the use of eol_scons.GlobalVariables() we'll
# reset PREFIX after invoking the tool.
env = env.Clone(tools = ['qt4'],PREFIX=env['PREFIX'])
arch = env['ARCH']

Import(['LIBNIDAS_UTIL_' + arch,'LIBNIDAS_' + arch,'LIBNIDAS_DYNLD_' + arch,'NIDAS_APPS_' + arch])

libutil = locals()['LIBNIDAS_UTIL_' + arch]
libnidas = locals()['LIBNIDAS_' + arch]
libdynld = locals()['LIBNIDAS_DYNLD_' + arch]
apps = locals()['NIDAS_APPS_' + arch]

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
Export({'NIDAS_APPS_' + arch: apps})

inode = env.Install('$PREFIX/bin',auto_cal)
env.Clean('install',inode)


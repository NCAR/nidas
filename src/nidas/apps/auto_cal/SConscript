# -*- python -*-
##  Copyright 2005,2006 UCAR, NCAR, All Rights Reserved

Import(['env','LIBNIDAS_UTIL','LIBNIDAS','LIBNIDAS_DYNLD','NIDAS_APPS'])

# The qt4 tool updates the environment from the eol_scons GlobalVariables,
# which results in env['PREFIX'] being reset back to the original value, not
# the modified value passed in the environment down to this SConscript.
# So, short of removing the use of eol_scons.GlobalVariables() we'll
# reset PREFIX after invoking the tool.
env = env.Clone(tools = ['qt4'],PREFIX=env['PREFIX'])

libpath = [ LIBNIDAS_UTIL.Dir(''), LIBNIDAS.Dir(''), LIBNIDAS_DYNLD.Dir('') ]

# env.Append(CCFLAGS=['-Wall'])
# env.Append(CCFLAGS=['-Weffc++'])

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
    LIBS=[env['LIBS'],'nidas_util','nidas','nidas_dynld','gsl','xerces-c','xmlrpcpp'],
    LIBPATH=libpath)
name = env.subst("${TARGET.filebase}", target=auto_cal)
NIDAS_APPS[name] = auto_cal

inode = env.Install('$PREFIX/bin',auto_cal)
env.Clean('install',inode)


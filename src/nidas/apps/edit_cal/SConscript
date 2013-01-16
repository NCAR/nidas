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

env = env.Clone(tools = ['qt4', 'qwt', 'gsl'])
arch = env['ARCH']  # empty string for native builds

qt4Modules = Split('QtSql QtGui QtCore QtNetwork')
if not env.EnableQt4Modules(qt4Modules):
    Return()

if not env.EnableQwt():
    print "qwt.h not found.  Do \"scons --config=force\" to redo the check. See config.log for more information"
    Return()

# Override CXXFLAGS in order to turn off -Weffc++ for now
env['CXXFLAGS'] = [ '-Wall','-O2' ]

#   CheckableMessageBox.ui
#   CheckableMessageBox.cc
uis = Split("""
    ViewTextDialog.ui
    CalibrationPlot.ui
    CalibrationForm.ui
""")

env.Uic4(uis)

#   EditCalPltWindow.cc
#   CalibrationPlot.cc
sources = Split("""
    main.cc
    MainWindow.cc
    CalibrationPlot.cc
    CalibrationForm.cc
    polyfitgsl.cc
    SortFilterProxyModel.cc
    ViewTextDialog.cc
""")

edit_cal = env.Program('edit_cal', sources,
    LIBS=[env['LIBS']], LIBPATH=[env['LIBPATH']])

name = env.subst("${TARGET.filebase}", target=edit_cal)

inode = env.Install('$PREFIX/bin',edit_cal)
env.Clean('install',inode)

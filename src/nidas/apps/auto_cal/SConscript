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

env = env.Clone(tools = ['qt4','nidas','gsl'])

# Override CXXFLAGS in order to turn off -Weffc++ for now
env['CXXFLAGS'] = [ '-Wall','-O2' ]

qt4Modules = Split('QtGui QtCore QtNetwork')
env.EnableQt4Modules(qt4Modules)

sources = Split("""
    main.cc
    TreeItem.cc
    TreeModel.cc
    AutoCalClient.cc
    CalibrationWizard.cc
    Calibrator.cc
""")

auto_cal = env.NidasProgram('auto_cal', sources)

name = env.subst("${TARGET.filebase}", target=auto_cal)

inode = env.Install('$PREFIX/bin',auto_cal)
env.Clean('install',inode)

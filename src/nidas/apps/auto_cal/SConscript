# -*- python -*-
##  Copyright 2005,2006 UCAR, NCAR, All Rights Reserved

import os,sys,re
import eol_scons
Import('env')

# For some reason we have to pass PREFIX across this Clone,
# otherwise it gets reset back to the initial value from the
# top SConstruct (without the x86 suffix).
tools = Split("""qt4""")
env = env.Clone(tools = tools,PREFIX=env['PREFIX'])
# print 'apps/auto_cal/SConscript: PREFIX=' + env['PREFIX']

# env.Append(CCFLAGS=['-Wall'])
# env.Append(CCFLAGS=['-Weffc++'])

qt4Modules = Split('QtGui QtCore QtNetwork')
env.EnableQt4Modules(qt4Modules)

# These are already in the env passed from nidas/apps
# allLibs = env['LIBS']
# allLibs.append( 'xmlrpcpp' )
# allLibs.append( 'xerces-c' )
# allLibs.append( 'gsl' )
# allLibs.append( 'gslcblas' )

SOURCES = [Split("""
    TreeItem.cc
    TreeModel.cc
    AutoCalClient.cc
    CalibrationWizard.cc
    Calibrator.cc
    main.cc
""") ]

auto_cal = env.Program('auto_cal', SOURCES,LIBS=[env['LIBS'],'gsl','gslcblas'])

Alias('install', env.Install('$PREFIX/bin',auto_cal))

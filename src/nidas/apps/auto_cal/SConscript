# -*- python -*-
##  Copyright 2005,2006 UCAR, NCAR, All Rights Reserved

import os,sys,re
import eol_scons

tools = Split("""qt4 nidas""")
env = Environment(tools = ['default'] + tools)

env.Append(CCFLAGS=['-Wall'])
#env.Append(CCFLAGS=['-Weffc++'])

qt4Modules = Split('QtGui QtCore QtNetwork')
env.EnableQt4Modules(qt4Modules)

allLibs = env['LIBS']
allLibs.append( 'xmlrpcpp' )
allLibs.append( 'gsl' )
allLibs.append( 'gslcblas' )

SOURCES = [Split("""
    TreeItem.cc
    TreeModel.cc
    AutoCalClient.cc
    CalibrationWizard.cc
    Calibrator.cc
    main.cc
""") ]

auto_cal = env.Program('auto_cal', SOURCES, LIBS=allLibs)

PREFIX = env['PREFIX'] + '/x86/bin'

Alias('install', env.Install(PREFIX,'auto_cal'))

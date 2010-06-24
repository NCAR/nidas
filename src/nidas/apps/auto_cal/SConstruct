# -*- python -*-
##  Copyright 2005,2006 UCAR, NCAR, All Rights Reserved

import os,sys,re
import eol_scons


##sys.path.insert(0,os.path.join(os.getcwd(),'sconslib'))


os.environ["QT4DIR"]="/usr/lib/qt4"
os.environ["QTDIR"]="/usr/lib/qt4"
os.environ["QTINC"]="/usr/lib/qt4/incluce"
os.environ["setenv"]="/usr/lib/qt4/lib"
sys.path.insert(0, "/usr/lib/qt4/bin:")

tools = Split("""qt4 nidas""")
env = Environment(tools = ['default'] + tools)

qt4Modules = Split('QtGui QtCore QtNetwork')
env.EnableQt4Modules(qt4Modules)

allLibs = env['LIBS']
allLibs.append( 'gsl' )
allLibs.append( 'gslcblas' )

print "LIBS =", allLibs

SOURCES = [Split("""
    TreeItem.cc
    TreeModel.cc
    AutoCalClient.cc
    CalibrationWizard.cc
    Calibrator.cc
    main.cc
""") ]

HEADERS = [Split("""
    CalibrationWizard.h
""")]

auto_cal = env.Program('AutoCalWiz', SOURCES, LIBS=allLibs)

Alias('install', env.Install('/opt/local/nidas/x86/bin','auto_cal'))

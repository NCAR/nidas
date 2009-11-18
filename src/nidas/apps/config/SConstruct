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

SOURCES = [Split("""
    main.cc
    configwindow.cc
    DSMTableWidget.cc
    Document.cc
    SensorCatalogWidget.cc
    exceptions/UserFriendlyExceptionHandler.cc
    exceptions/CuteLoggingExceptionHandler.cc
    exceptions/CuteLoggingStreamHandler.cc
    AddSensorDialog.cc
    AddSensorComboDialog.cc
""") ]

HEADERS = [Split("""
    DSMTableWidget.h
    configwindow.h
    SensorCatalogWidget.h
""")]

HEADERS += env.Uic4("""AddSensorDialog.ui""")
HEADERS += env.Uic4("""AddSensorComboDialog.ui""")

configview = env.Program('configview', SOURCES)

Alias('install', env.Install('/opt/local/nidas/x86/bin','configview'))


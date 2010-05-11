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
    Document.cc
    exceptions/UserFriendlyExceptionHandler.cc
    exceptions/CuteLoggingExceptionHandler.cc
    exceptions/CuteLoggingStreamHandler.cc
    AddSensorComboDialog.cc
    AddDSMComboDialog.cc
    AddSampleComboDialog.cc
    AddA2DVariableComboDialog.cc
    DeviceValidator.cc
    nidas_qmv/ProjectItem.cc
    nidas_qmv/SiteItem.cc
    nidas_qmv/DSMItem.cc
    nidas_qmv/SensorItem.cc
    nidas_qmv/SampleItem.cc
    nidas_qmv/VariableItem.cc
    nidas_qmv/NidasItem.cc
    nidas_qmv/NidasModel.cc
""") ]

HEADERS = [Split("""
    configwindow.h
""")]

HEADERS += env.Uic4("""AddSensorComboDialog.ui""")
HEADERS += env.Uic4("""AddDSMComboDialog.ui""")
HEADERS += env.Uic4("""AddSampleComboDialog.ui""")
HEADERS += env.Uic4("""AddA2DVariableComboDialog.ui""")

configview = env.Program('configview', SOURCES)

Alias('install', env.Install('/opt/local/nidas/x86/bin','configview'))


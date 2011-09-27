# -*- python -*-
##  Copyright 2005,2006 UCAR, NCAR, All Rights Reserved

import os,sys,re
import eol_scons

##sys.path.insert(0,os.path.join(os.getcwd(),'sconslib'))

tools = Split("""qt4 nidas""")
env = Environment(tools = ['default'] + tools)

qt4Modules = Split('QtGui QtCore QtNetwork')
env.EnableQt4Modules(qt4Modules)


env.Append(CPPPATH=[os.path.join(os.environ['JLOCAL'],'include'), ])
env.Append(LIBPATH=[os.path.join(os.environ['JLOCAL'],'lib'), ])
env.Append(LIBS=['raf++'])

SOURCES = [Split("""
    main.cc
    configwindow.cc
    Document.cc
    exceptions/UserFriendlyExceptionHandler.cc
    exceptions/CuteLoggingExceptionHandler.cc
    exceptions/CuteLoggingStreamHandler.cc
    AddSensorComboDialog.cc
    AddDSMComboDialog.cc
    AddA2DVariableComboDialog.cc
    VariableComboDialog.cc
    DeviceValidator.cc
    nidas_qmv/ProjectItem.cc
    nidas_qmv/SiteItem.cc
    nidas_qmv/DSMItem.cc
    nidas_qmv/SensorItem.cc
    nidas_qmv/A2DSensorItem.cc
    nidas_qmv/PMSSensorItem.cc
    nidas_qmv/VariableItem.cc
    nidas_qmv/A2DVariableItem.cc
    nidas_qmv/NidasItem.cc
    nidas_qmv/NidasModel.cc
""") ]

HEADERS = [Split("""
    configwindow.h
""")]

HEADERS += env.Uic4("""AddSensorComboDialog.ui""")
HEADERS += env.Uic4("""AddDSMComboDialog.ui""")
HEADERS += env.Uic4("""AddA2DVariableComboDialog.ui""")
HEADERS += env.Uic4("""VariableComboDialog.ui""")

configedit = env.Program('configedit', SOURCES)

Alias('install', env.Install('/opt/local/nidas/x86/bin','configedit'))


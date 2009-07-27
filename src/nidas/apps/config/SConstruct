# -*- python -*-
##  Copyright 2005,2006 UCAR, NCAR, All Rights Reserved

import os,sys,re
import eol_scons


##sys.path.insert(0,os.path.join(os.getcwd(),'sconslib'))


os.environ["QT4DIR"]="/usr/lib/qt4"

tools = Split("""qt4 nidas""")
env = Environment(tools = ['default'] + tools)

qt4Modules = Split('QtGui QtCore QtNetwork')
env.EnableQt4Modules(qt4Modules)

SOURCES = [Split("""
    configview.cc
    dsmtablewidget.cc
""") ]

HEADERS = ["dsmtablewidget.h"]
configview = env.Program('configview', SOURCES)


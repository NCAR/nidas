# -*- python -*-
##  Copyright 2005,2006 UCAR, NCAR, All Rights Reserved

import os

Import('env')

# Check if $JLOCAL/include/raf and $JLOCAL/lib exists
if (not os.path.exists(os.path.join(env["JLOCAL"],'include','raf'))) or \
(not os.path.exists(os.path.join(env["JLOCAL"],'lib'))):
    print 'Cannot find $JLOCAL/include/raf or $JLOCAL/lib. configedit will not be built'
    Return()

env = env.Clone(tools = ['qt4'])
arch = env['ARCH']  # empty string for native builds

Import(['LIBNIDAS_UTIL' + arch,'LIBNIDAS' + arch,'LIBNIDAS_DYNLD' + arch,
    'NIDAS_APPS' + arch])
libutil = locals()['LIBNIDAS_UTIL' + arch]
libnidas = locals()['LIBNIDAS' + arch]
libdynld = locals()['LIBNIDAS_DYNLD' + arch]
apps = locals()['NIDAS_APPS' + arch]

libpath = [ libutil.Dir(''), libnidas.Dir(''), libdynld.Dir('') ]

qt4Modules = Split('QtGui QtCore QtNetwork')
env.EnableQt4Modules(qt4Modules)

# Override CXXFLAGS in order to turn off -Weffc++ for now
env['CXXFLAGS'] = [ '-Wall','-O2' ]

env.Append(CPPPATH=[os.path.join(env['JLOCAL'],'include'),'.'])
env.Append(LIBPATH=[os.path.join(env['JLOCAL'],'lib'),libpath ])
env.Append(LIBS=['nidas_util','nidas','nidas_dynld','xerces-c','raf++','VarDB','netcdf','hdf5_hl','hdf5'])

sources = Split("""
    main.cc
    configwindow.cc
    Document.cc
    exceptions/UserFriendlyExceptionHandler.cc
    exceptions/CuteLoggingExceptionHandler.cc
    exceptions/CuteLoggingStreamHandler.cc
    AddSensorComboDialog.cc
    AddDSMComboDialog.cc
    AddA2DVariableComboDialog.cc
    NewProjectDialog.cc
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
""")

headers = Split("""
    configwindow.h
""")

headers += env.Uic4("""AddSensorComboDialog.ui""")
headers += env.Uic4("""AddDSMComboDialog.ui""")
headers += env.Uic4("""AddA2DVariableComboDialog.ui""")
headers += env.Uic4("""VariableComboDialog.ui""")
headers += env.Uic4("""NewProjectDialog.ui""")

configedit = env.Program('configedit', sources)

name = env.subst("${TARGET.filebase}", target=configedit)
apps[name] = configedit
Export({'NIDAS_APPS' + arch: apps})

inode = env.Install('$PREFIX/bin',configedit)
env.Clean('install',inode)


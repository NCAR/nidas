# -*- python -*-

#import os.path
#import eol_scons 
        
def AutoCal(env):
    env.Require(['prefixoptions', 'buildmode'])
    env['DEFAULT_OPT_PREFIX'] = '/opt/local/nidas/x86'
    return env

tools = Split("""prefixoptions buildmode qt4 nidas xmlrpc gsl""")

env = Environment(tools = ['default'], GLOBAL_TOOLS = [AutoCal])
env = Environment(tools = ['default'] + tools)

# env.Append(CCFLAGS=['-Wall'])
# env.Append(CCFLAGS=['-Weffc++'])

qt4Modules = ['QtCore', 'QtGui', 'QtNetwork', 'QtSql']
env.EnableQt4Modules(qt4Modules)

uis = Split("""
EditCalDialog.ui
""")

sources = Split("""
main.cc
TreeItem.cc
TreeModel.cc
AutoCalClient.cc
CalibrationWizard.cc
Calibrator.cc
ComboBoxDelegate.cc
EditCalDialog.cc
""")

env.Uic4(uis)

auto_cal = env.Program('auto_cal', sources)

Alias('install', env.Install(env['OPT_PREFIX']+'/bin','auto_cal'))

for item in sorted(env.Dictionary().items()):
    print "construction variable = '%s', value = '%s'" % item

import os
import re
import subprocess

def get_svnversion(env):
    """
    Return value reported by svnversion on top directory.
    """
    svn_path = env.WhereIs('svnversion', os.environ['PATH'])
    revision = "Unknown"

    if svn_path != None:
	rev = os.popen('svnversion ' + env.Dir('#.').abspath).readline().strip()
	if rev != "":
		    revision = rev
    return revision

def get_cxxversion(env):
    try:
        # do g++ --version, grab 3rd field for CXXVERSION
        revline = subprocess.Popen([env['CXX'],'--version'],
            env={'PATH':env['ENV']['PATH']},
            stdout=subprocess.PIPE).stdout.readline()
        rev = re.split('\s+',revline)[2]
        return rev
    except OSError, (errno,strerror):
        print "Error: %s: %s" %(env['CXX'],strerror)
        return None

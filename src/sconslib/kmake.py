
import os
import SCons.Errors
# import re

def run(cmd):
    import sys
    import os
    import SCons.Script
    """Run a command and return the return code."""
    res = os.system(cmd)
    code = 0
    # Assumes that if a process doesn't call exit, it was successful
    if (os.WIFEXITED(res)):
        code = os.WEXITSTATUS(res)
        if code != 0:
            print "Error: return code: " + str(code)
            raise Exception(cmd + ": error: return code: " + str(code))
            # if SCons.Script.keep_going_on_error == 0:
             #    sys.exit(code)
    return code

def Kmake(env,target,source):

    if not env.has_key('KERNELDIR') or env['KERNELDIR'] == '':
	    print "KERNELDIR not specified, " + target[0].abspath + " will not be built"
            return None

    print 'KMAKE=' + env['KMAKE']
    print "KMAKE PATH=" + env['ENV']['PATH']
    # Have the shell subprocess do a cd to the source directory.
    # If scons/python does it, then the -j multithreaded option doesn't work.
    srcdir = os.path.dirname(source[0].abspath)
    try:
      env.Execute('cd ' + srcdir + ';' + env['KMAKE'])
    except:
      raise SCons.Errors.UserError, 'error in ' + env['KMAKE']
      
    return None

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


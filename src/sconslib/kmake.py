import os
import SCons.Errors

def Kmake(env,target,source):

    if not env.has_key('KERNELDIR') or env['KERNELDIR'] == '':
	    print "KERNELDIR not specified, " + target[0].abspath + " will not be built"
            return None

    # Have the shell subprocess do a cd to the source directory.
    # If scons/python does it, then the -j multithreaded option doesn't work.
    srcdir = os.path.dirname(source[0].abspath)
    if env.Execute('cd ' + srcdir + '; ' + env['KMAKE']) != 0:
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


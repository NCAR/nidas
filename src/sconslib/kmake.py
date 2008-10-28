import os
import re
import subprocess
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



import os
# import re

def Kmake(env,target,source):

    def run(cmd):
	import sys
	import os
	import SCons.Script
	"""Run a command and decipher the return code. Exit by default."""
	res = os.system(cmd)
	# Assumes that if a process doesn't call exit, it was successful
	if (os.WIFEXITED(res)):
	    code = os.WEXITSTATUS(res)
	    if code != 0:
		print "Error: return code: " + str(code)
		if SCons.Script.keep_going_on_error == 0:
		    sys.exit(code)


    if not env.has_key('KERNELDIR') or env['KERNELDIR'] == '':
	    print "KERNELDIR not specified, " + target[0].abspath + " will not be built"
	    return

    # print (["sources="] + [s.path for s in source])
    # print (["targets="] + [s.path for s in target])

    cwd = os.getcwd()
    # print "os.getcwd()=" + os.getcwd()

    # get absolute path to target on the build_dir
    srcdir = os.path.dirname(source[0].abspath)
    # print "srcdir=" + srcdir

    # convert build_arm to nidas
    # srcdir = re.compile('build_' + env['ARCH']).sub('nidas',srcdir)
    # convert build_xxx to xxx
    # srcdir = re.compile('build_').sub('',srcdir)

    # print "srcdir=" + srcdir
    # if not os.path.exists(srcdir):
    #   os.makedirs(srcdir)
    os.chdir(srcdir)

    # print "os.getcwd()=" + os.getcwd()

    # print 'makecmd=' + makecmd
    run(env['KMAKE'])
    os.chdir(cwd)


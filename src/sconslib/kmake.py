
import os
# import re

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

def Kmake(env,target,source):

    if not env.has_key('KERNELDIR') or env['KERNELDIR'] == '':
	    print "KERNELDIR not specified, " + target[0].abspath + " will not be built"
	    return

    # print (["sources="] + [s.path for s in source])
    # print (["targets="] + [s.path for s in target])

    cwd = os.getcwd()
    # print "os.getcwd()=" + os.getcwd()

    # get absolute path to source on the build_dir
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


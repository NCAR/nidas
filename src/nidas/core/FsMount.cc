/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-04-23 12:53:07 -0600 (Thu, 23 Apr 2009) $

    $LastChangedRevision: 4578 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/FsMount.cc $
 ********************************************************************

*/

#include <nidas/core/FsMount.h>
#include <nidas/core/FileSet.h>
#include <nidas/util/Logger.h>
#include <nidas/util/Process.h>
#include <nidas/util/util.h>

#include <fstream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

FsMount::FsMount() : _type("auto"),_fileset(0),_worker(0)
{
}

void FsMount::setDevice(const std::string& val)
{
    _device = val;
    _deviceExpanded = n_u::Process::expandEnvVars(val);
    _deviceMsg = (_device == _deviceExpanded) ? _device :
        _device + "(" + _deviceExpanded + ")";

}

void FsMount::setDir(const std::string& val)
{
    _dir = val;
    _dirExpanded = n_u::Process::expandEnvVars(val);
    _dirMsg = (_dir == _dirExpanded) ? _dir :
        _dir + "(" + _dirExpanded + ")";
}

void FsMount::mount()
       throw(n_u::Exception)
{
    if (isMounted()) return;
    n_u::Logger::getInstance()->log(LOG_INFO,"Mounting: %s at %s",
        _deviceMsg.c_str(),_dirMsg.c_str());

    /* A mount can be done with either the libc mount() function
     * or the mount command.
     * advantages of mount command:
     *  1. allows non-root users to mount/unmount a filesystem if the
     *      "user" option is in the fstab.
     *  2. sys admin can put the appropriate options in /etc/fstab.
     *      Nidas config then only needs to know the mount point.
     */

    string cmd = string("mount");
    if (getDeviceExpanded().length() > 0)
        cmd += ' ' + getDeviceExpanded();
    if (getType().length() > 0)
        cmd += " -t " + getType();
    if (getOptions().length() > 0) 
        cmd += " -o " + getOptions();
    cmd += ' ' + getDirExpanded() + " 2>&1";

    _mountProcess = n_u::Process::spawn(cmd);
    string cmdout;
    istream& outst = _mountProcess.outStream();
    for (; !outst.eof();) {
        char cbuf[32];
        outst.read(cbuf,sizeof(cbuf)-1);
        cbuf[outst.gcount()] = 0;
        cmdout += cbuf;
    }

    n_u::trimString(cmdout);

    int status;
    _mountProcess.wait(true,&status);
    if (!WIFEXITED(status) || WEXITSTATUS(status))
        throw n_u::IOException(cmd,"failed",cmdout);
}

/* Just issue a "cd /dir || mount /dir" command.
 * If /dir is automounted then it may work, whereas
 * "mount /dev/sdXn -o blahblah /dir"
 * will likely fail for a normal user on a server.
 */
void FsMount::autoMount()
       throw(n_u::Exception)
{
    if (isMounted()) return;
    n_u::Logger::getInstance()->log(LOG_INFO,"Automounting: %s",
        _dirMsg.c_str());

    const string& dir = getDirExpanded();
    string cmd = string("{ cd ") + dir + " || mount " + dir + "; } 2>&1";

    _mountProcess = n_u::Process::spawn(cmd);
    string cmdout;
    istream& outst = _mountProcess.outStream();
    for (; !outst.eof();) {
        char cbuf[32];
        outst.read(cbuf,sizeof(cbuf)-1);
        cbuf[outst.gcount()] = 0;
        cmdout += cbuf;
    }

    n_u::trimString(cmdout);

    int status;
    _mountProcess.wait(true,&status);

    // check if automount worked
    if (isMounted()) {
        n_u::Logger::getInstance()->log(LOG_INFO,"%s is mounted",
            _dirMsg.c_str());
        return;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status))
        throw n_u::IOException(cmd,"failed",cmdout);
    throw n_u::IOException(cmd,"automount","failed");
}

void FsMount::mount(FileSet* fset)
{
    _fileset = fset;
    if (isMounted()) {
        _fileset->mounted();
	return;
    }
    cancel();		// cancel previous request if running
    _worker = new FsMountWorkerThread(this);
    _worker->start();	// start mounter thread
}

void FsMount::cancel()
{
    n_u::Synchronized autolock(_workerLock);
    if (_worker) {
	// since we have the workerLock and worker is non-null
	// the joiner will not delete the worker
	if (!_worker->isJoined()) {
	    if (_worker->isRunning()) {
		n_u::Logger::getInstance()->log(LOG_ERR,
		    "Cancelling previous mount of %s",
			getDevice().c_str());
		try {
		    _worker->cancel();
                }
		catch(const n_u::Exception& e) {
		    n_u::Logger::getInstance()->log(LOG_ERR,
			"cannot cancel mount of %s: %s",
			    getDevice().c_str(),e.what());
		}
                int status;
                try {
                    if (_mountProcess.getPid() > 0) {
                        _mountProcess.kill(SIGKILL);
                        pid_t pid = 0;
                        for (int i = 0; pid == 0 && i < 5; i++) {
                            usleep(USECS_PER_SEC / 10);
                            pid = _mountProcess.wait(false,&status);
                        }
                    }
                }
		catch(const n_u::Exception& e) {
		    n_u::Logger::getInstance()->log(LOG_ERR,
			"cannot kill mount of %s: %s",
			    getDevice().c_str(),e.what());
		}
                try {
                    if (_umountProcess.getPid() > 0) {
                        _umountProcess.kill(SIGKILL);
                        pid_t pid = 0;
                        for (int i = 0; pid == 0 && i < 5; i++) {
                            usleep(USECS_PER_SEC / 10);
                            pid = _umountProcess.wait(false,&status);
                        }
                    }
                }
		catch(const n_u::Exception& e) {
		    n_u::Logger::getInstance()->log(LOG_ERR,
			"cannot kill umount of %s: %s",
			    getDevice().c_str(),e.what());
		}
	    }
	    // worker run method starts the ThreadJoiner
	}
    }
}

void FsMount::finished()
{
    // cerr << "FsMount::finished" << endl;
    _workerLock.lock();
    _worker = 0;
    _workerLock.unlock();
    _fileset->mounted();
    // cerr << "FsMount::finished finished" << endl;
}

void FsMount::unmount()
       throw(n_u::IOException)
{
    if (!isMounted()) return;

    if (::umount(getDirExpanded().c_str()) == 0)
        if (errno != EPERM)
            throw n_u::IOException(string("umount ") + _dirMsg,
		"failed",errno);
    string cmd = string("umount") + ' ' + getDirExpanded() + " 2>&1";
    _umountProcess = n_u::Process::spawn(cmd);
    string cmdout;
    istream& outst = _umountProcess.outStream();
    for (; !outst.eof();) {
        char cbuf[32];
        outst.read(cbuf,sizeof(cbuf)-1);
        cbuf[outst.gcount()] = 0;
        cmdout += cbuf;
    }
    int status;
    _umountProcess.wait(true,&status);
    if (!WIFEXITED(status) || WEXITSTATUS(status))
            throw n_u::IOException(_dirMsg,"umount",cmdout);
}

bool FsMount::isMounted() {
    ifstream mfile("/proc/mounts");

    for (;;) {
	string mdev,mpt;
	mfile >> mdev >> mpt;
        // cerr << "mdev=" << mdev << " mpt=" << mpt << endl;
	mfile.ignore(1000,'\n');
	if (mfile.fail()) return false;
	if (mfile.eof()) return false;
	if (mpt == getDirExpanded()) return true;
    }
}

void FsMount::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    if(node->hasAttributes()) {
	// get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
	    if (aname == "dir") setDir(aval);
	    else if (aname == "dev") setDevice(aval);
	    else if (aname == "type") setType(aval);
	    else if (aname == "options") setOptions(aval);
	    else throw n_u::InvalidParameterException("mount",
			"unrecognized attribute", aname);
	}
    }
}

FsMountWorkerThread::FsMountWorkerThread(FsMount* fsmnt):
    n_u::Thread(string("mount:") + fsmnt->getDevice()),fsmount(fsmnt)
{
    blockSignal(SIGINT);
    blockSignal(SIGHUP);
    blockSignal(SIGTERM);
}

int FsMountWorkerThread::run() throw(n_u::Exception)
{
    int sleepsecs = 30;
    for (int i = 0;; i++) {
        if (isInterrupted()) break;
	try {
            // try "real" mount first, then autoMount
	    if (!(i % 2)) fsmount->mount();
            else fsmount->autoMount();
	    break;
	}
	catch(const n_u::IOException& e) {
	    if (e.getErrno() == EINTR) break;
            if (isInterrupted()) break;
            if ((i % 2)) {
                n_u::Logger::getInstance()->log(LOG_ERR,
                        "%s mount: %s, waiting %d secs to try again.",
                    getName().c_str(),e.what(),sleepsecs);
                struct timespec slp = { sleepsecs, 0};
                ::nanosleep(&slp,0);
            }
            else n_u::Logger::getInstance()->log(LOG_ERR,
                        "%s mount: %s",
                    getName().c_str(),e.what(),sleepsecs);
	}
    }
    fsmount->finished();
    n_u::ThreadJoiner* joiner = new n_u::ThreadJoiner(this);
    joiner->start();
    return RUN_OK;
}


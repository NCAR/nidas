/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/FsMount.h>
#include <nidas/core/FileSet.h>
#include <nidas/util/Logger.h>
#include <nidas/util/Process.h>
#include <nidas/util/util.h>

#include <fstream>
#include <unistd.h> // usleep()

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

FsMount::FsMount() :
    _dir(),_dirExpanded(),_dirMsg(),
    _device(),_deviceExpanded(),_deviceMsg(),
    _type(""),_options(),_fileset(0),
    _worker(0),_workerLock(),
    _mountProcess(),_umountProcess()
{
}

FsMount::FsMount(const FsMount& x): DOMable(),
    _dir(x._dir),_dirExpanded(),_dirMsg(),
    _device(x._device),_deviceExpanded(),_deviceMsg(),
    _type(x._type), _options(x._options),_fileset(0),
    _worker(0),_workerLock(),
    _mountProcess(),_umountProcess()
{}

FsMount& FsMount::operator=(const FsMount& rhs)
{
    if (&rhs != this) {
        *(DOMable*) this = rhs;
        setDevice(rhs.getDevice());
        setDir(rhs.getDir());
        _type = rhs._type;
        _options = rhs._options;
        _fileset = 0;
        _worker = 0;
    }
    return *this;
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
       throw(n_u::IOException)
{
    if (isMounted()) return;
    ILOG(("Mounting: %s at %s",_deviceMsg.c_str(),_dirMsg.c_str()));

    /* A mount can be done with either the libc mount() function
     * or the mount command.
     * advantages of mount command:
     *  1. allows non-root users to mount/unmount a filesystem if the
     *      "user" option is in the fstab.
     *  2. sys admin can put the appropriate options in /etc/fstab.
     *      Nidas config then only needs to know the mount point.
     */

    n_u::FileSet::createDirectory(getDirExpanded());

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
        throw n_u::IOException(cmd,cmdout);
    ILOG(("%s mounted at %s",_deviceMsg.c_str(),_dirMsg.c_str()));
}

/* Just issue a "cd /dir || mount /dir" command.
 * If /dir is automounted then it may work, whereas
 * "mount /dev/sdXn -o blahblah /dir"
 * will likely fail for a normal user on a server.
 */
void FsMount::autoMount()
       throw(n_u::IOException)
{
    if (isMounted()) return;
    ILOG(("Automounting: %s",_dirMsg.c_str()));

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
        ILOG(("%s is mounted",_dirMsg.c_str()));
        return;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status))
        throw n_u::IOException(cmd,cmdout);
    throw n_u::IOException(cmd,"failed");
    ILOG(("%s mounted",_dirMsg.c_str()));
}

/* asynchronous mount request. finished() method is called when done */
void FsMount::mount(FileSet* fset)
       throw(n_u::IOException)
{
    _fileset = fset;
    if (isMounted()) {
        _fileset->mounted();
	return;
    }
    cancel();		// cancel previous request if running
    n_u::Synchronized autolock(_workerLock);
    _worker = new FsMountWorkerThread(this);
    try {
        _worker->start();	// start mounter thread
    }
    catch(const n_u::Exception& e) {
        delete _worker;
        _worker = 0;
        throw n_u::IOException(fset->getName(),"thread",e.what());
    }
}

void FsMount::cancel()
{
    n_u::Synchronized autolock(_workerLock);
    if (_worker) {
	// since we have the workerLock and worker is non-null
	// the joiner will not delete the worker
	if (!_worker->isJoined()) {
	    if (_worker->isRunning()) {
#ifdef DEBUG
		DLOG(("cancelling previous mount of %s with SIGUSR1 signal",getDevice().c_str()));
#endif
		try {
		    _worker->kill(SIGUSR1);
                }
		catch(const n_u::Exception& e) {
		    PLOG(("cannot cancel mount of %s: %s",
			    getDevice().c_str(),e.what()));
		}
                int status;
                // kill any mount process
                try {
                    if (_mountProcess.getPid() > 0) {
                        _mountProcess.kill(SIGTERM);
                        pid_t pid = 0;
                        for (int i = 0; pid == 0 && i < 10; i++) {
                            if (i == 9) _mountProcess.kill(SIGKILL);
                            usleep(USECS_PER_SEC / 10);
                            pid = _mountProcess.wait(false,&status);
                        }
                    }
                }
		catch(const n_u::Exception& e) {
		    PLOG(("cannot kill mount of %s: %s",
			    getDevice().c_str(),e.what()));
		}
                // kill any unmount process
                try {
                    if (_umountProcess.getPid() > 0) {
                        _umountProcess.kill(SIGTERM);
                        pid_t pid = 0;
                        for (int i = 0; pid == 0 && i < 10; i++) {
                            if (i == 9) _umountProcess.kill(SIGKILL);
                            usleep(USECS_PER_SEC / 10);
                            pid = _umountProcess.wait(false,&status);
                        }
                    }
                }
		catch(const n_u::Exception& e) {
		    PLOG(("cannot kill umount of %s: %s",
			    getDevice().c_str(),e.what()));
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

    if (::umount(getDirExpanded().c_str()) == 0) return;

    // libc call failed, perhaps because of permission errors,
    // try umount command, which might succeed.

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
            throw n_u::IOException(_dirMsg,cmdout);
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
    unblockSignal(SIGUSR1);
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
            if (i == 0) PLOG(("%s mount: %s", getName().c_str(),e.what()));
            else {
                if (i < 2) PLOG(("%s mount: %s, trying every %d secs",
                    getName().c_str(),e.what(),sleepsecs));
                struct timespec slp = { sleepsecs, 0};
                ::nanosleep(&slp,0);
            }
	}
	catch(const n_u::Exception& e) {
            PLOG(("%s mount: %s", getName().c_str(),e.what()));
        }
    }
    fsmount->finished();
    n_u::ThreadJoiner* joiner = new n_u::ThreadJoiner(this);
    joiner->start();
    return RUN_OK;
}


/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-12-03 15:02:26 -0700 (Sat, 03 Dec 2005) $

    $LastChangedRevision: 3176 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/FsMount.cc $
 ********************************************************************

*/

#include <nidas/dynld/FsMount.h>
#include <nidas/dynld/FileSet.h>
#include <nidas/util/Logger.h>

#include <fstream>

using namespace nidas::dynld;
using namespace std;
using namespace xercesc;

namespace n_u = nidas::util;

FsMount::FsMount() : type("auto"),fileset(0),worker(0) {}

void FsMount::mount()
       throw(n_u::IOException)
{
    if (::mount(getDevice().c_str(),getDir().c_str(),
    	getType().c_str(),MS_NOATIME,getOptions().c_str()) < 0)
	    throw n_u::IOException(
	    string("mount ") + getDevice() + " -t " + getType() +
	    (getOptions().length() > 0 ?  string(" -o ") + getOptions() : "") +
	    ' ' + getDir(),"failed",errno);
}

void FsMount::mount(FileSet* fset)
{
    fileset = fset;
    if (isMounted()) {
        fileset->mounted();
	return;
    }
    cancel();		// cancel previous request if running
    worker = new FsMountWorkerThread(this);
    worker->start();	// start mounter thread
}

void FsMount::cancel()
{
    n_u::Synchronized autolock(workerLock);
    if (worker) {
	// since we have the workerLock and worker is non-null
	// the joiner will not delete the worker
	if (!worker->isJoined()) {
	    if (worker->isRunning()) {
		n_u::Logger::getInstance()->log(LOG_ERR,
		    "Cancelling previous mount of %s\n",
			getDevice().c_str());
		try {
		    worker->cancel();
		}
		catch(const n_u::Exception& e) {
		    n_u::Logger::getInstance()->log(LOG_ERR,
			"cannot cancel mount of %s: %s\n",
			    getDevice().c_str(),e.what());
		}
	    }
	    // worker run method starts the ThreadJoiner
	}
    }
}

void FsMount::finished()
{
    cerr << "FsMount::finished" << endl;
    workerLock.lock();
    worker = 0;
    workerLock.unlock();
    fileset->mounted();
    cerr << "FsMount::finished finished" << endl;
}

void FsMount::unmount()
       throw(n_u::IOException)
{
    if (!isMounted()) return;

    if (::umount(getDir().c_str()) < 0)
	throw n_u::IOException(string("umount ") + getDir(),
		"failed",errno);
}

bool FsMount::isMounted() {
    ifstream mfile("/proc/mounts");

    for (;;) {
	string mdev,mpt;
	mfile >> mdev >> mpt;
	mfile.ignore(1000,'\n');
	if (mfile.fail()) return false;
	if (mfile.eof()) return false;
	if (mpt == getDir()) return true;
    }
}

void FsMount::fromDOMElement(const DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    const string& elname = xnode.getNodeName();
    if(node->hasAttributes()) {
	// get all the attributes of the node
        DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
	    if (!aname.compare("dir")) setDir(aval);
	    else if (!aname.compare("dev")) setDevice(aval);
	    else if (!aname.compare("type")) setType(aval);
	    else if (!aname.compare("options")) setOptions(aval);
	    else throw n_u::InvalidParameterException("mount",
			"unrecognized attribute", aname);
	}
    }
}

DOMElement* FsMount::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("mount"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* FsMount::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

FsMountWorkerThread::FsMountWorkerThread(FsMount* fsmnt):
    n_u::Thread(string("mount:") + fsmnt->getDevice()),fsmount(fsmnt) {}

int FsMountWorkerThread::run() throw(n_u::Exception)
{
    int sleepsecs = 30;
    for (;;) {
	try {
	    fsmount->mount();
	    break;
	}
	catch(const n_u::IOException& e) {
	    if (e.getErrno() == EINTR) break;
	    n_u::Logger::getInstance()->log(LOG_ERR,
		    "%s mount failed: %s, waiting %d secs to try again.\n",
		getName().c_str(),e.what(),sleepsecs);
	}
	if (isInterrupted()) break;
	struct timespec slp = { sleepsecs, 0};
	::nanosleep(&slp,0);
	if (isInterrupted()) break;
    }
    fsmount->finished();
    n_u::ThreadJoiner* joiner = new n_u::ThreadJoiner(this);
    joiner->start();
    return RUN_OK;
}


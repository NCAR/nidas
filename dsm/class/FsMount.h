/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-10-10 10:11:54 -0600 (Mon, 10 Oct 2005) $

    $LastChangedRevision: 3042 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/FileSet.h $
 ********************************************************************

*/

#ifndef DSM_FSMOUNT_H
#define DSM_FSMOUNT_H

#include <atdUtil/Thread.h>
#include <atdUtil/IOException.h>

#include <DOMable.h>

#include <sys/mount.h>

#include <string>
#include <iostream>

namespace dsm {

class FileSet;
class FsMountWorkerThread;

/**
 * Filesystem mounter/unmounter.
 */
class FsMount : public DOMable {

public:

    FsMount();

    FsMount(const FsMount& x):
    	dir(x.dir),device(x.device),type(x.type),
	options(x.options),fileset(0),worker(0) {}

    void setDir(const std::string& val)
    {
        dir = val;
    }

    const std::string& getDir() const { return dir; }

    void setDevice(const std::string& val)
    {
        device = val;
    }

    const std::string& getDevice() const { return device; }

    void setType(const std::string& val)
    {
        type = val;
    }

    const std::string& getType() const { return type; }

    void setOptions(const std::string& val)
    {
        options = val;
    }

    const std::string& getOptions() const { return options; }

    /**
     * Reads /proc/mount to see if getDir() is mounted.
     */
    bool isMounted();

    /**
     * Synchronous mount request (on return the file system is mounted).
     */
    void mount() throw(atdUtil::IOException);

    /**
     * Asynchronous mount request. finished() method will
     * be called when mount is done.
     */
    void mount(FileSet*);

    void unmount() throw(atdUtil::IOException);

    void cancel();

    void finished();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException);

    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException);
    
protected:

    std::string dir;

    std::string device;

    std::string type;
 
    std::string options;

    FileSet* fileset;

    FsMountWorkerThread* worker;

    atdUtil::Mutex workerLock;

};

/**
 * Filesystem mounter/unmounter.
 */
class FsMountWorkerThread : public atdUtil::Thread
{
public:
    FsMountWorkerThread(FsMount* fsm);
    int run() throw(atdUtil::Exception);

private:
    FsMount* fsmount;
};

}

#endif

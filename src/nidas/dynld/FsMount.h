/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_FSMOUNT_H
#define NIDAS_DYNLD_FSMOUNT_H

#include <nidas/util/Thread.h>
#include <nidas/util/Process.h>
#include <nidas/util/IOException.h>

#include <nidas/core/DOMable.h>

#include <sys/mount.h>

#include <string>
#include <iostream>

namespace nidas { namespace dynld {

using namespace nidas::core;

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

    /**
     * Set the mount point directory. It may contain
     * environment variables, e.g.: $DATA, or ${DATA}.
     */
    void setDir(const std::string& val);

    const std::string& getDir() const { return dir; }

    /**
     * Get the mount point directory, with environment variables expanded.
     */
    const std::string& getDirExpanded() const { return dirExpanded; }

    void setDevice(const std::string& val);

    const std::string& getDevice() const { return device; }

    const std::string& getDeviceExpanded() const { return deviceExpanded; }

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
    void mount() throw(nidas::util::Exception);

    /**
     * Just issue a "mount /dir" command. If /dir is automounted
     * then it may work, whereas  "mount /dev/sdXn -o blahblah /dir"
     * may fail for the user on a server.
     */
    void autoMount() throw(nidas::util::Exception);

    /**
     * Asynchronous mount request. finished() method will
     * be called when mount is done.
     */
    void mount(FileSet*);

    void unmount() throw(nidas::util::IOException);

    void cancel();

    void finished();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

protected:

    std::string dir;

    std::string dirExpanded;

    std::string dirMsg;

    std::string device;

    std::string deviceExpanded;

    std::string deviceMsg;

    std::string type;
 
    std::string options;

    FileSet* fileset;

    FsMountWorkerThread* worker;

    nidas::util::Mutex workerLock;

    nidas::util::Process _mountProcess;

    nidas::util::Process _umountProcess;

};

/**
 * Filesystem mounter/unmounter.
 */
class FsMountWorkerThread : public nidas::util::Thread
{
public:
    FsMountWorkerThread(FsMount* fsm);
    int run() throw(nidas::util::Exception);

private:
    FsMount* fsmount;
};

}}	// namespace nidas namespace core

#endif

// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_FSMOUNT_H
#define NIDAS_CORE_FSMOUNT_H

#include <nidas/util/Thread.h>
#include <nidas/util/Process.h>
#include <nidas/util/IOException.h>

#include <nidas/core/DOMable.h>

#include <sys/mount.h>

#include <string>
#include <iostream>

namespace nidas { namespace core {

using namespace nidas::core;

class FileSet;
class FsMountWorkerThread;

/**
 * Filesystem mounter/unmounter.
 */
class FsMount : public DOMable {

public:

    FsMount();

    /**
     * Copy.
     */
    FsMount(const FsMount& x);

    ~FsMount() {}

    /**
     * Assignment.
     */
    FsMount& operator=(const FsMount& rhs);

    /**
     * Set the mount point directory. It may contain
     * environment variables, e.g.: $DATA, or ${DATA}.
     */
    void setDir(const std::string& val);

    const std::string& getDir() const { return _dir; }

    /**
     * Get the mount point directory, with environment variables expanded.
     */
    const std::string& getDirExpanded() const { return _dirExpanded; }

    void setDevice(const std::string& val);

    const std::string& getDevice() const { return _device; }

    const std::string& getDeviceExpanded() const { return _deviceExpanded; }

    void setType(const std::string& val)
    {
        _type = val;
    }

    const std::string& getType() const { return _type; }

    void setOptions(const std::string& val)
    {
        _options = val;
    }

    const std::string& getOptions() const { return _options; }

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
     * be called when mount is done. Does not own
     * the FileSet pointer.
     */
    void mount(FileSet*);

    void unmount() throw(nidas::util::IOException);

    void cancel();

    void finished();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

protected:

    std::string _dir;

    std::string _dirExpanded;

    std::string _dirMsg;

    std::string _device;

    std::string _deviceExpanded;

    std::string _deviceMsg;

    std::string _type;
 
    std::string _options;

    FileSet* _fileset;

    FsMountWorkerThread* _worker;

    nidas::util::Mutex _workerLock;

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

    /**
     * No copy.
     */
    FsMountWorkerThread(const FsMountWorkerThread&);

    /**
     * No assignment.
     */
    FsMountWorkerThread& operator=(const FsMountWorkerThread&);
};

}}	// namespace nidas namespace core

#endif

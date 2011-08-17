// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2010-06-17 15:24:30 -0600 (Thu, 17 Jun 2010) $

    $LastChangedRevision: 5575 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/DSMSerialSensor.cc $

 ******************************************************************
*/

#ifdef HAS_INOTIFY_H

#include <nidas/dynld/WatchedFileSensor.h>

#include <nidas/util/Logger.h>

#include <sys/inotify.h>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::core;
using namespace nidas::dynld;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(WatchedFileSensor)

WatchedFileSensor::WatchedFileSensor():_inotifyfd(-1),_watchd(-1)
{
    setDefaultMode(O_RDONLY);
}

WatchedFileSensor::~WatchedFileSensor()
{
}

void WatchedFileSensor::open(int flags)
    throw(n_u::IOException,n_u::InvalidParameterException)
{
    // CharacterSensor::open(flags);
    //     calls DSMSensor::open
    //         _iodev = buildIODevice()
    //         _iodev->setName(getDeviceName());
    //         NLOG
    //         _iodev->open(flags);
    //         if (!_scanner) _scanner = buildSampleScanner();
    //             _scanner->init();
    //         }

    _inotifyfd = inotify_init();
    if (_inotifyfd < 0) throw n_u::IOException(getDeviceName(),"inotify_init",errno);

    _watchd = inotify_add_watch(_inotifyfd,getDeviceName().c_str(),
        IN_MODIFY | IN_CLOSE_WRITE | IN_CREATE | IN_DELETE_SELF | IN_MOVE_SELF);
    if (_watchd < 0) throw n_u::IOException(getDeviceName(),"inotify_add_watch",errno);

    IODevice* iodev = buildIODevice();

    if (::access(getDeviceName().c_str(),F_OK) == 0) {
        // this could fail here if the file gets moved between the access check
        // and the open. We'll let it fail since NIDAS typically tries again.
        NLOG(("opening: ") << getDeviceName());
        iodev->open(flags);
        if (::lseek(iodev->getReadFd(),0,SEEK_END) < 0) 
            throw n_u::IOException(getDeviceName(),"lseek",errno);
    }

    SampleScanner* scanr = buildSampleScanner();
    setSampleScanner(scanr);
}

void WatchedFileSensor::close()
    throw(n_u::IOException)
{
    if (_inotifyfd >= 0) {
        int wd = _watchd;
        _watchd = -1;
        if (wd >= 0 && inotify_rm_watch(_inotifyfd,wd) < 0)
            throw n_u::IOException(getDeviceName(),"inotify_rm_watch",errno);
        int fd = _inotifyfd;
        _inotifyfd = -1;
        if (::close(fd) < 0)
            throw n_u::IOException(getDeviceName(),"close",errno);

    }
    // does the IODevice close
    DSMSensor::close();
}

IODevice* WatchedFileSensor::buildIODevice() throw(n_u::IOException)
{
    IODevice* iodev = new UnixIODevice();
    iodev->setName(getDeviceName());
    setIODevice(iodev);
    return iodev;
}

dsm_time_t WatchedFileSensor::readSamples() throw(n_u::IOException)
{
    // called when select/poll indicates something has happened
    // on the file I am monitoring.

    // We are not watching a directory, therefore the
    // inotify_event name file should be empty and we
    // don't need to allocate a buffer bigger than inotify_event.
    struct inotify_event event;

    ssize_t len = ::read(_inotifyfd,&event,sizeof(event));
    if (len < 0) throw n_u::IOException(getDeviceName(),"inotify read",errno);
    if (len < (signed) sizeof(event)) {
        WLOG(("") << getDeviceName() << "read(inotifyfd), len=" << len <<
                ", sizeof(event)=" << sizeof(event));
        return 0;
    }

    if (event.mask & IN_CLOSE_WRITE) {
        DLOG(("inotify: ") << getDeviceName() <<
                ": IN_CLOSE_WRITE");
        getIODevice()->close();
    }
    if (event.mask & IN_DELETE_SELF) {
        DLOG(("inotify: ") << getDeviceName() <<
                ": IN_DELETE_SELF");
        getIODevice()->close();
    }
    if (event.mask & IN_MOVE_SELF) {
        DLOG(("inotify: ") << getDeviceName() <<
                ": IN_MOVE_SELF");
        getIODevice()->close();
        NLOG(("opening: ") << getDeviceName());
        getIODevice()->open(getDefaultMode());
        if (::lseek(getIODevice()->getReadFd(),0,SEEK_END) < 0) 
            throw n_u::IOException(getDeviceName(),"lseek",errno);
    }
    if (event.mask & IN_CREATE) {
        DLOG(("inotify: ") << getDeviceName() << ": IN_CREATE");
        getIODevice()->close();
        NLOG(("opening: ") << getDeviceName());
        getIODevice()->open(getDefaultMode());
    }
    if (event.mask & IN_MODIFY) {
        DLOG(("inotify: ") << getDeviceName() << ": IN_MODIFY");
        if (getIODevice()->getReadFd() < 0) {
            NLOG(("opening after IN_MODIFY: ") << getDeviceName());
            getIODevice()->open(getDefaultMode());
        }
        return CharacterSensor::readSamples();
    }
    DLOG(("%s: mask=%x",getName().c_str(),event.mask));
    return 0;
}

#endif  // HAS_INOTIFY_H

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

WatchedFileSensor::WatchedFileSensor():_inotifyfd(-1),_watchd(-1),
    _events(IN_MODIFY | IN_DELETE_SELF | IN_ATTRIB | IN_MOVE_SELF),_nlinks(0)
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

    IODevice* iodev = buildIODevice();

    _inotifyfd = inotify_init();
    if (_inotifyfd < 0) throw n_u::IOException(getDeviceName(),"inotify_init",errno);

    _watchd = inotify_add_watch(_inotifyfd,getDeviceName().c_str(),_events);
    // will fail if file doesn't exist
    if (_watchd < 0) throw n_u::IOException(getDeviceName(),"inotify_add_watch",errno);

    NLOG(("opening: ") << getDeviceName());
    iodev->open(flags);
    if (::lseek(iodev->getReadFd(),0,SEEK_END) < 0) 
        throw n_u::IOException(getDeviceName(),"lseek",errno);

    _nlinks = getNLinks();

    SampleScanner* scanr = buildSampleScanner();
    setSampleScanner(scanr);
}

nlink_t WatchedFileSensor::getNLinks() throw(n_u::IOException)
{
    struct stat statbuf;
    if (::fstat(getIODevice()->getReadFd(),&statbuf) < 0)
        throw n_u::IOException(getDeviceName(),"fstat",errno);
    return statbuf.st_nlink;
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
    DSMSensor::close();
    _nlinks = 0;
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
    //
    // When this method returns, things should be in one of two states:
    // 1. If file exists, it should be opened, and being watched.
    // 1. If file doesn't exist, throw an exception.

    // We are not watching a directory, therefore the
    // inotify_event file name field should be empty and we
    // don't need to allocate a buffer bigger than inotify_event.
    struct inotify_event event;

    ssize_t len = ::read(_inotifyfd,&event,sizeof(event));
    if (len < 0) throw n_u::IOException(getDeviceName(),"inotify read",errno);
    if (len < (signed) sizeof(event)) {
        WLOG(("") << getDeviceName() << "read(inotifyfd), len=" << len <<
                ", sizeof(event)=" << sizeof(event));
        return 0;
    }

    /*
     * inotify watches inodes, not pathnames.
     *
     * If a file doesn't exist, inotify_add_watch fails.
     *
     * file exists, only one link to inode
     * writer modifies  IN_MODIFY
     * writer closes    IN_CLOSE_WRITE
     * file deleted     IN_DELETE_SELF
     *
     * file exists, one of multiple links to the same inode
     * writer modifies  IN_MODIFY
     * writer closes    IN_CLOSE_WRITE
     * file deleted     IN_ATTRIB, because link count changed (linux 2.6.25)
     *                  You don't get IN_DELETE_SELF because inode still exists
     *
     * ntp file scenario
     * loopstats is a hard link to loopstats.20110528
     * ntp closes loopstats.20110528    IN_CLOSE_WRITE
     * ntp opens loopstats.20110529
     * ntp deletes loopstats            IN_ATTRIB, link count changed
     * loopstats is linked to loopstats.20110529
     */

    if (event.mask & IN_DELETE_SELF) {
        ILOG(("inotify: ") << getDeviceName() << ": IN_DELETE_SELF");
        getIODevice()->close();
        _watchd = inotify_add_watch(_inotifyfd,getDeviceName().c_str(),_events);
        if (_watchd < 0) throw n_u::IOException(getDeviceName(),"inotify_add_watch",errno);
    }
    if (event.mask & IN_MOVE_SELF) {
        ILOG(("inotify: ") << getDeviceName() << ": IN_MOVE_SELF");
        getIODevice()->close();
        _watchd = inotify_add_watch(_inotifyfd,getDeviceName().c_str(),_events);
        if (_watchd < 0) throw n_u::IOException(getDeviceName(),"inotify_add_watch",errno);
    }
    if (event.mask & IN_ATTRIB) {
        ILOG(("inotify: ") << getDeviceName() << ": IN_ATTRIB");
        nlink_t nlinks = getNLinks();
        ILOG(("%s: nlinks, prev=%d, now=%d",getDeviceName().c_str(),_nlinks,nlinks));
        if (nlinks < _nlinks) {
            // our pathname may have been unlinked, so watch again.
            _watchd = inotify_add_watch(_inotifyfd,getDeviceName().c_str(),_events);
            if (_watchd < 0) throw n_u::IOException(getDeviceName(),"inotify_add_watch",errno);
        }
        _nlinks = nlinks;
    }
    if (event.mask & IN_MODIFY) return CharacterSensor::readSamples();
    return 0;
}

#endif  // HAS_INOTIFY_H

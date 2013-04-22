// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

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

WatchedFileSensor::WatchedFileSensor():
    _inotifyfd(-1),_watchd(-1),
    _events(IN_MODIFY | IN_DELETE_SELF | IN_ATTRIB | IN_MOVE_SELF | IN_CLOSE_WRITE),
    _iodev(0),_flags(O_RDONLY),_dev(0),_inode(0)
{
    setDefaultMode(O_RDONLY);
}

WatchedFileSensor::~WatchedFileSensor()
{
}

void WatchedFileSensor::open(int flags)
    throw(n_u::IOException,n_u::InvalidParameterException)
{
    _flags = flags;

    _inotifyfd = inotify_init();
    if (_inotifyfd < 0) throw n_u::IOException(getDeviceName(),"inotify_init",errno);

    _watchd = inotify_add_watch(_inotifyfd,getDeviceName().c_str(),_events);
    // will fail if file doesn't exist
    if (_watchd < 0) throw n_u::IOException(getDeviceName(),"inotify_add_watch",errno);

    CharacterSensor::open(flags);
    _iodev = getIODevice();

    getFileStatus();
    if (::lseek(_iodev->getReadFd(),0,SEEK_END) < 0) 
        throw n_u::IOException(getDeviceName(),"lseek",errno);
}

void WatchedFileSensor::close()
    throw(n_u::IOException)
{
    if (_inotifyfd >= 0) {
        int wd = _watchd;
        _watchd = -1;
        if (wd >= 0 && inotify_rm_watch(_inotifyfd,wd) < 0)
            WLOG(("") << n_u::IOException(getDeviceName(),"inotify_rm_watch on close",errno).toString());
        int fd = _inotifyfd;
        _inotifyfd = -1;
        if (::close(fd) < 0)
            throw n_u::IOException(getDeviceName(),"close",errno);

    }
    CharacterSensor::close();
}

void WatchedFileSensor::reopen() throw(n_u::IOException)
{
    _iodev->close();

    if (_watchd >= 0 && inotify_rm_watch(_inotifyfd,_watchd)) {
        _watchd = -1;
        WLOG(("") << n_u::IOException(getDeviceName(),"inotify_rm_watch",errno).toString());
    }

    _watchd = inotify_add_watch(_inotifyfd,getDeviceName().c_str(),_events);
    if (_watchd < 0) throw n_u::IOException(getDeviceName(),"inotify_add_watch",errno);

    NLOG(("opening: ") << getDeviceName());

    _iodev->open(_flags);
    getFileStatus();
    if (::lseek(_iodev->getReadFd(),0,SEEK_END) < 0) 
        throw n_u::IOException(getDeviceName(),"lseek",errno);
}

void WatchedFileSensor::getFileStatus() throw(n_u::IOException)
{
    struct stat statbuf;
    if (::fstat(_iodev->getReadFd(),&statbuf) < 0)
        throw n_u::IOException(getDeviceName(),"fstat",errno);
    _dev = statbuf.st_dev;
    _inode = statbuf.st_ino;
}

void WatchedFileSensor::getPathStatus(dev_t& dev, ino_t& inode) throw(n_u::IOException)
{
    struct stat statbuf;
    if (::stat(getDeviceName().c_str(),&statbuf) < 0)
        throw n_u::IOException(getDeviceName(),"stat",errno);
    dev = statbuf.st_dev;
    inode = statbuf.st_ino;
}

bool WatchedFileSensor::differentInode() throw(n_u::IOException)
{
    dev_t dev;
    ino_t inode;
    getPathStatus(dev,inode);

    // If device ID or inode value has changed, then
    // the getDeviceName() points to a new inode.
    if (dev != _dev || inode != _inode) return true;
    return false;
}

bool WatchedFileSensor::readSamples() throw(n_u::IOException)
{
    bool exhausted = false;
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
        // shouldn't happen
        WLOG(("") << getDeviceName() << "read(inotifyfd), len=" << len <<
                ", sizeof(event)=" << sizeof(event));
        return exhausted;
    }

    /*
     * We're doing an inotify watch of a file, not a directory. It it
     * important to understand that inotify watches the inode
     * that the pathname pointed to when the watch was created.
     * If you don't detect when the pathname has been
     * relinked to a different inode, you'll keep watching the
     * original inode, which is probably not what you want.
     * To do this you must watch IN_ATTRIB.
     *
     * You won't get an IN_DELETE_SELF until the last link to that
     * inode is gone.
     *
     * If an inode for a pathname doesn't exist, inotify_add_watch() fails,
     * and, somewhat mysteriously, so does inotify_rm_watch().
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
     * Files can be rotated in various ways. Here are the ways I've thought of.
     *
     * ntp stats files (e.g. loopstats), as of version 4.2.6:
     *   pseudo-code                                inotify event, watching "loopstats"
     *   unlink("loopstats")                        IN_ATTRIB: # of links changed.
     *                                              Code get an IOException trying
     *                                              checking for the "loopstats" inode.
     *   fdnew=creat("loopstats.20110829")          No event.
     *      create next day's file, empty
     *   close(fd), loopstats.20110828              Probably no event since we're not
     *                                              watching the file after it was
     *                                              deleted.
     *   fd = fdnew
     *   link("loopstats.20110829","loopstats")     IN_ATTRIB: number of links 
     *                                              to loopstats.20110828 reduced by 1.
     *                                              Inode of "loopstats" changed.
     *                                              Renew watch of "loopstats".
     *
     * A "smarter" ntp could use the atomic rename system function so that
     * the "loopstats" file always exists:
     *   pseudo-code                                inotify event, watching "loopstats"
     *   fdnew=creat("loopstats.20110829")          No event.
     *      create next day's file, empty
     *   link("loopstats.20110829","tmp")           No event.
     *      make a second hard link to new file
     *   rename("tmp","loopstats")                  IN_ATTRIB: number of links 
     *      atomic relink                           to loopstats.20110828 reduced by 1.
     *                                              Inode of "loopstats" changed.
     *                                              Renew watch of "loopstats".
     *   close(fd), loopstats.20110828              No event since we're watching
     *      close prev day's file                   new "loopstats".
     *   fd = fdnew
     *
     *
     * logrotate create, the default handling in RedHat /etc/logrotate.conf:
     *  pseudo-code                                 inotify event, watching "messages"
     *   rename("messages","messages.0")            No event since 
     *                                              messages.0 didn't exist, and
     *                                              inode still has 1 link.
     *   open("messages",O_CREAT|O_TRUNC)           No event, since we're
     *                                              still watching the previous
     *                                              inode.
     *   HUP to syslog daemon                        
     *   syslog: close(fd)                          IN_CLOSE_WRITE of orig inode.
     *                                              Check if inode of "messages"
     *                                              has changed. Renew watch of
     *                                              "messages"
     *   syslog: open("messages")
     *
     * logrotate nocreate
     *  pseudo-code                                 inotify event, watching "messages"
     *   open("messages",O_CREAT|O_TRUNC)           no inotify event, since we're
     *                                              still watching the previous
     *                                              inode
     *   HUP to syslog daemon                        
     *   syslog: close(fd)                          IN_CLOSE_WRITE of orig inode
     *                                              check that inode of "messages"
     *                                              has changed. renew watch of
     *                                              "messages"
     *   syslog: open("messages")
     *                                              
     * logrotate copy (logrotate makes a copy, old file keeps growing)
     *   copy("messages","message.0")
     *   Inotify won't need to see any events since the original inode is
     *   still being written to.
     *
     * logrotate copytruncate
     *    copy("messages","message.0")              no event
     *    truncate("messages")                      IN_MODIFY (read will get EOF)
     */
    
    if (event.mask & IN_DELETE_SELF) {      // last link to inode removed
        ILOG(("inotify: ") << getDeviceName() << ": IN_DELETE_SELF");
        reopen();
    }
    if (event.mask & IN_MOVE_SELF) {        // only link to inode renamed
        ILOG(("inotify: ") << getDeviceName() << ": IN_MOVE_SELF");
        reopen();
    }
    if (event.mask & IN_CLOSE_WRITE) {      // a close on the inode
        ILOG(("inotify: ") << getDeviceName() << ": IN_CLOSE_WRITE");
        if (differentInode()) reopen();
    }
    if (event.mask & IN_ATTRIB) {           // perhaps a change in the number of links
        ILOG(("inotify: ") << getDeviceName() << ": IN_ATTRIB");
        if (differentInode()) reopen();
    }
    if (event.mask & IN_MODIFY) {
        try {
            exhausted = CharacterSensor::readSamples();
        }
        catch(const n_u::EOFException& e) {
            // file appears to have been truncated, so seek to the
            // new end of file.
            NLOG(("%s: EOF, doing lseek to end",getDeviceName().c_str()));
            if (::lseek(_iodev->getReadFd(),0,SEEK_END) < 0) 
                throw n_u::IOException(getDeviceName(),"lseek",errno);
        }
    }
    return exhausted;
}

#endif  // HAS_INOTIFY_H

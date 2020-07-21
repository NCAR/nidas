// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include <nidas/Config.h>

#ifdef HAVE_SYS_INOTIFY_H

#include "WatchedFileSensor.h"

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

    // do non-blocking reads of all available events
    if (::fcntl(_inotifyfd, F_SETFL, O_NONBLOCK))
        throw n_u::IOException(getDeviceName(),"fcntl(, F_SETFL, O_NONBLOCK)",errno);

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
    // called when select/poll indicates something has happened
    // on the file I am monitoring.
    //
    // Use non-blocking I/O on _inotifyfd and read all available events.
    // So both edge-triggered and level-triggered polling should work
    // fine on _inotifyfd, no matter whether the return value of this
    // function (exhausted) is true or false (where for edge-triggered
    // polling a return of true means repeat reads on the file descriptor).
    // Therefore the return value will indicate the status of the read of the underlying
    // file, and so warnings in the SensorHandler about "full buffer reads" will apply
    // to the file reads, which is good.
    // If edge-triggering is used, and a full buffer read was done on the file
    // (exhausted=false,meaning there is probably more to be read) an extra,
    // perhaps unnecessary, read will done on the inotify fd,
    // but that shouldn't be an issue.  The file won't be read again until a subsequent
    // inotify event happens on it, which isn't ideal.  So "full buffer read" warnings
    // are an indication that the size of the buffer should be expanded, especially
    // in the case of edge-triggered polling in SensorHandler.
    //
    // When this method returns, things should be in one of two states:
    // 1. If the file exists, it should be opened, and being watched.
    // 1. If the file doesn't exist, throw an exception.

    // We are not watching a directory, therefore the
    // inotify_event file name field is be empty and we
    // don't need to allocate buffer space for the file name.
    struct inotify_event event;

    bool exhausted = false;

    for (;;) {

        ssize_t len = ::read(_inotifyfd,&event,sizeof(event));
        if (len < 0) {
            // inotify fd is O_NONBLOCK, so read until EAGAIN to get all events
            if (errno == EAGAIN || EWOULDBLOCK) {
                exhausted = true;
                break;
            }
            throw n_u::IOException(getDeviceName(),"inotify read",errno);
        }

        if (len < (signed) sizeof(event)) {
            // shouldn't happen
            WLOG(("") << getDeviceName() << "read(inotifyfd), len=" << len <<
                    ", sizeof(event)=" << sizeof(event));
            break;
        }

        // if event from old watch
        if (event.wd != _watchd) continue;

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
         * If you're watching file.dat and someone does:
         *    cat "99" >> file.dat
         * This will generally generate two events, IN_MODIFY and
         * IN_CLOSE_WRITE, rather than one event with both bits set.
         * When a poll detects activity on inotify fd, there may be
         * one or two events available to be read, depending on how
         * quick the poll responds to the inotify activity.
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
         *   pseudo-code                                inotify watching "loopstats"
         *   unlink("loopstats")                        IN_ATTRIB: # of links changed.
         *                                              Will get an IOException checking for the
         *                                              "loopstats" inode. Nidas will schedule the
         *                                              file (and the inotify fd) to be reopened.
         *
         *   fdnew=creat("loopstats.20110829")          No event.
         *      create next day's file, empty
         *   close(fd), loopstats.20110828              Probably no event since we're not
         *                                              watching the file after it was
         *                                              deleted.
         *   fd = fdnew
         *   link("loopstats.20110829","loopstats")     Eventually nidas should see the new "loopstats" file.
         *                                              and start watching it.
         *
         * A "smarter" ntp could use the atomic rename system function so that
         * the "loopstats" file always exists:
         *   pseudo-code                                inotify watching "loopstats"
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
         *  pseudo-code                                 inotify watching "messages"
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
         *  pseudo-code                                 inotify watching "messages"
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
         * logrotate copytruncate
         *    copy("messages","message.0")              no event
         *    truncate("messages")                      IN_MODIFY (read will get EOF)
         *
         *  To read a value from /sys/devices, such as CPU temperature
         *
         *  script                                      Nidas and inotify
         *  #!/bin/sh
         *  in=/sys/devices/platform/.../temp1_input    Nidas loops trying to open cpu_temp.txt
         *  out=/tmp/cpu_temp.txt                       Once it sees cpu_temp.txt, starts watching it,
         *  old=/tmp/cpu_temp.old                       and seeks to end of file.
         *  rm -f $out
         *  while true; do
         *      i=0
         *      while [ $((i=i+1)) -le 1000 ]; do
         *         cat $in >> $out                      inotify events: IN_MODIFY, IN_CLOSE_WRITE 
         *         sleep 5                                 reads data, keeps file open.
         *      done                                    
         *      mv $out $old                            IN_MOVE_SELF, then IOException trying to reopen
         *      cat /dev/null > $out                    cpu_temp.txt. Schedules it to be reopened.
         #                                              Once it is reopened a seek is done to the end.
         # done                                         Since the sensor opener loop doesn't run fast,
         *                                              maybe once every 10 seconds, the seek may
         *                                              skip some record.
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
    }
    return exhausted;
}

#endif  // HAVE_SYS_INOTIFY_H

// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
    Copyright 2011 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-11-11 11:57:05 -0700 (Wed, 11 Nov 2009) $

    $LastChangedRevision: 5083 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/DSMSerialSensor.h $

 ******************************************************************
*/

#ifdef HAS_INOTIFY_H
/* From inotify man page:
 * Inotify  was  merged into the 2.6.13 Linux kernel.  The required library inter‐
 * faces were added to glibc in version 2.4.   (IN_DONT_FOLLOW,  IN_MASK_ADD,  and
 * IN_ONLYDIR were only added in version 2.5.)
 * As of Aug 2011, we're using glibc 2.3.3 on the Vipers and Vulcans
 *      cat /opt/arcom/arm-linux/lib/libc.so
 *      ls -l /opt/arcom/arm-linux/lib/libc.so.6 ->  libc-2.3.3.so
 * So, there is no inotify on the Vipers/Vulcans until they are upgraded.
 * HAS_INOTIFY_H will be defined by Scons if sys/inotify.h is found.
 */

#ifndef NIDAS_DYNLD_WATCHEDFILESENSOR_H
#define NIDAS_DYNLD_WATCHEDFILESENSOR_H

#include <nidas/core/CharacterSensor.h>
#include <nidas/core/UnixIODevice.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * NIDAS sensor input from a file.  The device name is the
 * name of the file.  The state of the file is monitored
 * with the inotify system call after call to the open() method.
 * The sensor file descriptor (as returned by getReadFd()) is the
 * file descriptor returned by inotify_init, and can
 * then be monitored with select/poll, as for any DSMSensor.
 * When the system detects that the file has been modified,
 * then a select/poll on that file descriptor
 * will return indicating a change of file status, and the
 * readSamples() method should be called as for any sensor.
 * The WatchedFileSensor::readSamples() method reads from the inotify
 * file descriptor to determine what inotify events have occured,
 * and if the file has been modified, then samples are read from the file.
 *
 * The file does not have to exist when the open() method is called.
 * When inotify indicates that the file has been created it will
 * be opened in readSamples().  When the file is opened, an
 * lseek is done to position the read pointer to the current end of file.
 *
 * This class was originally written to ingest data from the NTP
 * loopstats and peerstats files as they are written by ntpd,
 * but it should be useful for other files.
 */
class WatchedFileSensor : public CharacterSensor
{

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    WatchedFileSensor();

    ~WatchedFileSensor();

    /**
     * Override getReadFd() so that it returns the inotify file
     * descriptor.
     */
    int getReadFd() const
    {
        return _inotifyfd;
    }

    int getWriteFd() const
    {
        return -1;
    }

    /**
     * Start monitoring the file.
     */
    void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    /*
     * Stop monitoring the file.
     */
    void close() throw(nidas::util::IOException);

    /**
     * Called by SensorHandler when there is activity on
     * my read file descriptor, indicating that inotify
     * has detected a watched event on the file.
     */
    dsm_time_t readSamples() throw(nidas::util::IOException);

private:

    /**
     * Set the device id and inode from the opened file descriptor.
     */
    void getFileStatus() throw(nidas::util::IOException);

    /**
     * Fetch the device id and inode of the file with name getDeviceName(),
     * which because a process may be renaming files, may not be the inode
     * that is currently opened.
     *
     */
    void getPathStatus(dev_t& dev, ino_t& inode) throw(nidas::util::IOException);

    /**
     * Does file with name getDeviceName() point to a different inode than
     * the currently opened file?
     */
    bool differentInode() throw(nidas::util::IOException);

    /**
     * Close the currently opened file, start a new watch
     * of the file named getDeviceName(), open it, and
     * seek to the end. This is called when, due to
     * file renames, the inode of the file named getDeviceName() is
     * not the inode which is currently opened.
     */
    void reopen() throw(nidas::util::IOException);

    /**
     * File descriptor returned from inotify_init in the open method.
     */
    int _inotifyfd;

    /**
     * inotify watch descriptor.
     */
    int _watchd;

    /**
     * Inotify events to watch for.
     */
    uint32_t _events;

    /**
     * IODevice for accessing the actual file.
     */
    IODevice* _iodev;

    /**
     * Flags for open;
     */
    int _flags;

    /**
     * The ID of the device containing the currently opened file.
     * See man page of fstat(2).
     */
    dev_t _dev;

    /**
     * The inode number of the currently opened file.
     * See man page of fstat(2).
     */
    ino_t _inode;

};

}}	// namespace nidas namespace dynld

#endif
#endif  // HAS_INOTIFY_H
